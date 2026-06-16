package main

import (
	"fmt"
	"os"
	"path/filepath"

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
	_ = os.MkdirAll("client_files", 0755)
	_ = os.MkdirAll("downloads", 0755)

	a := app.NewWithID("sharenet.client")
	a.Settings().SetTheme(appui.Theme{})
	w := a.NewWindow("ShareNet Client")
	w.Resize(fyne.NewSize(980, 640))
	w.CenterOnScreen()

	ipEntry := widget.NewEntry()
	ipEntry.SetText("127.0.0.1")
	portEntry := widget.NewEntry()
	portEntry.SetText("5000")
	connectionState := widget.NewLabelWithStyle("Not checked", fyne.TextAlignLeading, fyne.TextStyle{Bold: true})
	connectionDot := appui.NewStatusDot(false)

	downloadDir := abs("downloads")
	downloadDirLabel := widget.NewLabel(appui.Ellipsize(downloadDir, 92))

	selectedFilePath := ""
	selectedFileName := widget.NewLabelWithStyle("No file selected", fyne.TextAlignLeading, fyne.TextStyle{Bold: true})
	selectedFileMeta := appui.CompactMeta("Select a file before uploading.")
	uploadProgress := appui.NewSlimProgress()
	uploadState := widget.NewLabelWithStyle("Idle", fyne.TextAlignLeading, fyne.TextStyle{Bold: true})
	uploadBtn := widget.NewButtonWithIcon("Upload", theme.UploadIcon(), nil)
	uploadBtn.Disable()

	downloadProgress := appui.NewSlimProgress()
	downloadState := widget.NewLabelWithStyle("Idle", fyne.TextAlignLeading, fyne.TextStyle{Bold: true})

	var files []netshare.FileInfo
	downloaded := map[string]bool{}

	addr := func() string {
		return ipEntry.Text + ":" + portEntry.Text
	}
	setProgress := func(bar *appui.SlimProgress, state *widget.Label, prefix string) netshare.ProgressFunc {
		return func(done, total int64) {
			fyne.Do(func() {
				if total <= 0 {
					bar.SetValue(0)
					state.SetText(prefix)
					return
				}
				value := float64(done) / float64(total)
				bar.SetValue(value)
				if done >= total {
					if prefix == "Downloading" {
						state.SetText("Finalizing download...")
					} else {
						state.SetText("Finalizing upload...")
					}
					return
				}
				state.SetText(appui.Ellipsize(fmt.Sprintf("%s  %d%%  |  %s / %s", prefix, int(value*100), netshare.FormatSize(done), netshare.FormatSize(total)), 72))
			})
		}
	}

	var refresh func()
	downloadFile := func(name string) {
		downloadProgress.SetValue(0)
		downloadState.SetText("Downloading " + appui.Ellipsize(name, 56))
		go func() {
			out, err := netshare.Download(addr(), name, downloadDir, setProgress(downloadProgress, downloadState, "Downloading"))
			fyne.Do(func() {
				if err != nil {
					downloadState.SetText("Download failed")
					connectionState.SetText("Offline")
					connectionDot.SetRunning(false)
					appui.ShowError(w, "Download failed", err)
					return
				}
				downloadProgress.SetValue(1)
				downloadState.SetText("Saved to " + appui.Ellipsize(out, 72))
				downloaded[name] = true
				refresh()
			})
		}()
	}

	fileList := widget.NewList(
		func() int { return len(files) },
		func() fyne.CanvasObject {
			statusIcon := widget.NewIcon(theme.DocumentIcon())
			name := widget.NewLabelWithStyle("", fyne.TextAlignLeading, fyne.TextStyle{Bold: true})
			meta := appui.CompactMeta("")
			downloadBtn := widget.NewButtonWithIcon("", theme.DownloadIcon(), nil)
			downloadBtn.Importance = widget.MediumImportance
			return container.NewHBox(statusIcon, container.NewVBox(name, meta), downloadBtn)
		},
		func(id widget.ListItemID, obj fyne.CanvasObject) {
			row := obj.(*fyne.Container)
			statusIcon := row.Objects[0].(*widget.Icon)
			body := row.Objects[1].(*fyne.Container)
			downloadBtn := row.Objects[2].(*widget.Button)
			nameLabel := body.Objects[0].(*widget.Label)
			meta := body.Objects[1].(*widget.Label)
			info := files[id]

			if downloaded[info.Name] {
				statusIcon.SetResource(theme.ConfirmIcon())
			} else {
				statusIcon.SetResource(theme.DocumentIcon())
			}
			nameLabel.SetText(appui.Ellipsize(info.Name, 52))
			meta.SetText(fmt.Sprintf("%s  |  %s", netshare.FormatSize(info.Size), displayTime(info.ModTime)))
			downloadBtn.OnTapped = func() {
				downloadFile(info.Name)
			}
		},
	)

	refresh = func() {
		connectionState.SetText("Checking...")
		connectionDot.SetRunning(false)
		go func() {
			list, err := netshare.List(addr())
			fyne.Do(func() {
				if err != nil {
					connectionState.SetText("Offline")
					connectionDot.SetRunning(false)
					files = nil
					fileList.Refresh()
					return
				}
				files = list
				fileList.Refresh()
				connectionState.SetText(fmt.Sprintf("Online | %d files", len(files)))
				connectionDot.SetRunning(true)
			})
		}()
	}

	selectFileBtn := widget.NewButtonWithIcon("Select File", theme.FileIcon(), func() {
		d := dialog.NewFileOpen(func(r fyne.URIReadCloser, err error) {
			if err != nil {
				appui.ShowError(w, "Could not open file", err)
				return
			}
			if r == nil {
				return
			}
			defer r.Close()
			selectedFilePath = r.URI().Path()
			info, statErr := os.Stat(selectedFilePath)
			if statErr != nil {
				selectedFileName.SetText(appui.Ellipsize(filepath.Base(selectedFilePath), 64))
				selectedFileMeta.SetText(appui.Ellipsize(selectedFilePath, 92))
			} else {
				selectedFileName.SetText(appui.Ellipsize(info.Name(), 64))
				selectedFileMeta.SetText(appui.Ellipsize(fmt.Sprintf("%s  |  %s", netshare.FormatSize(info.Size()), selectedFilePath), 92))
			}
			uploadBtn.Enable()
		}, w)
		d.Resize(fyne.NewSize(820, 560))
		d.Show()
	})

	uploadBtn.OnTapped = func() {
		if selectedFilePath == "" {
			dialog.ShowInformation("No file selected", "Select a file before uploading.", w)
			return
		}
		uploadProgress.SetValue(0)
		uploadState.SetText("Uploading " + appui.Ellipsize(filepath.Base(selectedFilePath), 56))
		uploadBtn.Disable()
		go func(path string) {
			err := netshare.Upload(addr(), path, setProgress(uploadProgress, uploadState, "Uploading"))
			fyne.Do(func() {
				uploadBtn.Enable()
				if err != nil {
					uploadState.SetText("Upload failed")
					connectionState.SetText("Offline")
					connectionDot.SetRunning(false)
					appui.ShowError(w, "Upload failed", err)
					return
				}
				uploadProgress.SetValue(1)
				uploadState.SetText("Upload complete")
				refresh()
			})
		}(selectedFilePath)
	}

	browseDownloadDir := widget.NewButtonWithIcon("Choose Folder", theme.FolderOpenIcon(), func() {
		d := dialog.NewFolderOpen(func(uri fyne.ListableURI, err error) {
			if err != nil {
				appui.ShowError(w, "Could not choose folder", err)
				return
			}
			if uri == nil {
				return
			}
			downloadDir = uri.Path()
			downloadDirLabel.SetText(appui.Ellipsize(downloadDir, 92))
		}, w)
		d.Show()
	})

	serverTab := appui.Section("Server", "Endpoint and connection status",
		container.NewVBox(
			container.NewGridWithColumns(2,
				container.NewBorder(nil, nil, widget.NewLabel("IP"), nil, ipEntry),
				container.NewBorder(nil, nil, widget.NewLabel("Port"), nil, portEntry),
			),
			container.NewHBox(widget.NewButtonWithIcon("Check Connection", theme.SearchIcon(), refresh), container.NewCenter(connectionDot), connectionState),
		),
	)

	storageTab := appui.Section("Storage", "Download destination",
		container.NewVBox(
			container.NewBorder(nil, nil, widget.NewLabel("Downloads"), browseDownloadDir, downloadDirLabel),
			appui.Meta("Downloaded files are saved here unless you choose another folder."),
		),
	)

	selectedFileCard := container.NewBorder(nil, nil, widget.NewIcon(theme.FileIcon()), nil, container.NewVBox(selectedFileName, selectedFileMeta))
	uploadTab := appui.Section("Upload", "Choose a local file and send it to the server",
		container.NewVBox(
			container.NewHBox(selectFileBtn, uploadBtn),
			selectedFileCard,
			container.NewVBox(uploadState, uploadProgress),
		),
	)

	filesToolbar := container.NewVBox(
		container.NewHBox(widget.NewButtonWithIcon("Refresh", theme.ViewRefreshIcon(), refresh), downloadState),
		downloadProgress,
	)
	filesTab := appui.Section("Server Files", "Hosted files",
		container.NewBorder(filesToolbar, nil, nil, nil, fileList),
	)

	tabs := container.NewAppTabs(
		container.NewTabItemWithIcon("Server", theme.ComputerIcon(), serverTab),
		container.NewTabItemWithIcon("Storage", theme.FolderIcon(), storageTab),
		container.NewTabItemWithIcon("Upload", theme.UploadIcon(), uploadTab),
		container.NewTabItemWithIcon("Files", theme.DocumentIcon(), filesTab),
	)
	tabs.SetTabLocation(container.TabLocationTop)

	w.SetContent(tabs)
	refresh()
	w.ShowAndRun()
}

func abs(path string) string {
	out, err := filepath.Abs(path)
	if err != nil {
		return path
	}
	return out
}

func displayTime(value string) string {
	if value == "" {
		return "unknown"
	}
	return value
}
