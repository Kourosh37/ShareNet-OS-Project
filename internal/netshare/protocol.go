package netshare

import (
	"bufio"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"
	"unicode/utf8"
)

const maxFileNameRunes = 180
const transferBufferSize = 64 * 1024
const progressInterval = 100 * time.Millisecond
const finalResponseTimeout = 2 * time.Minute

const (
	TypeRequestList     = "REQUEST_LIST"
	TypeListLegacy      = "list"
	TypeUploadStart     = "UPLOAD_START"
	TypeUploadLegacy    = "upload"
	TypeDownloadRequest = "DOWNLOAD_REQUEST"
	TypeDownloadLegacy  = "download"
	TypeDeleteRequest   = "DELETE_REQUEST"
	TypeDeleteLegacy    = "delete"
)

type FileInfo struct {
	Name    string `json:"name"`
	Size    int64  `json:"size"`
	ModTime string `json:"mod_time,omitempty"`
	SHA256  string `json:"sha256,omitempty"`
}

type request struct {
	Type string `json:"type"`
	Name string `json:"name,omitempty"`
	Size int64  `json:"size,omitempty"`
}

type response struct {
	OK    bool       `json:"ok"`
	Error string     `json:"error,omitempty"`
	Files []FileInfo `json:"files,omitempty"`
	Size  int64      `json:"size,omitempty"`
}

type ProgressFunc func(done, total int64)

func cleanName(name string) (string, error) {
	name = strings.ToValidUTF8(strings.TrimSpace(name), "")
	name = filepath.Base(name)
	if name == "." || name == "" {
		return "", errors.New("empty filename")
	}
	if utf8.RuneCountInString(name) > maxFileNameRunes {
		return "", errors.New("filename is too long")
	}
	if strings.ContainsAny(name, `/\<>:"|?*`) {
		return "", errors.New("invalid filename")
	}
	for _, r := range name {
		if r == 0 || r < 32 {
			return "", errors.New("invalid filename")
		}
	}
	return name, nil
}

func writeJSONLine(w io.Writer, v any) error {
	data, err := json.Marshal(v)
	if err != nil {
		return err
	}
	data = append(data, '\n')
	_, err = w.Write(data)
	return err
}

func readJSONLine(r *bufio.Reader, v any) error {
	line, err := r.ReadBytes('\n')
	if err != nil {
		return err
	}
	return json.Unmarshal(line, v)
}

func copyWithProgress(dst io.Writer, src io.Reader, total int64, progress ProgressFunc) error {
	buf := make([]byte, transferBufferSize)
	var done int64
	lastProgress := time.Now()
	if progress != nil {
		progress(0, total)
	}
	for {
		n, readErr := src.Read(buf)
		if n > 0 {
			written, writeErr := dst.Write(buf[:n])
			done += int64(written)
			if progress != nil && (done == total || time.Since(lastProgress) >= progressInterval) {
				progress(done, total)
				lastProgress = time.Now()
			}
			if writeErr != nil {
				return writeErr
			}
			if written != n {
				return io.ErrShortWrite
			}
		}
		if readErr == io.EOF {
			return nil
		}
		if readErr != nil {
			return readErr
		}
	}
}

func ListLocalFiles(dir string) ([]FileInfo, error) {
	entries, err := os.ReadDir(dir)
	if err != nil {
		return nil, err
	}
	files := make([]FileInfo, 0, len(entries))
	for _, entry := range entries {
		if entry.IsDir() {
			continue
		}
		info, err := entry.Info()
		if err != nil {
			continue
		}
		if strings.HasPrefix(entry.Name(), ".upload-") || strings.HasSuffix(entry.Name(), ".tmp") {
			continue
		}
		files = append(files, FileInfo{
			Name:    entry.Name(),
			Size:    info.Size(),
			ModTime: info.ModTime().Format("2006-01-02 15:04:05"),
		})
	}
	sort.Slice(files, func(i, j int) bool {
		return strings.ToLower(files[i].Name) < strings.ToLower(files[j].Name)
	})
	return files, nil
}

func dial(addr string) (net.Conn, *bufio.Reader, error) {
	dialer := net.Dialer{
		Timeout:   10 * time.Second,
		KeepAlive: 30 * time.Second,
	}
	conn, err := dialer.Dial("tcp", addr)
	if err != nil {
		return nil, nil, err
	}
	return conn, bufio.NewReader(conn), nil
}

func List(addr string) ([]FileInfo, error) {
	conn, r, err := dial(addr)
	if err != nil {
		return nil, err
	}
	defer conn.Close()
	if err := writeJSONLine(conn, request{Type: TypeRequestList}); err != nil {
		return nil, err
	}
	var resp response
	if err := readJSONLine(r, &resp); err != nil {
		return nil, err
	}
	if !resp.OK {
		return nil, errors.New(resp.Error)
	}
	return resp.Files, nil
}

func Upload(addr, path string, progress ProgressFunc) error {
	file, err := os.Open(path)
	if err != nil {
		return err
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		return err
	}
	name, err := cleanName(info.Name())
	if err != nil {
		return err
	}
	conn, r, err := dial(addr)
	if err != nil {
		return err
	}
	defer conn.Close()
	if info.Size() < 0 {
		return errors.New("invalid file size")
	}
	if err := writeJSONLine(conn, request{Type: TypeUploadStart, Name: name, Size: info.Size()}); err != nil {
		return err
	}
	var prep response
	if err := readJSONLine(r, &prep); err != nil {
		return fmt.Errorf("upload was rejected before transfer: %w", err)
	}
	if !prep.OK {
		return errors.New(prep.Error)
	}
	if err := copyWithProgress(conn, file, info.Size(), progress); err != nil {
		return fmt.Errorf("upload transfer failed: %w", err)
	}
	if tcpConn, ok := conn.(*net.TCPConn); ok {
		_ = tcpConn.CloseWrite()
	}
	_ = conn.SetReadDeadline(time.Now().Add(finalResponseTimeout))
	var resp response
	if err := readJSONLine(r, &resp); err != nil {
		return fmt.Errorf("upload finished but server response was lost: %w", err)
	}
	_ = conn.SetReadDeadline(time.Time{})
	if !resp.OK {
		return errors.New(resp.Error)
	}
	return nil
}

func Download(addr, name, dir string, progress ProgressFunc) (string, error) {
	clean, err := cleanName(name)
	if err != nil {
		return "", err
	}
	if err := os.MkdirAll(dir, 0755); err != nil {
		return "", err
	}
	conn, r, err := dial(addr)
	if err != nil {
		return "", err
	}
	defer conn.Close()
	if err := writeJSONLine(conn, request{Type: TypeDownloadRequest, Name: clean}); err != nil {
		return "", err
	}
	var resp response
	if err := readJSONLine(r, &resp); err != nil {
		return "", err
	}
	if !resp.OK {
		return "", errors.New(resp.Error)
	}
	outPath := filepath.Join(dir, clean)
	out, err := os.Create(outPath)
	if err != nil {
		return "", err
	}
	defer out.Close()
	if err := copyWithProgress(out, io.LimitReader(r, resp.Size), resp.Size, progress); err != nil {
		return "", err
	}
	return outPath, nil
}

func Delete(addr, name string) error {
	clean, err := cleanName(name)
	if err != nil {
		return err
	}
	conn, r, err := dial(addr)
	if err != nil {
		return err
	}
	defer conn.Close()
	if err := writeJSONLine(conn, request{Type: TypeDeleteRequest, Name: clean}); err != nil {
		return err
	}
	var resp response
	if err := readJSONLine(r, &resp); err != nil {
		return err
	}
	if !resp.OK {
		return errors.New(resp.Error)
	}
	return nil
}

func SHA256File(path string) (string, error) {
	file, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer file.Close()
	hash := sha256.New()
	if _, err := io.Copy(hash, file); err != nil {
		return "", err
	}
	return hex.EncodeToString(hash.Sum(nil)), nil
}

func FormatSize(size int64) string {
	const unit = 1024
	if size < unit {
		return fmt.Sprintf("%d B", size)
	}
	div, exp := int64(unit), 0
	for n := size / unit; n >= unit; n /= unit {
		div *= unit
		exp++
	}
	return fmt.Sprintf("%.1f %ciB", float64(size)/float64(div), "KMGTPE"[exp])
}
