import UIKit

// рисование Hymenaeus по макету: небо и облака, нити звука, кольца, жемчужная нить, солнце.
// Неподвижное (небо, два облака, ободки колец) — в кеше «стена»; живое — каждый кадр, без градиентов кроме спрайта glowAt и без shadowBlur.
final class HymDrawState {
  struct WallKey: Equatable { var W, H, sc: CGFloat }
  var wall: CGImage?, wallKey: WallKey?, wallBuilds = 0
}

private struct HymRnd {                                  // тот же генератор, что seed * 16807 % 2147483647 в макете
  var s: Int
  init(_ seed: Int) { s = max(1, seed) }
  mutating func next() -> CGFloat { s = (s * 16807) % 2147483647; return CGFloat(s - 1) / 2147483646 }
}

private let tau = CGFloat.pi * 2

extension HymenaeusView {
  var wallBuilds: Int { dr.wallBuilds }

  private func col(_ c: RGB, _ a: CGFloat) -> CGColor { Draw.color(c, a) }
  private func circle(_ g: CGContext, _ x: CGFloat, _ y: CGFloat, _ r: CGFloat) { g.strokeEllipse(in: CGRect(x: x - r, y: y - r, width: 2 * r, height: 2 * r)) }
  private func disc(_ g: CGContext, _ x: CGFloat, _ y: CGFloat, _ r: CGFloat) { g.fillEllipse(in: CGRect(x: x - r, y: y - r, width: 2 * r, height: 2 * r)) }

  // звёздочка-гравюра: нота в полёте
  private func star4(_ g: CGContext, _ x: CGFloat, _ y: CGFloat, _ r: CGFloat, _ c: CGColor) {
    g.setStrokeColor(c); g.setLineWidth(1); g.beginPath()
    g.move(to: CGPoint(x: x - r, y: y)); g.addLine(to: CGPoint(x: x + r, y: y)); g.move(to: CGPoint(x: x, y: y - r)); g.addLine(to: CGPoint(x: x, y: y + r))
    g.move(to: CGPoint(x: x - r * 0.4, y: y - r * 0.4)); g.addLine(to: CGPoint(x: x + r * 0.4, y: y + r * 0.4))
    g.move(to: CGPoint(x: x - r * 0.4, y: y + r * 0.4)); g.addLine(to: CGPoint(x: x + r * 0.4, y: y - r * 0.4))
    g.strokePath()
  }

  // облако — объединение кругов: контур только там, где дуга не внутри соседа; снизу — штриховка-тень (макет cloud())
  private func cloud(_ g: CGContext, _ cx: CGFloat, _ cy: CGFloat, _ scale: CGFloat, _ a: CGFloat, hatch: Bool = true, frozen: Bool = false) {
    var rnd = HymRnd(Int((cx * 7 + cy).rounded()))
    var C: [(x: CGFloat, y: CGFloat, r: CGFloat)] = []
    let n = 7
    for i in 0..<n {
      let t = CGFloat(i) / CGFloat(n - 1) - 0.5
      C.append((cx + t * 120 * scale, cy - cos(t * .pi) * 14 * scale + (rnd.next() - 0.5) * 8 * scale, (17 + cos(t * .pi) * 15 + rnd.next() * 6) * scale))
    }
    C.append((cx - 25 * scale, cy + 8 * scale, 20 * scale)); C.append((cx + 22 * scale, cy + 9 * scale, 19 * scale))
    func inside(_ x: CGFloat, _ y: CGFloat, _ j: Int) -> Bool {
      for (k, c) in C.enumerated() where k != j && (x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) < c.r * c.r - 0.5 { return true }
      return false
    }
    func path() { g.beginPath(); for c in C { g.addEllipse(in: CGRect(x: c.x - c.r, y: c.y - c.r, width: 2 * c.r, height: 2 * c.r)) } }
    if hatch {                                           // облако закрывает всё под собой; призраки эха — прозрачные
      g.setFillColor(theme.bg.cgColor); path(); g.fillPath()
      g.saveGState(); path(); g.clip()
      let bot = C.map { $0.y + $0.r }.max() ?? cy, top = C.map { $0.y - $0.r }.min() ?? cy
      g.setStrokeColor(col(Hym.ink, a * (frozen ? 0.55 : 0.22))); g.setLineWidth(0.8)
      let step = (frozen ? 2.6 : 3.4) * max(1, scale)
      var y = frozen ? top : cy - 2 * scale
      g.beginPath()
      while y < bot {
        g.move(to: CGPoint(x: cx - 90 * scale, y: y)); g.addLine(to: CGPoint(x: cx + 90 * scale, y: y + (frozen ? 0 : 1.5)))
        y += step
      }
      g.strokePath()
      g.restoreGState()
    }
    g.setStrokeColor(col(Hym.ink, a)); g.setLineWidth(1.1); g.beginPath()
    for (j, c) in C.enumerated() {
      var on = false
      for k in 0...90 {
        let t = CGFloat(k) / 90 * tau, x = c.x + cos(t) * c.r, y = c.y + sin(t) * c.r
        if inside(x, y, j) { on = false; continue }
        let p = CGPoint(x: x, y: y)
        if on { g.addLine(to: p) } else { g.move(to: p) }
        on = true
      }
    }
    g.strokePath()
  }

  // зёрна — рисинки: короткие наклонные зёрнышки
  private func rice(_ g: CGContext, _ x: CGFloat, _ y: CGFloat, _ n: Int, _ spread: CGFloat, _ a: CGFloat) {
    var rnd = HymRnd(5)
    g.setLineWidth(1.1)
    for _ in 0..<n {
      let r = rnd.next().squareRoot() * spread, t = rnd.next() * tau, px = x + cos(t) * r * 1.3, py = y + sin(t) * r * 0.7
      g.saveGState(); g.translateBy(x: px, y: py); g.rotate(by: rnd.next() * .pi)
      g.setStrokeColor(col(Hym.ink, a * (0.5 + rnd.next() * 0.5)))
      g.strokeEllipse(in: CGRect(x: -2.4, y: -0.9, width: 4.8, height: 1.8)); g.restoreGState()
    }
  }

  // обод кольца: двойной контур, гравировка — свет сверху-слева, тень частой штриховкой (макет band())
  private func band(_ g: CGContext, _ cx: CGFloat, _ cy: CGFloat, _ R: CGFloat, _ w: CGFloat, _ a: CGFloat) {
    g.setFillColor(theme.bg.cgColor); g.beginPath()
    g.addEllipse(in: CGRect(x: cx - R - w / 2, y: cy - R - w / 2, width: 2 * R + w, height: 2 * R + w))
    g.addEllipse(in: CGRect(x: cx - R + w / 2, y: cy - R + w / 2, width: 2 * R - w, height: 2 * R - w))
    g.fillPath(using: .evenOdd)
    g.setStrokeColor(col(Hym.gold, a)); g.setLineWidth(1.3); circle(g, cx, cy, R + w / 2)
    g.setStrokeColor(col(Hym.gold, a * 0.85)); g.setLineWidth(1); circle(g, cx, cy, R - w / 2)
    g.setLineWidth(0.8)
    for k in 0..<160 {
      let t = CGFloat(k) / 160 * tau, sh = 0.5 + 0.5 * sin(t - 0.6)
      if k % (sh > 0.6 ? 1 : 3) != 0 { continue }
      g.setStrokeColor(col(Hym.gold, a * (0.15 + sh * 0.35)))
      g.beginPath()
      g.move(to: CGPoint(x: cx + cos(t) * (R - w / 2 + 1), y: cy + sin(t) * (R - w / 2 + 1)))
      g.addLine(to: CGPoint(x: cx + cos(t) * (R + w / 2 - 1), y: cy + sin(t) * (R + w / 2 - 1)))
      g.strokePath()
    }
  }

  // стена: фон, звёзды неба, два облака буфера и три ободка; ключ — W×H×плотность
  func wallImage() -> CGImage? {
    let sc = Draw.scale, G = geo
    let key = HymDrawState.WallKey(W: G.W, H: G.H, sc: sc)
    if let w = dr.wall, key == dr.wallKey { return w }
    dr.wallKey = key; dr.wallBuilds += 1
    guard let g = CGContext(data: nil, width: Int((G.W * sc).rounded()), height: Int((G.H * sc).rounded()), bitsPerComponent: 8, bytesPerRow: 0,
                            space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue) else { dr.wall = nil; return nil }
    g.translateBy(x: 0, y: CGFloat(g.height)); g.scaleBy(x: sc, y: -sc)
    g.setFillColor(theme.bg.cgColor); g.fill(CGRect(x: 0, y: 0, width: G.W, height: G.H))
    var rnd = HymRnd(11)
    for _ in 0..<70 {
      let x = rnd.next() * G.W, y = 70 * G.S + rnd.next() * (G.H * 0.62), r: CGFloat = rnd.next() < 0.1 ? 0.9 : 0.5
      g.setFillColor(col(Hym.ink, 0.15 + rnd.next() * 0.35)); disc(g, x, y, r)
    }
    let p1 = G.pt(150, 185), p2 = G.pt(285, 250)
    cloud(g, p1.x, p1.y, G.S, 0.55); cloud(g, p2.x, p2.y, 1.15 * G.S, 0.8)
    for i in 0..<Hym.nr {
      let c = G.ringCenter(i)
      band(g, c.x, c.y, G.ringRadius(i), HymGeo.ringW * G.S, i == 0 ? 0.95 : i == 1 ? 0.75 : 0.6)
    }
    dr.wall = g.makeImage()
    return dr.wall
  }

  private func drawClouds(_ d: Draw, _ now: Double) {
    let g = d.g, G = geo, S = G.S, m = mirror
    let pos = CGFloat(m.cloud[Hym.Cloud.pos]), size = CGFloat(m.cloud[Hym.Cloud.size]), dens = m.cloud[Hym.Cloud.dens]
    let echo = CGFloat(m.cloud[Hym.Cloud.echo]), e = CGFloat(min(1, m.cloudEnergy))
    if echo > 0.03 {                                     // призраки эха — правее и бледнее
      let p = G.pt(285, 250)
      cloud(g, p.x + 25 * S * echo * 2, p.y + 4 * S, 1.15 * S, 0.16 * echo * 2, hatch: false)
      cloud(g, p.x + 50 * S * echo * 2, p.y + 8 * S, 1.15 * S, 0.07 * echo * 2, hatch: false)
    }
    if m.cloud[Hym.Cloud.freeze] > 0.5 { let p = G.pt(140, 335); cloud(g, p.x, p.y, 0.85 * S, 0.65, frozen: true) }
    let px = G.pt(40 + pos * 310, 0).x, y0 = G.pt(0, 120).y, y1 = G.pt(0, 300).y
    if e > 0.02 { d.glowAt(px, G.pt(0, 250).y, 70 * S, e * 0.35) }   // облако дышит громкостью
    g.setLineDash(phase: 0, lengths: [2, 4]); g.setStrokeColor(col(Hym.ink, 0.35)); g.setLineWidth(1)
    g.beginPath(); g.move(to: CGPoint(x: px, y: y0)); g.addLine(to: CGPoint(x: px, y: y1)); g.strokePath(); g.setLineDash(phase: 0, lengths: [])
    g.saveGState(); g.translateBy(x: px, y: y0); g.rotate(by: .pi / 4)
    g.setStrokeColor(col(Hym.ink, 0.8)); g.setLineWidth(1.1); g.stroke(CGRect(x: -3.5, y: -3.5, width: 7, height: 7)); g.restoreGState()
    g.setStrokeColor(col(Hym.ink, 0.5)); g.setLineWidth(1)                 // окно зерна — скобки вокруг нити
    let hw = (8 + size * 28) * S, by0 = G.pt(0, 225).y, by1 = G.pt(0, 275).y
    for sgn: CGFloat in [-1, 1] {
      let x = px + sgn * hw
      g.beginPath(); g.move(to: CGPoint(x: x - sgn * 4, y: by0)); g.addLine(to: CGPoint(x: x, y: by0)); g.addLine(to: CGPoint(x: x, y: by1)); g.addLine(to: CGPoint(x: x - sgn * 4, y: by1)); g.strokePath()
    }
    rice(g, px, G.pt(0, 250).y, Int(8 + dens * 32), hw * 0.95, 0.8)
  }

  // нити звука: от каждого кольца к облаку, ноты в полёте
  private func threadPoint(_ r: Int, _ t: CGFloat, _ x1: CGFloat, _ y1: CGFloat) -> CGPoint {
    let c = geo.ringCenter(r), y0 = c.y - geo.ringRadius(r), x0 = c.x
    let w = sin(t * 9 + CGFloat(r + 1)) * 9 * t * geo.S
    return CGPoint(x: x0 + (x1 - x0) * t * t + w, y: y0 + (y1 - y0) * t)
  }

  private func drawThreads(_ d: Draw, _ now: Double) {
    let g = d.g, G = geo, px = G.pt(40 + CGFloat(mirror.cloud[Hym.Cloud.pos]) * 310, 0).x, y1 = G.pt(0, 300).y
    for r in 0..<Hym.nr {
      let a = CGFloat(0.28 + min(1, mirror.rings[r].energy) * 0.6) * (r == 1 ? 1 : 0.8)
      let x1 = px + CGFloat(r - 1) * 36 * G.S
      g.setStrokeColor(col(Hym.ink, a * 0.5)); g.setLineWidth(0.9); g.beginPath()
      for i in 0...40 { let p = threadPoint(r, CGFloat(i) / 40, x1, y1); if i > 0 { g.addLine(to: p) } else { g.move(to: p) } }
      g.strokePath()
      for n in notes where n.ring == r {
        let age = now - n.born
        guard age >= 0 && age < HymenaeusView.noteLife else { continue }
        let t = CGFloat(age / HymenaeusView.noteLife), p = threadPoint(r, t, x1, y1)
        star4(g, p.x, p.y, 4 + 3 * (1 - t), col(Hym.gold, 0.9 * (1 - t * 0.6)))
      }
    }
  }

  private func drawRings(_ d: Draw, _ now: Double) {
    let g = d.g, G = geo
    for r in 0..<Hym.nr {
      let c = G.ringCenter(r), R = G.ringRadius(r), w = HymGeo.ringW * G.S, st = mirror.rings[r].structure
      let e = CGFloat(min(1, mirror.rings[r].energy)), flash = max(0, 1 - CGFloat(now - ringHit[r]) / 0.7)
      let modes = 1 + Int(st * 3.99)                      // моды — концентрические волны внутри ударенного кольца (structure)
      let lit = max(e, flash)
      if lit > 0.02 {
        for i in 1...modes {
          g.setStrokeColor(col(Hym.gold, 0.28 / CGFloat(i) * min(1, lit * 1.5))); g.setLineWidth(0.8)
          circle(g, c.x, c.y, R - w / 2 - CGFloat(i) * 7 * G.S)
        }
      }
      if flash > 0 {                                      // место удара на ободе: вспышка и звёздочка
        let ang = CGFloat(ringHitPos[r]) * tau - .pi / 2, hx = c.x + cos(ang) * (R + w / 2), hy = c.y + sin(ang) * (R + w / 2)
        d.glowAt(hx, hy, 26 * G.S, flash * 0.5)
        star4(g, hx, hy, 7, col(Hym.gold, 0.95 * flash))
      }
    }
  }

  private func drawNecklace(_ d: Draw, _ now: Double) {
    let g = d.g, G = geo, m = mirror, S = G.S
    let cx = HymGeo.neckC.x, cy = HymGeo.neckC.y, rx = HymGeo.neckRx, ry = HymGeo.neckRy
    let gap0 = HymGeo.gap0, gap1 = HymGeo.gap1(m.dejavu)   // застёжка déjà vu
    func at(_ t: CGFloat) -> CGPoint { G.pt(cx + cos(t) * rx, cy + sin(t) * ry) }
    g.setStrokeColor(col(Hym.ink, 0.35)); g.setLineWidth(0.8); g.beginPath()
    for i in 0...200 { let p = at(gap1 + CGFloat(i) / 200 * (tau - (gap1 - gap0))); if i > 0 { g.addLine(to: p) } else { g.move(to: p) } }
    g.strokePath()
    g.setStrokeColor(col(Hym.ink, 0.6)); g.setLineWidth(0.9)
    for t in [gap0, gap1] { let p = at(t); circle(g, p.x, p.y, 2.6) }
    let n = max(1, m.len), played = m.playedIndex
    for i in 0..<n {
      let p = G.pearlPoint(i, n: n, dejavu: m.dejavu), q = m.pearls[i]
      if q.rest { g.setStrokeColor(col(Hym.ink, 0.25)); g.setLineWidth(0.8); circle(g, p.x, p.y, 2.2 * S); continue }
      let r = CGFloat(4.5 + q.pitch * 5) * S, cur = m.run && i == played
      g.setFillColor(theme.bg.cgColor); disc(g, p.x, p.y, r)
      g.setStrokeColor(col(Hym.ink, cur ? 0.95 : 0.55)); g.setLineWidth(1); circle(g, p.x, p.y, r)
      g.setFillColor(col(Hym.ink, cur ? 0.95 : 0.45)); disc(g, p.x - r * 0.35, p.y - r * 0.35, max(0.7, r * 0.22))   // блик жемчуга
      if cur {
        d.glowAt(p.x, p.y, 20 * S, 0.45)
        let rg = HymMirror.ringFor(q.pitch), c = G.ringCenter(rg), R = G.ringRadius(rg) + 5.5 * S, ang = 0.3 * tau - .pi / 2   // нить удара: к месту на ободе
        g.setLineDash(phase: 0, lengths: [1.5, 3]); g.setStrokeColor(col(Hym.gold, 0.55)); g.setLineWidth(0.9)
        g.beginPath(); g.move(to: p); g.addLine(to: CGPoint(x: c.x + cos(ang) * R, y: c.y + sin(ang) * R)); g.strokePath(); g.setLineDash(phase: 0, lengths: [])
      }
    }
  }

  func frame(_ d: Draw, _ now: Double, sun: Bool) {
    let g = d.g, G = geo
    if let w = wallImage() {
      g.saveGState(); g.translateBy(x: 0, y: G.H); g.scaleBy(x: 1, y: -1)
      g.draw(w, in: CGRect(x: 0, y: 0, width: G.W, height: G.H)); g.restoreGState()
    }
    drawClouds(d, now)
    drawThreads(d, now)
    drawNecklace(d, now)
    drawRings(d, now)
    for l in activeLabels { d.mapText(l.text, l.at.x, l.at.y, 12, 0.85, align: .center) }       // подписи — только пока жест идёт
    guard sun else { return }
    d.quarterSun(G.sun.x, G.sun.y, G.sun.r, side: 1, now: now * 1000, col: Hym.gold, disc: theme.bg)
  }
}
