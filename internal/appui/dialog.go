package appui

import (
	"errors"
	"strings"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

func ShowError(w fyne.Window, title string, err error) {
	if err == nil {
		err = errors.New("unknown error")
	}
	heading := widget.NewLabelWithStyle(title, fyne.TextAlignLeading, fyne.TextStyle{Bold: true})
	summary := widget.NewLabel(errorSummary(err))
	summary.Wrapping = fyne.TextWrapWord
	details := widget.NewLabel("Details: " + Ellipsize(err.Error(), 520))
	details.Wrapping = fyne.TextWrapWord
	body := container.NewGridWrap(fyne.NewSize(520, 190), container.NewVBox(heading, summary, widget.NewSeparator(), details))
	content := container.NewBorder(nil, nil, widget.NewIcon(theme.WarningIcon()), nil, body)
	dialog.ShowCustom("Error", "Close", content, w)
}

func errorSummary(err error) string {
	text := strings.ToLower(err.Error())
	switch {
	case strings.Contains(text, "forcibly closed") || strings.Contains(text, "connection reset"):
		return "The connection was closed while the transfer was running. Make sure the server is still running and has enough disk space, then try again."
	case strings.Contains(text, "no space") || strings.Contains(text, "disk"):
		return "The destination disk could not store the file. Check available space and write permissions."
	case strings.Contains(text, "permission") || strings.Contains(text, "access is denied"):
		return "The app does not have permission to access that file or folder."
	case strings.Contains(text, "connection refused"):
		return "The server is not accepting connections at this address."
	default:
		return "The operation could not be completed."
	}
}
