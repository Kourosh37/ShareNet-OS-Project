package appui

import (
	"image/color"
	"time"
	"unicode/utf8"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

type Theme struct{}

func (Theme) Color(name fyne.ThemeColorName, variant fyne.ThemeVariant) color.Color {
	switch name {
	case theme.ColorNameBackground:
		return color.NRGBA{R: 10, G: 15, B: 24, A: 255}
	case theme.ColorNameForeground:
		return color.NRGBA{R: 248, G: 250, B: 252, A: 255}
	case theme.ColorNameButton:
		return color.NRGBA{R: 28, G: 41, B: 59, A: 255}
	case theme.ColorNameDisabledButton:
		return color.NRGBA{R: 30, G: 41, B: 59, A: 255}
	case theme.ColorNameInputBackground:
		return color.NRGBA{R: 18, G: 28, B: 46, A: 255}
	case theme.ColorNamePlaceHolder:
		return color.NRGBA{R: 203, G: 213, B: 225, A: 255}
	case theme.ColorNamePrimary:
		return color.NRGBA{R: 45, G: 212, B: 191, A: 255}
	case theme.ColorNameSelection:
		return color.NRGBA{R: 19, G: 78, B: 74, A: 255}
	case theme.ColorNameSeparator:
		return color.NRGBA{R: 71, G: 85, B: 105, A: 255}
	case theme.ColorNameHover:
		return color.NRGBA{R: 42, G: 56, B: 78, A: 255}
	}
	return theme.DefaultTheme().Color(name, variant)
}

func (Theme) Font(style fyne.TextStyle) fyne.Resource {
	return theme.DefaultTheme().Font(style)
}

func (Theme) Icon(name fyne.ThemeIconName) fyne.Resource {
	return theme.DefaultTheme().Icon(name)
}

func (Theme) Size(name fyne.ThemeSizeName) float32 {
	switch name {
	case theme.SizeNamePadding:
		return 10
	case theme.SizeNameInlineIcon:
		return 18
	case theme.SizeNameText:
		return 14
	}
	return theme.DefaultTheme().Size(name)
}

func Section(title, subtitle string, body fyne.CanvasObject) fyne.CanvasObject {
	heading := widget.NewLabelWithStyle(title, fyne.TextAlignLeading, fyne.TextStyle{Bold: true})
	caption := widget.NewLabel(subtitle)
	caption.Importance = widget.MediumImportance
	header := container.NewVBox(heading, caption)
	return widget.NewCard("", "", container.NewBorder(header, nil, nil, nil, body))
}

func Pulse() fyne.CanvasObject {
	dot := canvas.NewCircle(color.NRGBA{R: 20, G: 184, B: 166, A: 255})
	dot.Resize(fyne.NewSize(12, 12))
	go func() {
		bright := true
		ticker := time.NewTicker(700 * time.Millisecond)
		defer ticker.Stop()
		for range ticker.C {
			fill := color.NRGBA{R: 15, G: 118, B: 110, A: 255}
			if bright {
				fill = color.NRGBA{R: 45, G: 212, B: 191, A: 255}
			}
			bright = !bright
			fyne.Do(func() {
				dot.FillColor = fill
				canvas.Refresh(dot)
			})
		}
	}()
	return container.NewGridWrap(fyne.NewSize(12, 12), dot)
}

func Meta(text string) *widget.Label {
	label := widget.NewLabel(text)
	label.Importance = widget.MediumImportance
	return label
}

func CompactMeta(text string) *widget.Label {
	label := widget.NewLabel(text)
	label.Importance = widget.MediumImportance
	return label
}

func Ellipsize(text string, maxRunes int) string {
	if maxRunes <= 0 || utf8.RuneCountInString(text) <= maxRunes {
		return text
	}
	if maxRunes <= 3 {
		return "..."
	}
	runes := []rune(text)
	return string(runes[:maxRunes-3]) + "..."
}
