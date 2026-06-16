package main

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"time"

	"sharenet/internal/appui"
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
	a.Settings().SetTheme(appui.Theme{})
	w := a.NewWindow("ShareNet Server")
	w.Resize(fyne.NewSize(980, 640))
	w.CenterOnScreen()

	ipEntry := widget.NewEntry()
	ipEntry.SetText("0.0.0.0")
	portEntry := widget.NewEntry()
	portEntry.SetText("5000")
	dirEntry := widget.NewEntry()
	dirEntry.SetText(abs("server_files"))

	logs := widget.NewMultiLineEntry()
	logs.Disable()
	logs.SetPlaceHolder("Server events")
	status := widget.NewLabel("Stopped")
	status.TextStyle = fyne.TextStyle{Bold: true}
	statusDot := appui.NewStatusDot(false)

	var files []netshare.FileInfo
	var selected string
	fileList := widget.NewList(
		func() int { return len(files) },
		func() fyne.CanvasObject {
			icon := widget.NewIcon(theme.DocumentIcon())
			name := widget.NewLabelWithStyle("", fyne.TextAlignLeading, fyne.TextStyle{Bold: true})
			meta := appui.Meta("")
			return container.NewHBox(icon, container.NewVBox(name, meta))
		},
		func(id widget.ListItemID, obj fyne.CanvasObject) {
			row := obj.(*fyne.Container)
			body := row.Objects[1].(*fyne.Container)
			name := body.Objects[0].(*widget.Label)
			meta := body.Objects[1].(*widget.Label)
			info := files[id]
			name.SetText(appui.Ellipsize(info.Name, 54))
			meta.SetText(fmt.Sprintf("%s  |  %s", netshare.FormatSize(info.Size), displayTime(info.ModTime)))
		},
	)
	fileList.OnSelected = func(id widget.ListItemID) {
		if id >= 0 && id < len(files) {
			selected = files[id].Name
			status.SetText("Selected " + appui.Ellipsize(selected, 48))
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
			appui.ShowError(w, "Could not start server", err)
			return
		}
		status.SetText("Running on " + appui.Ellipsize(addr, 48))
		statusDot.SetRunning(true)
		startBtn.Disable()
		stopBtn.Enable()
		refresh()
	}
	stopBtn.OnTapped = func() {
		_ = srv.Stop()
		status.SetText("Stopped")
		statusDot.SetRunning(false)
		startBtn.Enable()
		stopBtn.Disable()
	}

	browseBtn := widget.NewButtonWithIcon("", theme.FolderOpenIcon(), func() {
		d := dialog.NewFolderOpen(func(uri fyne.ListableURI, err error) {
			if err != nil {
				appui.ShowError(w, "Could not choose folder", err)
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
		name := selected
		go func() {
			err := os.Remove(filepath.Join(dirEntry.Text, name))
			fyne.Do(func() {
				if err != nil {
					appui.ShowError(w, "Could not delete file", err)
					return
				}
				appendLog("deleted " + name)
				refresh()
			})
		}()
	})
	copyBtn := widget.NewButtonWithIcon("Copy To", theme.DownloadIcon(), func() {
		if selected == "" {
			dialog.ShowInformation("No file selected", "Select a server file first.", w)
			return
		}
		name := selected
		d := dialog.NewFolderOpen(func(uri fyne.ListableURI, err error) {
			if err != nil {
				appui.ShowError(w, "Could not choose folder", err)
				return
			}
			if uri == nil {
				return
			}
			dstDir := uri.Path()
			go func() {
				err := copyFile(filepath.Join(dirEntry.Text, name), filepath.Join(dstDir, name))
				fyne.Do(func() {
					if err != nil {
						appui.ShowError(w, "Could not copy file", err)
						return
					}
					appendLog("copied " + appui.Ellipsize(name, 40) + " to " + appui.Ellipsize(dstDir, 64))
				})
			}()
		}, w)
		d.Show()
	})

	serverControls := appui.Section("Server", "Bind endpoint and process concurrent clients",
		container.NewVBox(
			container.NewGridWithColumns(2,
				container.NewBorder(nil, nil, widget.NewLabel("Bind IP"), nil, ipEntry),
				container.NewBorder(nil, nil, widget.NewLabel("Port"), nil, portEntry),
			),
			container.NewHBox(
				startBtn,
				stopBtn,
			),
			container.NewHBox(container.NewCenter(statusDot), status),
		),
	)
	storage := appui.Section("Storage", "Hosted files directory",
		container.NewVBox(
			container.NewBorder(nil, nil, widget.NewLabel("Folder"), browseBtn, dirEntry),
		),
	)
	activity := appui.Section("Activity", "Recent server operations", logs)
	filesTab := appui.Section("Files", "Hosted files",
		container.NewBorder(container.NewHBox(refreshBtn, deleteBtn, copyBtn), nil, nil, nil, fileList),
	)
	tabs := container.NewAppTabs(
		container.NewTabItemWithIcon("Server", theme.ComputerIcon(), serverControls),
		container.NewTabItemWithIcon("Storage", theme.FolderIcon(), storage),
		container.NewTabItemWithIcon("Files", theme.DocumentIcon(), filesTab),
		container.NewTabItemWithIcon("Activity", theme.HistoryIcon(), activity),
	)
	tabs.SetTabLocation(container.TabLocationTop)
	w.SetContent(tabs)
	refresh()
	w.SetCloseIntercept(func() {
		_ = srv.Stop()
		statusDot.SetRunning(false)
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

func copyFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	out, err := os.Create(dst)
	if err != nil {
		return err
	}
	defer out.Close()
	if _, err := io.Copy(out, in); err != nil {
		return err
	}
	return out.Sync()
}

func displayTime(value string) string {
	if value == "" {
		return "unknown"
	}
	return value
}
