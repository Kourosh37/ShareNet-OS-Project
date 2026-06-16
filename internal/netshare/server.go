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
)

type Server struct {
	Dir      string
	Log      func(string)
	listener net.Listener
	mu       sync.Mutex
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
	r := bufio.NewReader(conn)
	var req request
	if err := readJSONLine(r, &req); err != nil {
		s.writeError(conn, err)
		return
	}
	switch req.Type {
	case "list":
		files, err := ListLocalFiles(s.Dir)
		if err != nil {
			s.writeError(conn, err)
			return
		}
		_ = writeJSONLine(conn, response{OK: true, Files: files})
	case "upload":
		s.handleUpload(conn, r, req)
	case "download":
		s.handleDownload(conn, req)
	case "delete":
		s.handleDelete(conn, req)
	default:
		s.writeError(conn, errors.New("unknown request"))
	}
}

func (s *Server) handleUpload(conn net.Conn, r *bufio.Reader, req request) {
	name, err := cleanName(req.Name)
	if err != nil {
		s.writeError(conn, err)
		return
	}
	out, err := os.Create(filepath.Join(s.Dir, name))
	if err != nil {
		s.writeError(conn, err)
		return
	}
	defer out.Close()
	if _, err := io.CopyN(out, r, req.Size); err != nil {
		s.writeError(conn, err)
		return
	}
	s.log(fmt.Sprintf("uploaded %s (%s)", name, FormatSize(req.Size)))
	_ = writeJSONLine(conn, response{OK: true})
}

func (s *Server) handleDownload(conn net.Conn, req request) {
	name, err := cleanName(req.Name)
	if err != nil {
		s.writeError(conn, err)
		return
	}
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
