package netshare

import (
	"bufio"
	"errors"
	"fmt"
	"io"
	"net"
	"os"
	"path/filepath"
	"sync"
	"time"
)

const uploadIdleTimeout = 5 * time.Minute

type Server struct {
	Dir      string
	Log      func(string)
	listener net.Listener
	mu       sync.Mutex
	locksMu  sync.Mutex
	locks    map[string]*sync.RWMutex
}

func (s *Server) Start(addr string) error {
	if err := os.MkdirAll(s.Dir, 0755); err != nil {
		return err
	}
	ln, err := net.Listen("tcp", addr)
	if err != nil {
		return err
	}
	s.mu.Lock()
	s.listener = ln
	s.mu.Unlock()
	s.log("listening on " + addr)
	go s.acceptLoop(ln)
	return nil
}

func (s *Server) Stop() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.listener == nil {
		return nil
	}
	err := s.listener.Close()
	s.listener = nil
	return err
}

func (s *Server) Running() bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.listener != nil
}

func (s *Server) acceptLoop(ln net.Listener) {
	for {
		conn, err := ln.Accept()
		if err != nil {
			s.log("server stopped")
			return
		}
		go s.handle(conn)
	}
}

func (s *Server) handle(conn net.Conn) {
	defer conn.Close()
	_ = conn.SetReadDeadline(time.Now().Add(2 * time.Minute))
	r := bufio.NewReader(conn)
	var req request
	if err := readJSONLine(r, &req); err != nil {
		s.writeError(conn, err)
		return
	}
	_ = conn.SetDeadline(time.Time{})
	switch req.Type {
	case TypeRequestList, TypeListLegacy:
		files, err := ListLocalFiles(s.Dir)
		if err != nil {
			s.writeError(conn, err)
			return
		}
		_ = writeJSONLine(conn, response{OK: true, Files: files})
	case TypeUploadStart, TypeUploadLegacy:
		s.handleUpload(conn, r, req)
	case TypeDownloadRequest, TypeDownloadLegacy:
		s.handleDownload(conn, req)
	case TypeDeleteRequest, TypeDeleteLegacy:
		s.handleDelete(conn, req)
	default:
		s.writeError(conn, errors.New("unknown request"))
	}
}

func (s *Server) handleUpload(conn net.Conn, r *bufio.Reader, req request) {
	if req.Size < 0 {
		s.writeError(conn, errors.New("invalid file size"))
		return
	}
	name, err := cleanName(req.Name)
	if err != nil {
		s.writeError(conn, err)
		return
	}
	lock := s.fileLock(name)
	lock.Lock()
	defer lock.Unlock()

	tmp, err := os.CreateTemp(s.Dir, ".upload-*.tmp")
	if err != nil {
		s.writeError(conn, err)
		return
	}
	tmpPath := tmp.Name()
	committed := false
	defer func() {
		_ = tmp.Close()
		if !committed {
			_ = os.Remove(tmpPath)
		}
	}()

	if req.Type == TypeUploadStart {
		if err := writeJSONLine(conn, response{OK: true}); err != nil {
			return
		}
	}

	written, err := receiveUpload(conn, tmp, r, req.Size)
	if err != nil {
		s.writeError(conn, err)
		return
	}
	if written != req.Size {
		s.writeError(conn, io.ErrShortWrite)
		return
	}
	if err := tmp.Close(); err != nil {
		s.writeError(conn, err)
		return
	}
	finalPath := filepath.Join(s.Dir, name)
	if err := replaceFile(tmpPath, finalPath); err != nil {
		s.writeError(conn, err)
		return
	}
	committed = true
	s.log(fmt.Sprintf("uploaded %s (%s)", name, FormatSize(req.Size)))
	_ = writeJSONLine(conn, response{OK: true})
}

func (s *Server) handleDownload(conn net.Conn, req request) {
	name, err := cleanName(req.Name)
	if err != nil {
		s.writeError(conn, err)
		return
	}
	lock := s.fileLock(name)
	lock.RLock()
	defer lock.RUnlock()

	file, err := os.Open(filepath.Join(s.Dir, name))
	if err != nil {
		s.writeError(conn, err)
		return
	}
	defer file.Close()
	info, err := file.Stat()
	if err != nil {
		s.writeError(conn, err)
		return
	}
	if err := writeJSONLine(conn, response{OK: true, Size: info.Size()}); err != nil {
		return
	}
	_, _ = io.Copy(conn, file)
	s.log("downloaded " + name)
}

func (s *Server) handleDelete(conn net.Conn, req request) {
	name, err := cleanName(req.Name)
	if err != nil {
		s.writeError(conn, err)
		return
	}
	lock := s.fileLock(name)
	lock.Lock()
	defer lock.Unlock()

	if err := os.Remove(filepath.Join(s.Dir, name)); err != nil {
		s.writeError(conn, err)
		return
	}
	s.log("deleted " + name)
	_ = writeJSONLine(conn, response{OK: true})
}

func (s *Server) writeError(conn net.Conn, err error) {
	_ = writeJSONLine(conn, response{OK: false, Error: err.Error()})
	s.log("error: " + err.Error())
}

func (s *Server) log(msg string) {
	if s.Log != nil {
		s.Log(msg)
	}
}

func (s *Server) fileLock(name string) *sync.RWMutex {
	s.locksMu.Lock()
	defer s.locksMu.Unlock()
	if s.locks == nil {
		s.locks = make(map[string]*sync.RWMutex)
	}
	lock := s.locks[name]
	if lock == nil {
		lock = &sync.RWMutex{}
		s.locks[name] = lock
	}
	return lock
}

func replaceFile(src, dst string) error {
	if err := os.Rename(src, dst); err == nil {
		return nil
	}
	if err := os.Remove(dst); err != nil && !errors.Is(err, os.ErrNotExist) {
		return err
	}
	return os.Rename(src, dst)
}

func receiveUpload(conn net.Conn, dst io.Writer, src *bufio.Reader, total int64) (int64, error) {
	buf := make([]byte, transferBufferSize)
	var done int64
	for done < total {
		if err := conn.SetReadDeadline(time.Now().Add(uploadIdleTimeout)); err != nil {
			return done, err
		}
		want := int64(len(buf))
		if remaining := total - done; remaining < want {
			want = remaining
		}
		n, readErr := src.Read(buf[:int(want)])
		if n > 0 {
			written, writeErr := dst.Write(buf[:n])
			done += int64(written)
			if writeErr != nil {
				_ = drainUpload(conn, src, total-done)
				_ = conn.SetReadDeadline(time.Time{})
				return done, fmt.Errorf("server could not save uploaded data: %w", writeErr)
			}
			if written != n {
				_ = drainUpload(conn, src, total-done)
				_ = conn.SetReadDeadline(time.Time{})
				return done, io.ErrShortWrite
			}
		}
		if done >= total {
			break
		}
		if readErr != nil {
			_ = conn.SetReadDeadline(time.Time{})
			return done, fmt.Errorf("server stopped receiving upload after %s of %s: %w", FormatSize(done), FormatSize(total), readErr)
		}
	}
	_ = conn.SetReadDeadline(time.Time{})
	return done, nil
}

func drainUpload(conn net.Conn, src *bufio.Reader, remaining int64) error {
	buf := make([]byte, transferBufferSize)
	for remaining > 0 {
		_ = conn.SetReadDeadline(time.Now().Add(uploadIdleTimeout))
		want := int64(len(buf))
		if remaining < want {
			want = remaining
		}
		n, err := src.Read(buf[:int(want)])
		remaining -= int64(n)
		if err != nil {
			return err
		}
	}
	return conn.SetReadDeadline(time.Time{})
}
