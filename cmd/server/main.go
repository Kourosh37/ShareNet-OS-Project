package main

import (
	"fmt"
	"os"
	"path/filepath"
	"time"

	"sharenet/internal/netshare"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

func main() {
	_ = os.MkdirAll("server_files", 0755)

	a := app.NewWithID("sharenet.server")
	w := a.NewWindow("ShareNet Server")
	w.Resize(fyne.NewSize(820, 560))

	ipEntry := widget.NewEntry()
	ipEntry.SetText("0.0.0.0")
	portEntry := widget.NewEntry()
	portEntry.SetText("5000")
	dirEntry := widget.NewEntry()
	dirEntry.SetText(abs("server_files"))

	logs := widget.NewMultiLineEntry()
	logs.Disable()
	logs.SetPlaceHolder("Server events")

	var files []netshare.FileInfo
	var selected string
	fileList := widget.NewList(
		func() int { return len(files) },
		func() fyne.CanvasObject {
			return container.NewHBox(widget.NewIcon(theme.DocumentIcon()), widget.NewLabel(""))
		},
		func(id widget.ListItemID, obj fyne.CanvasObject) {
			row := obj.(*fyne.Container)
			label := row.Objects[1].(*widget.Label)
			label.SetText(fmt.Sprintf("%s    %s", files[id].Name, netshare.FormatSize(files[id].Size)))
		},
	)
	fileList.OnSelected = func(id widget.ListItemID) {
		if id >= 0 && id < len(files) {
			selected = files[id].Name
		}
	}

	appendLog := func(msg string) {
		fyne.Do(func() {
			logs.SetText(logs.Text + time.Now().Format("15:04:05") + "  " + msg + "\n")
			logs.CursorRow = len(logs.Text)
		})
	}

	refresh := func() {
		list, err := netshare.ListLocalFiles(dirEntry.Text)
		if err != nil {
			appendLog("refresh failed: " + err.Error())
			return
		}
		files = list
		selected = ""
		fileList.Refresh()
	}

	srv := &netshare.Server{Dir: dirEntry.Text, Log: appendLog}
	status := widget.NewLabel("Stopped")
	startBtn := widget.NewButtonWithIcon("Start", theme.MediaPlayIcon(), nil)
	stopBtn := widget.NewButtonWithIcon("Stop", theme.MediaStopIcon(), nil)
	stopBtn.Disable()

	startBtn.OnTapped = func() {
		if srv.Running() {
			return
		}
		srv.Dir = dirEntry.Text
		addr := ipEntry.Text + ":" + portEntry.Text
		if err := srv.Start(addr); err != nil {
			dialog.ShowError(err, w)
			return
		}
		status.SetText("Running on " + addr)
		startBtn.Disable()
		stopBtn.Enable()
		refresh()
	}

	stopBtn.OnTapped = func() {
		_ = srv.Stop()
		status.SetText("Stopped")
		startBtn.Enable()
		stopBtn.Disable()
	}

	browseBtn := widget.NewButtonWithIcon("Folder", theme.FolderOpenIcon(), func() {
		d := dialog.NewFolderOpen(func(uri fyne.ListableURI, err error) {
			if err != nil {
				dialog.ShowError(err, w)
				return
			}
			if uri != nil {
				dirEntry.SetText(uri.Path())
				srv.Dir = uri.Path()
				refresh()
			}
		}, w)
		d.Show()
	})

	refreshBtn := widget.NewButtonWithIcon("Refresh", theme.ViewRefreshIcon(), refresh)
	deleteBtn := widget.NewButtonWithIcon("Delete", theme.DeleteIcon(), func() {
		if selected == "" {
			dialog.ShowInformation("No file selected", "Select a server file first.", w)
			return
		}
		if err := os.Remove(filepath.Join(dirEntry.Text, selected)); err != nil {
			dialog.ShowError(err, w)
			return
		}
		appendLog("deleted " + selected)
		refresh()
	})
	copyBtn := widget.NewButtonWithIcon("Copy To...", theme.DownloadIcon(), func() {
		if selected == "" {
			dialog.ShowInformation("No file selected", "Select a server file first.", w)
			return
		}
		d := dialog.NewFolderOpen(func(uri fyne.ListableURI, err error) {
			if err != nil {
				dialog.ShowError(err, w)
				return
			}
			if uri == nil {
				return
			}
			src := filepath.Join(dirEntry.Text, selected)
			dst := filepath.Join(uri.Path(), selected)
			data, err := os.ReadFile(src)
			if err != nil {
				dialog.ShowError(err, w)
				return
			}
			if err := os.WriteFile(dst, data, 0644); err != nil {
				dialog.ShowError(err, w)
				return
			}
			appendLog("copied " + selected + " to " + uri.Path())
		}, w)
		d.Show()
	})

	top := container.NewVBox(
		container.NewGridWithColumns(4,
			container.NewBorder(nil, nil, widget.NewLabel("Bind IP"), nil, ipEntry),
			container.NewBorder(nil, nil, widget.NewLabel("Port"), nil, portEntry),
			startBtn,
			stopBtn,
		),
		container.NewBorder(nil, nil, widget.NewLabel("Files"), browseBtn, dirEntry),
		container.NewHBox(status, refreshBtn, deleteBtn, copyBtn),
	)

	content := container.NewBorder(top, nil, nil, nil,
		container.NewHSplit(fileList, logs),
	)
	w.SetContent(content)
	refresh()
	w.SetCloseIntercept(func() {
		_ = srv.Stop()
		w.Close()
	})
	w.ShowAndRun()
}

func abs(path string) string {
	out, err := filepath.Abs(path)
	if err != nil {
		return path
	}
	return out
}
