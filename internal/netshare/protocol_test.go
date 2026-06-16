package netshare

import (
	"bytes"
	"fmt"
	"net"
	"os"
	"path/filepath"
	"sync"
	"testing"
	"time"
)

func TestConcurrentTransfersKeepFileIntegrity(t *testing.T) {
	serverDir := t.TempDir()
	clientDir := t.TempDir()
	downloadDir := t.TempDir()

	sourcePath := filepath.Join(clientDir, "payload.bin")
	sourceData := bytes.Repeat([]byte("ShareNet phase two data\n"), 4096)
	if err := os.WriteFile(sourcePath, sourceData, 0644); err != nil {
		t.Fatal(err)
	}

	addr, stop := startTestServer(t, serverDir)
	defer stop()

	if err := Upload(addr, sourcePath, nil); err != nil {
		t.Fatalf("upload failed: %v", err)
	}

	const clients = 4
	var wg sync.WaitGroup
	errs := make(chan error, clients*2)
	for i := 0; i < clients; i++ {
		wg.Add(2)
		go func(id int) {
			defer wg.Done()
			files, err := List(addr)
			if err != nil {
				errs <- fmt.Errorf("list %d: %w", id, err)
				return
			}
			if len(files) != 1 || files[0].Name != "payload.bin" {
				errs <- fmt.Errorf("list %d returned %#v", id, files)
			}
		}(i)
		go func(id int) {
			defer wg.Done()
			outDir := filepath.Join(downloadDir, fmt.Sprintf("client-%d", id))
			out, err := Download(addr, "payload.bin", outDir, nil)
			if err != nil {
				errs <- fmt.Errorf("download %d: %w", id, err)
				return
			}
			got, err := os.ReadFile(out)
			if err != nil {
				errs <- fmt.Errorf("read download %d: %w", id, err)
				return
			}
			if !bytes.Equal(got, sourceData) {
				errs <- fmt.Errorf("download %d content mismatch", id)
			}
		}(i)
	}
	wg.Wait()
	close(errs)

	for err := range errs {
		if err != nil {
			t.Fatal(err)
		}
	}
}

func TestConcurrentSameNameUploadsLeaveOneCompleteFile(t *testing.T) {
	serverDir := t.TempDir()
	addr, stop := startTestServer(t, serverDir)
	defer stop()

	const uploads = 5
	expected := make(map[string]bool, uploads)
	var wg sync.WaitGroup
	errs := make(chan error, uploads)

	for i := 0; i < uploads; i++ {
		dir := t.TempDir()
		data := bytes.Repeat([]byte(fmt.Sprintf("writer-%d\n", i)), 8192)
		expected[string(data)] = true
		path := filepath.Join(dir, "shared.txt")
		if err := os.WriteFile(path, data, 0644); err != nil {
			t.Fatal(err)
		}

		wg.Add(1)
		go func(path string) {
			defer wg.Done()
			if err := Upload(addr, path, nil); err != nil {
				errs <- err
			}
		}(path)
	}

	wg.Wait()
	close(errs)
	for err := range errs {
		if err != nil {
			t.Fatal(err)
		}
	}

	files, err := ListLocalFiles(serverDir)
	if err != nil {
		t.Fatal(err)
	}
	if len(files) != 1 || files[0].Name != "shared.txt" {
		t.Fatalf("unexpected files after concurrent upload: %#v", files)
	}
	got, err := os.ReadFile(filepath.Join(serverDir, "shared.txt"))
	if err != nil {
		t.Fatal(err)
	}
	if !expected[string(got)] {
		t.Fatalf("final file is not one of the complete uploaded files; size=%d", len(got))
	}
}

func TestUnicodeFileNamesRoundTrip(t *testing.T) {
	serverDir := t.TempDir()
	clientDir := t.TempDir()
	downloadDir := t.TempDir()

	name := "گزارش نهایی ۱۴۰۳.txt"
	data := []byte("unicode filename payload")
	sourcePath := filepath.Join(clientDir, name)
	if err := os.WriteFile(sourcePath, data, 0644); err != nil {
		t.Fatal(err)
	}

	addr, stop := startTestServer(t, serverDir)
	defer stop()

	if err := Upload(addr, sourcePath, nil); err != nil {
		t.Fatalf("upload unicode file: %v", err)
	}
	files, err := List(addr)
	if err != nil {
		t.Fatalf("list unicode file: %v", err)
	}
	if len(files) != 1 || files[0].Name != name {
		t.Fatalf("unexpected unicode listing: %#v", files)
	}

	out, err := Download(addr, name, downloadDir, nil)
	if err != nil {
		t.Fatalf("download unicode file: %v", err)
	}
	got, err := os.ReadFile(out)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(got, data) {
		t.Fatalf("downloaded unicode file mismatch")
	}
	if filepath.Base(out) != name {
		t.Fatalf("downloaded name mismatch: got %q want %q", filepath.Base(out), name)
	}
}

func TestLargeFileUploadAndDownload(t *testing.T) {
	serverDir := t.TempDir()
	clientDir := t.TempDir()
	downloadDir := t.TempDir()

	sourcePath := filepath.Join(clientDir, "large.bin")
	sourceData := bytes.Repeat([]byte("0123456789abcdef"), 128*1024)
	if err := os.WriteFile(sourcePath, sourceData, 0644); err != nil {
		t.Fatal(err)
	}

	addr, stop := startTestServer(t, serverDir)
	defer stop()

	if err := Upload(addr, sourcePath, nil); err != nil {
		t.Fatalf("large upload failed: %v", err)
	}
	out, err := Download(addr, "large.bin", downloadDir, nil)
	if err != nil {
		t.Fatalf("large download failed: %v", err)
	}
	got, err := os.ReadFile(out)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(got, sourceData) {
		t.Fatalf("large file content mismatch")
	}
}

func startTestServer(t *testing.T, dir string) (string, func()) {
	t.Helper()
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	addr := ln.Addr().String()
	if err := ln.Close(); err != nil {
		t.Fatal(err)
	}

	srv := &Server{Dir: dir}
	if err := srv.Start(addr); err != nil {
		t.Fatal(err)
	}
	time.Sleep(20 * time.Millisecond)
	return addr, func() {
		_ = srv.Stop()
	}
}
