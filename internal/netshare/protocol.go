package netshare

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"path/filepath"
	"sort"
	"strings"
)

const maxFileNameLen = 180

type FileInfo struct {
	Name string `json:"name"`
	Size int64  `json:"size"`
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
	name = filepath.Base(strings.TrimSpace(name))
	if name == "." || name == "" {
		return "", errors.New("empty filename")
	}
	if len(name) > maxFileNameLen {
		return "", errors.New("filename is too long")
	}
	if strings.ContainsAny(name, `/\`) {
		return "", errors.New("invalid filename")
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
	buf := make([]byte, 64*1024)
	var done int64
	if progress != nil {
		progress(0, total)
	}
	for {
		n, readErr := src.Read(buf)
		if n > 0 {
			written, writeErr := dst.Write(buf[:n])
			done += int64(written)
			if progress != nil {
				progress(done, total)
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
		files = append(files, FileInfo{Name: entry.Name(), Size: info.Size()})
	}
	sort.Slice(files, func(i, j int) bool {
		return strings.ToLower(files[i].Name) < strings.ToLower(files[j].Name)
	})
	return files, nil
}

func dial(addr string) (net.Conn, *bufio.Reader, error) {
	conn, err := net.Dial("tcp", addr)
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
	if err := writeJSONLine(conn, request{Type: "list"}); err != nil {
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
	if err := writeJSONLine(conn, request{Type: "upload", Name: name, Size: info.Size()}); err != nil {
		return err
	}
	if err := copyWithProgress(conn, file, info.Size(), progress); err != nil {
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
	if err := writeJSONLine(conn, request{Type: "download", Name: clean}); err != nil {
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
	if err := writeJSONLine(conn, request{Type: "delete", Name: clean}); err != nil {
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
