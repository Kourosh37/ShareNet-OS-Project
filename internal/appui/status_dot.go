package appui

import (
	"image/color"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/widget"
)

type StatusDot struct {
	widget.BaseWidget
	running bool
	bright  bool
}

func NewStatusDot(running bool) *StatusDot {
	d := &StatusDot{running: running, bright: true}
	d.ExtendBaseWidget(d)
	go d.animate()
	return d
}

func (d *StatusDot) SetRunning(running bool) {
	d.running = running
	if !running {
		d.bright = true
	}
	d.Refresh()
}

func (d *StatusDot) CreateRenderer() fyne.WidgetRenderer {
	circle := canvas.NewCircle(statusDotColor(d.running, d.bright))
	return &statusDotRenderer{dot: d, circle: circle, objects: []fyne.CanvasObject{circle}}
}

type statusDotRenderer struct {
	dot     *StatusDot
	circle  *canvas.Circle
	objects []fyne.CanvasObject
}

func (r *statusDotRenderer) Layout(size fyne.Size) {
	r.circle.Resize(size)
}

func (r *statusDotRenderer) MinSize() fyne.Size {
	return fyne.NewSize(10, 10)
}

func (r *statusDotRenderer) Refresh() {
	r.circle.FillColor = statusDotColor(r.dot.running, r.dot.bright)
	r.Layout(r.dot.Size())
	canvas.Refresh(r.circle)
}

func (r *statusDotRenderer) Objects() []fyne.CanvasObject {
	return r.objects
}

func (r *statusDotRenderer) Destroy() {}

func (d *StatusDot) animate() {
	ticker := time.NewTicker(650 * time.Millisecond)
	defer ticker.Stop()
	for range ticker.C {
		if !d.running {
			continue
		}
		fyne.Do(func() {
			d.bright = !d.bright
			d.Refresh()
		})
	}
}

func statusDotColor(running, bright bool) color.Color {
	if running && bright {
		return color.NRGBA{R: 34, G: 197, B: 94, A: 255}
	}
	if running {
		return color.NRGBA{R: 22, G: 101, B: 52, A: 255}
	}
	return color.NRGBA{R: 248, G: 113, B: 113, A: 255}
}
