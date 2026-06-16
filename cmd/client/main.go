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
	_ = os.MkdirAll("client_files", 0755)
	_ = os.MkdirAll("downloads", 0755)

	a := app.NewWithID("sharenet.client")
	w := a.NewWindow("ShareNet Client")
	w.Resize(fyne.NewSize(880, 620))

	ipEntry := widget.NewEntry()
	ipEntry.SetText("127.0.0.1")
	portEntry := widget.NewEntry()
	portEntry.SetText("5000")
	uploadEntry := widget.NewEntry()
	uploadEntry.SetText(abs("client_files"))
	downloadEntry := widget.NewEntry()
	downloadEntry.SetText(abs("downloads"))
	progress := widget.NewProgressBar()
	status := widget.NewLabel("Ready")
	logs := widget.NewMultiLineEntry()
	logs.Disable()

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

	addr := func() string { return ipEntry.Text + ":" + portEntry.Text }
	appendLog := func(msg string) {
		fyne.Do(func() {
			logs.SetText(logs.Text + time.Now().Format("15:04:05") + "  " + msg + "\n")
			logs.CursorRow = len(logs.Text)
		})
	}
	setProgress := func(done, total int64) {
		if total <= 0 {
			return
		}
		fyne.Do(func() {
			progress.SetValue(float64(done) / float64(total))
			status.SetText(fmt.Sprintf("%d%%", int(float64(done)*100/float64(total))))
		})
	}
	refresh := func() {
		status.SetText("Listing...")
		go func() {
			list, err := netshare.List(addr())
			fyne.Do(func() {
				if err != nil {
					status.SetText("List failed")
					appendLog("list failed: " + err.Error())
					return
				}
				files = list
				selected = ""
				fileList.Refresh()
				status.SetText(fmt.Sprintf("%d files", len(files)))
			})
		}()
	}

	browseUpload := widget.NewButtonWithIcon("File", theme.FileIcon(), func() {
		d := dialog.NewFileOpen(func(r fyne.URIReadCloser, err error) {
			if err != nil {
				dialog.ShowError(err, w)
				return
			}
			if r == nil {
				return
			}
			defer r.Close()
			uploadEntry.SetText(r.URI().Path())
		}, w)
		d.Show()
	})
	browseDownload := widget.NewButtonWithIcon("Folder", theme.FolderOpenIcon(), func() {
		d := dialog.NewFolderOpen(func(uri fyne.ListableURI, err error) {
			if err != nil {
				dialog.ShowError(err, w)
				return
			}
			if uri != nil {
				downloadEntry.SetText(uri.Path())
			}
		}, w)
		d.Show()
	})

	listBtn := widget.NewButtonWithIcon("List", theme.ViewRefreshIcon(), refresh)
	uploadBtn := widget.NewButtonWithIcon("Upload", theme.UploadIcon(), func() {
		path := uploadEntry.Text
		progress.SetValue(0)
		status.SetText("Uploading...")
		go func() {
			err := netshare.Upload(addr(), path, setProgress)
			fyne.Do(func() {
				if err != nil {
					status.SetText("Upload failed")
					appendLog("upload failed: " + err.Error())
					return
				}
				progress.SetValue(1)
				status.SetText("Upload complete")
				appendLog("uploaded " + filepath.Base(path))
				refresh()
			})
		}()
	})
	downloadBtn := widget.NewButtonWithIcon("Download", theme.DownloadIcon(), func() {
		if selected == "" {
			dialog.ShowInformation("No file selected", "Select a server file first.", w)
			return
		}
		progress.SetValue(0)
		status.SetText("Downloading...")
		go func() {
			out, err := netshare.Download(addr(), selected, downloadEntry.Text, setProgress)
			fyne.Do(func() {
				if err != nil {
					status.SetText("Download failed")
					appendLog("download failed: " + err.Error())
					return
				}
				progress.SetValue(1)
				status.SetText("Download complete")
				appendLog("downloaded to " + out)
			})
		}()
	})
	deleteBtn := widget.NewButtonWithIcon("Delete", theme.DeleteIcon(), func() {
		if selected == "" {
			dialog.ShowInformation("No file selected", "Select a server file first.", w)
			return
		}
		go func(name string) {
			err := netshare.Delete(addr(), name)
			fyne.Do(func() {
				if err != nil {
					appendLog("delete failed: " + err.Error())
					return
				}
				appendLog("deleted " + name)
				refresh()
			})
		}(selected)
	})

	top := container.NewVBox(
		container.NewGridWithColumns(4,
			container.NewBorder(nil, nil, widget.NewLabel("Server"), nil, ipEntry),
			container.NewBorder(nil, nil, widget.NewLabel("Port"), nil, portEntry),
			listBtn,
			status,
		),
		container.NewBorder(nil, nil, widget.NewLabel("Upload"), browseUpload, uploadEntry),
		container.NewBorder(nil, nil, widget.NewLabel("Downloads"), browseDownload, downloadEntry),
		container.NewHBox(uploadBtn, downloadBtn, deleteBtn),
		progress,
	)

	w.SetContent(container.NewBorder(top, nil, nil, nil, container.NewHSplit(fileList, logs)))
	w.ShowAndRun()
}

func abs(path string) string {
	out, err := filepath.Abs(path)
	if err != nil {
		return path
	}
	return out
}
