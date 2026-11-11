import UIKit

// рисование Hymenaeus по макету: небо и облака, кольца со звёздочками нот, солнце.
// Неподвижное (небо, два облака, ободки колец) — в кеше «стена»; живое — каждый кадр, без градиентов кроме спрайта glowAt и без shadowBlur.
final class HymDrawState {
  struct WallKey: Equatable { var W, H, sc: CGFloat }
  var wall: CGImage?, wallKey: WallKey?, wallBuilds = 0
  struct CircKey: Hashable { var cx, cy, sc: CGFloat; var seed: Int? }
  var circles: [CircKey: [(x: CGFloat, y: CGFloat, r: CGFloat)]] = [:]       // круги облаков — чистая геометрия, считаем один раз
  struct LensKey: Equatable { var cx, cy, sc, dens, W, H: CGFloat }
  var lensKey: LensKey?, lensClipA: CGPath?, lensClipB: CGPath?, lensHatch: CGPath?   // линза: два клипа и штриховка, пересобираются при смене плотности
  var lensBuilds = 0
}

private struct HymRnd {                                  // тот же генератор, что seed * 16807 % 2147483647 в макете
  var s: Int
  init(_ seed: Int) { s = max(1, seed) }
  mutating func next() -> CGFloat { s = (s * 16807) % 2147483647; return CGFloat(s - 1) / 2147483646 }
}

private let tau = CGFloat.pi * 2

extension HymenaeusView {
  var wallBuilds: Int { dr.wallBuilds }
  var lensBuilds: Int { dr.lensBuilds }

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
  private typealias Circles = [(x: CGFloat, y: CGFloat, r: CGFloat)]
  private func cloudCircles(_ cx: CGFloat, _ cy: CGFloat, _ scale: CGFloat, seed: Int? = nil) -> Circles {
    let key = HymDrawState.CircKey(cx: cx, cy: cy, sc: scale, seed: seed)
    if let c = dr.circles[key] { return c }
    if dr.circles.count > 32 { dr.circles.removeAll() }
    var rnd = HymRnd(seed ?? Int((cx * 7 + cy).rounded()))
    var C: Circles = []
    let n = 7
    for i in 0..<n {
      let t = CGFloat(i) / CGFloat(n - 1) - 0.5
      C.append((cx + t * 120 * scale, cy - cos(t * .pi) * 14 * scale + (rnd.next() - 0.5) * 8 * scale, (17 + cos(t * .pi) * 15 + rnd.next() * 6) * scale))
    }
    C.append((cx - 25 * scale, cy + 8 * scale, 20 * scale)); C.append((cx + 22 * scale, cy + 9 * scale, 19 * scale))
    dr.circles[key] = C
    return C
  }

  private func cloud(_ g: CGContext, _ cx: CGFloat, _ cy: CGFloat, _ scale: CGFloat, _ a: CGFloat, hatch: Bool = true, frozen: Bool = false, seed: Int? = nil) {
    let C = cloudCircles(cx, cy, scale, seed: seed)
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


  // два облака макета C: A (левое, с тенью) и B (правое); пересечение — линза, её штриховка = число гранул
  private var cloudA: CGPoint { geo.pt(185 - 34 * 1.3, 175 - 6 * 1.3) }
  private var cloudB: CGPoint { geo.pt(185 + 34 * 1.3, 175 + 8 * 1.3) }
  private var cloudSc: CGFloat { 1.3 * geo.S }

  // стена: фон, звёзды неба, два облака и два переплетённых обода; ключ — W×H×плотность
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
    for _ in 0..<50 {                                                  // звёзды — только над нитями (в облаках и кольцах их не видно)
      let x = rnd.next() * G.W, y = 70 * G.S + rnd.next() * (G.H * 0.3), r: CGFloat = rnd.next() < 0.1 ? 0.9 : 0.5
      g.setFillColor(col(Hym.ink, 0.12 + rnd.next() * 0.28)); disc(g, x, y, r)
    }
    cloud(g, cloudA.x, cloudA.y, cloudSc, 0.8); cloud(g, cloudB.x, cloudB.y, cloudSc, 0.8, hatch: false)
    let a = G.ringCenter(0), b = G.ringCenter(1), R = G.ringR(0), w = HymGeo.ringW * G.S
    band(g, a.x, a.y, R, w, 0.95)
    band(g, b.x, b.y, G.ringR(1), w, 0.8)                              // струна поверх резонатора справа внизу, слева наверху — наоборот
    let dx = (b.x - a.x) / 2, hy = (R * R - dx * dx).squareRoot()
    g.saveGState(); g.beginPath(); g.addEllipse(in: CGRect(x: a.x + dx - w * 1.8, y: a.y - hy - w * 1.8, width: w * 3.6, height: w * 3.6)); g.clip()
    band(g, a.x, a.y, R, w, 0.95); g.restoreGState()
    dr.wall = g.makeImage()
    return dr.wall
  }

  private func drawClouds(_ d: Draw, _ now: Double) {
    let g = d.g, G = geo, S = G.S, m = mirror
    let pos = CGFloat(m.cloud[Hym.Cloud.pos]), size = CGFloat(m.cloud[Hym.Cloud.size]), dens = CGFloat(m.cloud[Hym.Cloud.dens])
    let echo = CGFloat(m.cloud[Hym.Cloud.echo]), e = CGFloat(min(1, m.cloudEnergy)), sc = cloudSc, B = cloudB
    if echo > 0.03 {                                     // призраки эха — правее и бледнее второго облака
      let k = min(1, echo * 2), dx = 18 * 1.3 * S * k
      cloud(g, B.x + dx, B.y + 2 * S, sc, 0.16 * k, hatch: false, seed: Int((B.x * 7 + B.y).rounded()))
      cloud(g, B.x + 2 * dx, B.y + 4 * S, sc, 0.07 * k, hatch: false, seed: Int((B.x * 7 + B.y).rounded()))
    }
    if m.cloud[Hym.Cloud.freeze] > 0.5 { cloud(g, cloudA.x, cloudA.y, sc, 0.65, frozen: true) }
    // линза пересечения: штриховка, чем гуще densitas — тем чаще штрихи (площадь линзы — число гранул)
    let key = HymDrawState.LensKey(cx: cloudA.x, cy: cloudA.y, sc: sc, dens: dens, W: G.W, H: G.H)
    if dr.lensKey != key {
      dr.lensKey = key; dr.lensBuilds += 1
      func ell(_ C: Circles) -> CGPath { let p = CGMutablePath(); for c in C { p.addEllipse(in: CGRect(x: c.x - c.r, y: c.y - c.r, width: 2 * c.r, height: 2 * c.r)) }; return p }
      dr.lensClipA = ell(cloudCircles(cloudA.x, cloudA.y, sc)); dr.lensClipB = ell(cloudCircles(B.x, B.y, sc))
      let h = CGMutablePath(), lens = G.lensRect.insetBy(dx: -20 * S, dy: -20 * S), step = (6 - dens * 3.6) * S
      var y = lens.minY
      while y < lens.maxY { h.move(to: CGPoint(x: lens.minX, y: y)); h.addLine(to: CGPoint(x: lens.maxX, y: y)); y += step }
      dr.lensHatch = h
    }
    if let ca = dr.lensClipA, let cb = dr.lensClipB, let h = dr.lensHatch {
      g.saveGState()
      g.beginPath(); g.addPath(ca); g.clip(); g.beginPath(); g.addPath(cb); g.clip()
      g.setStrokeColor(col(Hym.ink, 0.5)); g.setLineWidth(0.8); g.beginPath(); g.addPath(h); g.strokePath()
      g.restoreGState()
    }
    let px = G.pt(40 + pos * 310, 0).x, y0 = G.pt(0, 108).y, y1 = G.pt(0, 244).y        // позиция чтения: нить сверху вниз через облака
    if e > 0.02 { d.glowAt(px, G.pt(0, 175).y, 70 * S, e * 0.35) }   // облако дышит громкостью
    g.setLineDash(phase: 0, lengths: [2, 4]); g.setStrokeColor(col(Hym.ink, 0.35)); g.setLineWidth(1)
    g.beginPath(); g.move(to: CGPoint(x: px, y: y0)); g.addLine(to: CGPoint(x: px, y: y1)); g.strokePath(); g.setLineDash(phase: 0, lengths: [])
    g.saveGState(); g.translateBy(x: px, y: y0); g.rotate(by: .pi / 4)
    g.setStrokeColor(col(Hym.ink, 0.8)); g.setLineWidth(1.1); g.stroke(CGRect(x: -3.5, y: -3.5, width: 7, height: 7)); g.restoreGState()
    g.setStrokeColor(col(Hym.ink, 0.5)); g.setLineWidth(1)                 // окно зерна — скобки вокруг нити
    let hw = (8 + size * 28) * S, by0 = G.pt(0, 150).y, by1 = G.pt(0, 214).y
    for sgn: CGFloat in [-1, 1] {
      let x = px + sgn * hw
      g.beginPath(); g.move(to: CGPoint(x: x - sgn * 4, y: by0)); g.addLine(to: CGPoint(x: x, y: by0)); g.addLine(to: CGPoint(x: x, y: by1)); g.addLine(to: CGPoint(x: x - sgn * 4, y: by1)); g.strokePath()
    }
  }

  // mix: волосяная линия с насечками и бегунок; внутри бегунка золотая дуга — доля сухого
  private func drawMix(_ d: Draw) {
    let g = d.g, G = geo, S = G.S, y = G.pt(0, HymGeo.mixY).y, x0 = G.pt(HymGeo.mixX0, 0).x, x1 = G.pt(HymGeo.mixX1, 0).x, t = CGFloat(mirror.mix)
    g.setStrokeColor(col(Hym.ink, 0.4)); g.setLineWidth(1); g.beginPath(); g.move(to: CGPoint(x: x0, y: y)); g.addLine(to: CGPoint(x: x1, y: y))
    for i in 0...4 { let x = x0 + (x1 - x0) * CGFloat(i) / 4; g.move(to: CGPoint(x: x, y: y - 3 * S)); g.addLine(to: CGPoint(x: x, y: y + 3 * S)) }
    g.strokePath()
    let kx = x0 + (x1 - x0) * t
    g.setFillColor(theme.bg.cgColor); disc(g, kx, y, 8 * S)
    g.setStrokeColor(col(Hym.ink, 0.9)); g.setLineWidth(1.2); circle(g, kx, y, 8 * S)
    g.setStrokeColor(col(Hym.gold, 0.9)); g.setLineWidth(1); g.beginPath()
    g.addArc(center: CGPoint(x: kx, y: y), radius: 3.2 * S, startAngle: 0, endAngle: tau * (1 - t * 0.8), clockwise: false); g.strokePath()
  }

  // ноты в полёте: от точки над своим кольцом (правее — выше нота) к месту удара на ободе, пунктир золотом
  private func drawStrikes(_ d: Draw, _ now: Double) {
    let g = d.g, G = geo
    func start(_ r: Int, _ nt: Note) -> CGPoint { G.launchPoint(r, pitch: nt.pitch) }
    for r in 0..<Hym.nr {
      let c = G.ringCenter(r), ang = CGFloat(ringHitPos[r]) * tau - .pi / 2, rr = G.ringR(r) + HymGeo.ringW * G.S / 2
      let to = CGPoint(x: c.x + cos(ang) * rr, y: c.y + sin(ang) * rr)
      let flash = max(0, 1 - CGFloat(now - ringHit[r]) / 0.9)
      var newest: Note?
      for nt in notes where nt.ring == r && nt.pearl >= 0 {
        let age = now - nt.born
        guard age >= 0 && age < HymenaeusView.noteLife else { continue }
        if newest == nil || nt.born > newest!.born { newest = nt }
        let from = start(r, nt)
        let t = CGFloat(age / HymenaeusView.noteLife), p = CGPoint(x: from.x + (to.x - from.x) * min(1, t * 2.2), y: from.y + (to.y - from.y) * min(1, t * 2.2))
        star4(g, p.x, p.y, 3 + 3 * (1 - t), col(Hym.gold, 0.9 * (1 - t * 0.7)))
      }
      if flash > 0.02, let nt = newest {                  // пунктир — от точки старта последней ноты
        let from = start(r, nt)
        g.setLineDash(phase: 0, lengths: [1.5, 3]); g.setStrokeColor(col(Hym.gold, 0.85 * flash)); g.setLineWidth(1.1)
        g.beginPath(); g.move(to: from); g.addLine(to: to); g.strokePath(); g.setLineDash(phase: 0, lengths: [])
      }
    }
  }

  private func drawRings(_ d: Draw, _ now: Double) {
    let g = d.g, G = geo
    for r in 0..<Hym.nr {
      let c = G.ringCenter(r), R = G.ringR(r), w = HymGeo.ringW * G.S, st = mirror.rings[r].structure
      let e = CGFloat(min(1, mirror.rings[r].energy)), flash = max(0, 1 - CGFloat(now - ringHit[r]) / 0.7)
      let lit = max(e, flash), inner = R - w / 2
      if r == 0 {                                         // резонатор: моды — концентрические волны (structure), всегда едва видны
        let modes = 1 + Int(st * 3.99)
        for i in 1...modes {
          g.setStrokeColor(col(Hym.gold, (0.1 + 0.3 * min(1, lit * 1.5)) / CGFloat(i))); g.setLineWidth(0.8)
          circle(g, c.x, c.y, inner - CGFloat(i) * 7 * G.S)
        }
      } else {                                            // струна: три волнистые хорды, дрожат от звона
        let amp = (1.2 + lit * 2.5) * G.S, ph = CGFloat(now) * 18 * lit
        g.setStrokeColor(col(Hym.gold, 0.12 + 0.3 * min(1, lit * 1.5))); g.setLineWidth(0.8)
        for k in -1...1 {
          let yy = c.y + CGFloat(k) * inner * 0.55 * (0.5 + st * 0.5), half = max(0, inner * inner - (yy - c.y) * (yy - c.y)).squareRoot() - 3 * G.S
          g.beginPath()
          for j in 0...30 {
            let u = CGFloat(j) / 30, x = c.x - half + 2 * half * u, y = yy + sin(u * 14 + CGFloat(k) + ph) * amp
            if j > 0 { g.addLine(to: CGPoint(x: x, y: y)) } else { g.move(to: CGPoint(x: x, y: y)) }
          }
          g.strokePath()
        }
      }
      if flash > 0 {                                      // место удара на ободе: вспышка и звёздочка
        let ang = CGFloat(ringHitPos[r]) * tau - .pi / 2, hx = c.x + cos(ang) * (R + w / 2), hy = c.y + sin(ang) * (R + w / 2)
        d.glowAt(hx, hy, 22 * G.S, flash * 0.35)
        star4(g, hx, hy, 6, col(Hym.gold, 0.95 * flash))
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
    drawMix(d)
    drawStrikes(d, now)
    drawRings(d, now)
    for l in activeLabels { d.mapText(l.text, l.at.x, l.at.y, 12, 0.85, align: .center) }       // подписи — только пока жест идёт
    if holds.values.contains(where: { $0.zone == HymHit.mix }) {                                  // концы mix подписаны, пока ведёшь бегунок
      let y = G.pt(0, HymGeo.mixY).y + 22 * G.S
      d.mapText("sicca", G.pt(HymGeo.mixX0, 0).x, y, 12, 0.7, align: .center); d.mapText("umida", G.pt(HymGeo.mixX1, 0).x, y, 12, 0.7, align: .center)
    }
    guard sun else { return }
    d.quarterSun(G.sun.x, G.sun.y, G.sun.r, side: 1, now: now * 1000, col: Hym.gold, disc: theme.bg)
  }
}
