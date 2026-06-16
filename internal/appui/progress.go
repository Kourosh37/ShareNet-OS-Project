package appui

import (
	"image/color"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/widget"
)

type SlimProgress struct {
	widget.BaseWidget
	Value float64
}

func NewSlimProgress() *SlimProgress {
	p := &SlimProgress{}
	p.ExtendBaseWidget(p)
	return p
}

func (p *SlimProgress) SetValue(value float64) {
	if value < 0 {
		value = 0
	}
	if value > 1 {
		value = 1
	}
	p.Value = value
	p.Refresh()
}

func (p *SlimProgress) CreateRenderer() fyne.WidgetRenderer {
	bg := canvas.NewRectangle(color.NRGBA{R: 30, G: 41, B: 59, A: 255})
	fill := canvas.NewRectangle(color.NRGBA{R: 45, G: 212, B: 191, A: 255})
	return &slimProgressRenderer{progress: p, bg: bg, fill: fill, objects: []fyne.CanvasObject{bg, fill}}
}

type slimProgressRenderer struct {
	progress *SlimProgress
	bg       *canvas.Rectangle
	fill     *canvas.Rectangle
	objects  []fyne.CanvasObject
}

func (r *slimProgressRenderer) Layout(size fyne.Size) {
	r.bg.Resize(size)
	r.fill.Resize(fyne.NewSize(size.Width*float32(r.progress.Value), size.Height))
}

func (r *slimProgressRenderer) MinSize() fyne.Size {
	return fyne.NewSize(180, 7)
}

func (r *slimProgressRenderer) Refresh() {
	r.Layout(r.progress.Size())
	canvas.Refresh(r.progress)
}

func (r *slimProgressRenderer) Objects() []fyne.CanvasObject {
	return r.objects
}

func (r *slimProgressRenderer) Destroy() {}
