import UIKit

// Tenebrae: плашка связи — тип модуляции (fm tzfm am rm) и cut фишками, глубина ползунком. Как плашка планеты, в стиле для чтения

private let linkLabels = Ten.linkTypes + ["cut"]

extension TenebraeView {
  func drawLinkPlate(_ p: NatPen, _ d: Draw) {
    guard let r = ring, let m = linkMod(r.dst, r.src), (0..<Ten.linkTypes.count).contains(m.type) else { return }
    let g = d.g, s = linkGeo, W = CGFloat(d.W), H = CGFloat(d.H), top = CGFloat(s.top)
    for i in [r.dst, r.src] where voice(i).active {                // на поле — кто в связи: приёмник ярче, источник бледнее
      let pos = screenPos(i)
      d.ring(pos.x, pos.y, tenPlanetR(voice(i).amp) + 9, i == r.dst ? 0.85 : 0.5, 1.4)
    }
    d.reading {
      g.setFillColor(d.paper(0.97)); g.fill(CGRect(x: 0, y: top, width: W, height: H - top))
      g.setStrokeColor(d.ink(0.35)); g.setLineWidth(1)
      g.beginPath(); g.move(to: CGPoint(x: 0, y: top + 0.5)); g.addLine(to: CGPoint(x: W, y: top + 0.5)); g.strokePath()
      g.setFillColor(d.ink(0.3)); g.fill(CGRect(x: W / 2 - 18, y: top + 5, width: 36, height: 2))
      let depth = tenClamp(m.depth)
      d.mapText("link · \(tenInt(Horo.jsRound(depth * 100)))%", s.x, s.top + 27, 16, 0.95)
      d.hairBox(s.close.minX, s.close.minY, s.close.width, s.close.height, 0.5)
      d.text("close", s.close.midX, s.close.midY, 12, 0.85, align: .center)
      for (k, c) in s.chips.enumerated() {
        let on = k == m.type
        if on { g.setFillColor(d.ink(0.12)); g.fill(c) }
        d.hairBox(c.minX, c.minY, c.width, c.height, on ? 0.9 : 0.4)
        d.text(linkLabels[k], c.midX, c.midY, 13, on ? 1 : 0.65, align: .center)
      }
      let y = s.sliderY
      g.setStrokeColor(d.ink(0.3)); g.setLineWidth(1)
      g.beginPath(); g.move(to: CGPoint(x: s.tx0, y: y)); g.addLine(to: CGPoint(x: s.tx1, y: y)); g.strokePath()
      g.setStrokeColor(d.ink(0.85)); g.setLineWidth(2)
      let vx = s.tx0 + depth * (s.tx1 - s.tx0)
      g.beginPath(); g.move(to: CGPoint(x: s.tx0, y: y)); g.addLine(to: CGPoint(x: vx, y: y)); g.strokePath()
      g.setStrokeColor(d.ink(0.35)); g.setLineWidth(1); g.beginPath()
      for k in 0...10 {
        let x = s.tx0 + Double(k) / 10 * (s.tx1 - s.tx0), l = k % 5 == 0 ? 5.0 : 2.5
        g.move(to: CGPoint(x: x, y: y - l)); g.addLine(to: CGPoint(x: x, y: y + l))
      }
      g.strokePath()
      d.ring(vx, y, 7, 1, 1.4)
    }
  }
}
