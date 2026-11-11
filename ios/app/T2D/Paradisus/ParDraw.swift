import UIKit

// рисование Paradisus (порт draw из worlds/paradisus/view.js): слоновая кость и золото, девять сфер-эллипсов вокруг розы, ангелы штрихом.
// Неподвижное (лучи, сферы с точками, золотая точка розы) — в кеше «стена», свет в углу — отдельный малый слой; живое — каждый кадр,
// без градиентов и без shadowBlur.
enum ParLook {
  static let ink: RGB = (94, 69, 32), paper: RGB = (245, 239, 225)
  static let bg = UIColor(red: 245 / 255, green: 239 / 255, blue: 225 / 255, alpha: 1)
  static let gold: RGB = (176, 131, 44), light: RGB = (201, 154, 60)
  static let names = ["Primum Mobile", "Stellae", "Saturnus", "Iuppiter", "Mars", "Sol", "Venus", "Mercurius", "Luna"]   // изнутри наружу
  static let vow = ["a", "e", "i", "o", "u"]
  static let halo: [(rx: CGFloat, ry: CGFloat)] = [(6.5, 2.2), (5, 2.6), (2.4, 4.2), (3.6, 3.6), (2.2, 2.2)]            // нимб — форма гласной
  static func harm(_ k: Int) -> Int { 9 - min(8, max(0, k)) }
  static func vowelName(_ v: Double) -> String { vow[Int(v.rounded()) % 5] }
  static func name(_ k: Int) -> String { names[min(8, max(0, k))] }
}

final class ParDrawState {
  struct WallKey: Equatable { var W, H, cy, sc: CGFloat }
  var wall: CGImage?, wallKey: WallKey?, wallBuilds = 0
  var light: CGImage?, lightKey: WallKey?
  var openness = 0.0                                     // раскрытие розы: растёт с хором
  struct Flash { var t: Double; var x, y: CGFloat }      // вспышка над ангелом, когда над ним прошла планета
  struct Grain { var t: Double; var a, r: CGFloat }      // зерно на розе
  var flashes: [Flash] = [], glitter: [Grain] = []
  static let flashLife = 1.2, grainLife = 0.9, flashMax = 16, grainMax = 48
}

private let tau = CGFloat.pi * 2

extension ParadisusView {
  var wallBuilds: Int { dr.wallBuilds }

  private func col(_ c: RGB, _ a: CGFloat) -> CGColor { Draw.color(c, a) }
  private func ell(_ g: CGContext, _ x: CGFloat, _ y: CGFloat, _ rx: CGFloat, _ ry: CGFloat) { g.strokeEllipse(in: CGRect(x: x - rx, y: y - ry, width: 2 * rx, height: 2 * ry)) }
  private func disc(_ g: CGContext, _ x: CGFloat, _ y: CGFloat, _ r: CGFloat) { g.fillEllipse(in: CGRect(x: x - r, y: y - r, width: 2 * r, height: 2 * r)) }

  private func layer(_ w: CGFloat, _ h: CGFloat, _ paint: (CGContext) -> Void) -> CGImage? {
    let sc = Draw.scale
    guard let g = CGContext(data: nil, width: Int((w * sc).rounded(.up)), height: Int((h * sc).rounded(.up)), bitsPerComponent: 8, bytesPerRow: 0,
                            space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue) else { return nil }
    g.translateBy(x: 0, y: CGFloat(g.height)); g.scaleBy(x: sc, y: -sc)
    paint(g)
    return g.makeImage()
  }

  // фон: слоновая кость, лучи от розы, сферы с точками-звёздами, золотая точка розы
  func wallImage() -> CGImage? {
    let G = geo, key = ParDrawState.WallKey(W: G.W, H: G.H, cy: G.cy, sc: Draw.scale)
    if let w = dr.wall, key == dr.wallKey { return w }
    dr.wallKey = key; dr.wallBuilds += 1
    dr.wall = layer(G.W, G.H) { g in
      g.setFillColor(ParLook.bg.cgColor); g.fill(CGRect(x: 0, y: 0, width: G.W, height: G.H))
      g.setLineWidth(1); g.setStrokeColor(col(ParLook.light, 0.14)); g.beginPath()
      for j in 0..<36 {
        let a = CGFloat(j) / 36 * tau
        g.move(to: CGPoint(x: G.cx + cos(a) * 40, y: G.cy + sin(a) * 64)); g.addLine(to: CGPoint(x: G.cx + cos(a) * 430, y: G.cy + sin(a) * 690))
      }
      g.strokePath()
      for (k, s) in G.sph.enumerated() {
        g.setStrokeColor(col(ParLook.ink, 0.62 - 0.03 * CGFloat(k))); ell(g, G.cx, G.cy, s.rx, s.ry)
        g.setFillColor(col(ParLook.ink, 0.35))
        let n = 10 + k * 2
        for j in 0..<n { let a = CGFloat(j) / CGFloat(n) * tau + CGFloat(k) * 0.3; disc(g, G.cx + cos(a) * s.rx, G.cy + sin(a) * s.ry, 0.9) }
      }
      g.setStrokeColor(col(ParLook.gold, 1)); ell(g, G.cx, G.cy, 4.5, 4.5)
      g.setFillColor(col(ParLook.gold, 1)); disc(g, G.cx, G.cy, 1.6)
    }
    return dr.wall
  }

  // божественный свет: четверть сияния из правого нижнего угла (он же кнопка карты)
  private func lightImage() -> (img: CGImage, rect: CGRect)? {
    let G = geo, L = G.light, side = L.r * 1.6, key = ParDrawState.WallKey(W: G.W, H: G.H, cy: G.cy, sc: Draw.scale)
    let rect = CGRect(x: L.x - side, y: L.y - side, width: side, height: side)
    if dr.lightKey != key || dr.light == nil {
      dr.lightKey = key
      dr.light = layer(side, side) { g in
        g.translateBy(x: side, y: side)                                      // начало — угол света, правый нижний угол слоя
        g.setLineWidth(1); g.setStrokeColor(col(ParLook.gold, 0.7)); g.beginPath()
        for j in 0..<14 {
          let a = CGFloat.pi + (CGFloat(j) + 0.5) / 14 * (CGFloat.pi / 2), l = j % 2 == 1 ? L.r * 1.5 : L.r * 1.12
          g.move(to: CGPoint(x: cos(a) * 30, y: sin(a) * 30)); g.addLine(to: CGPoint(x: cos(a) * l, y: sin(a) * l))
        }
        g.strokePath()
        for (r, op) in [(30.0, 0.9), (38, 0.45), (58, 0.25), (84, 0.14)] as [(CGFloat, CGFloat)] {
          g.setStrokeColor(col(ParLook.gold, op)); g.beginPath()
          g.addArc(center: .zero, radius: r, startAngle: .pi, endAngle: .pi * 1.5, clockwise: false); g.strokePath()
        }
      }
    }
    guard let img = dr.light else { return nil }
    return (img, rect)
  }

  // ангел штрихом: крылья, тело, голова; поёт — темнее
  private func angel(_ g: CGContext, _ x: CGFloat, _ y: CGFloat, sing: CGFloat, held: Bool) {
    let al = 0.45 + 0.55 * sing
    if held { g.setStrokeColor(col(ParLook.light, 0.28)); g.setLineWidth(6); ell(g, x, y - 3, 15, 17) }
    g.setStrokeColor(col(ParLook.ink, al)); g.setLineWidth(1.1); g.beginPath()
    for s: CGFloat in [-1, 1] {
      g.move(to: CGPoint(x: x, y: y - 5))
      g.addCurve(to: CGPoint(x: x + s * 16, y: y - 3), control1: CGPoint(x: x + s * 6, y: y - 14), control2: CGPoint(x: x + s * 15, y: y - 10))
      g.addCurve(to: CGPoint(x: x, y: y - 1), control1: CGPoint(x: x + s * 11, y: y - 6), control2: CGPoint(x: x + s * 6, y: y - 3))
      g.move(to: CGPoint(x: x + s * 12, y: y - 5)); g.addLine(to: CGPoint(x: x + s * 9, y: y - 3.5))
    }
    g.move(to: CGPoint(x: x - 2.2, y: y - 1)); g.addLine(to: CGPoint(x: x, y: y + 7)); g.addLine(to: CGPoint(x: x + 2.2, y: y - 1))
    g.strokePath()
    g.setFillColor(col(ParLook.ink, al)); disc(g, x, y - 6.5, 2)
  }

  // нимб над головой: форма гласной (перетекает между соседними), двойной у закреплённого
  private func halo(_ g: CGContext, _ x: CGFloat, _ y: CGFloat, v: Double, double: Bool, a: CGFloat) {
    let i = Int(v.rounded(.down)) % 5, j = (i + 1) % 5, t = CGFloat(v - v.rounded(.down))
    let A = ParLook.halo[i], B = ParLook.halo[j], rx = A.rx + (B.rx - A.rx) * t, ry = A.ry + (B.ry - A.ry) * t
    g.setStrokeColor(col(ParLook.gold, a)); g.setLineWidth(1.2); ell(g, x, y - 10, rx, ry)
    if double { g.setStrokeColor(col(ParLook.gold, a * 0.5)); g.setLineWidth(1); ell(g, x, y - 10, rx + 2.6, ry + 2.6) }
  }

  private func drawRose(_ g: CGContext, _ now: Double) {
    let G = geo, o = CGFloat(dr.openness)
    let tiers: [(r: CGFloat, n: Int, op: CGFloat)] = [(1, 16, 0.5), (0.76, 12, 0.65), (0.53, 9, 0.8), (0.3, 6, 0.9)]
    for (t, tier) in tiers.enumerated() {
      let n = tier.n, R = G.rose * tier.r * (0.88 + 0.28 * o)
      g.setStrokeColor(col(ParLook.ink, tier.op)); g.setLineWidth(1); g.beginPath()
      for j in 0..<n {
        let a = CGFloat(j) / CGFloat(n) * tau + CGFloat(t) * 0.2 + o * 0.15 * (t % 2 == 1 ? 1 : -1)
        let ri = R * (0.62 - 0.12 * o), sp = CGFloat.pi / CGFloat(n) * (0.8 + 0.3 * o), a1 = a - sp, a2 = a + sp
        g.move(to: CGPoint(x: G.cx + cos(a1) * ri, y: G.cy + sin(a1) * ri * 1.12))
        g.addQuadCurve(to: CGPoint(x: G.cx + cos(a2) * ri, y: G.cy + sin(a2) * ri * 1.12),
                       control: CGPoint(x: G.cx + cos(a) * (R + 3), y: G.cy + sin(a) * (R + 3) * 1.12))
      }
      g.strokePath()
    }
  }

  func frame(_ d: Draw, _ now: Double, light: Bool) {
    let g = d.g, G = geo
    d.blit(wallImage())
    let fx = mirror.fx
    if fx[0] > 0.05 {                                                        // shimmer — лучи от розы ярче
      g.setStrokeColor(col(ParLook.light, CGFloat(fx[0] * 0.25 * (0.8 + 0.2 * sin(now * 2))))); g.setLineWidth(1); g.beginPath()
      for j in 0..<18 {
        let a = CGFloat(j) / 18 * tau + CGFloat(now * 0.05)
        g.move(to: CGPoint(x: G.cx + cos(a) * 44, y: G.cy + sin(a) * 70)); g.addLine(to: CGPoint(x: G.cx + cos(a) * 300, y: G.cy + sin(a) * 480))
      }
      g.strokePath()
    }
    drawRose(g, now)
    if fx[7] > 0.01 {                                                        // планеты на сферах
      let al = CGFloat(0.35 + 0.5 * min(1, fx[7] * 2))
      for (k, sp) in G.sph.enumerated() {
        let u = CGFloat(mirror.planet(k)) * tau, x = G.cx + sin(u) * sp.rx, y = G.cy - cos(u) * sp.ry
        g.setFillColor(ParLook.bg.cgColor); disc(g, x, y, 4)
        g.setStrokeColor(col(ParLook.gold, al)); g.setLineWidth(1); ell(g, x, y, 4, 4)
        g.setFillColor(col(ParLook.gold, al)); disc(g, x, y, 1.6)
      }
    }
    for q in dr.flashes {                                                    // звон планеты над ангелом
      let k = CGFloat(1 - (now - q.t) / ParDrawState.flashLife)
      if k <= 0 { continue }
      g.setStrokeColor(col(ParLook.light, 0.6 * k)); g.setLineWidth(1); g.beginPath()
      for j in 0..<8 {
        let a = CGFloat(j) / 8 * tau, r1 = 14 + 14 * (1 - k)
        g.move(to: CGPoint(x: q.x + cos(a) * 14, y: q.y - 8 + sin(a) * 14)); g.addLine(to: CGPoint(x: q.x + cos(a) * r1, y: q.y - 8 + sin(a) * r1))
      }
      g.strokePath()
    }
    for q in dr.glitter {                                                    // зёрна Gloria вспыхивают на розе
      let k = CGFloat(1 - (now - q.t) / ParDrawState.grainLife)
      if k <= 0 { continue }
      let r = G.rose * (0.3 + q.r * 0.9)
      g.setFillColor(col(ParLook.gold, 0.9 * k)); disc(g, G.cx + cos(q.a) * r, G.cy + sin(q.a) * r * 1.12, 1.2 + k)
    }
    let held = Set(holds.values.compactMap { $0.kind == .none ? nil : $0.id })
    for a in mirror.angels {
      let p = G.angelPos(a.k, a.v)
      g.saveGState(); g.translateBy(x: p.x, y: p.y); g.scaleBy(x: 1.35, y: 1.35)                      // крупнее штриха сфер — под палец и на глаз
      angel(g, 0, 0, sing: CGFloat(a.lvl), held: held.contains(a.id))
      halo(g, 0, 0, v: a.vEff, double: a.pinned, a: CGFloat(0.5 + 0.5 * a.lvl))
      g.restoreGState()
    }
    for l in activeLabels {                                                  // подписи — только пока трогаешь
      d.mapText(l.title, l.at.x, l.at.y - 6, 15, 0.95, align: l.align)
      d.mapText(l.sub, l.at.x, l.at.y + 11, 12, 0.75, align: l.align)
    }
    guard light, let (img, r) = lightImage() else { return }
    g.saveGState(); g.translateBy(x: r.minX, y: r.maxY); g.scaleBy(x: 1, y: -1)
    g.draw(img, in: CGRect(x: 0, y: 0, width: r.width, height: r.height)); g.restoreGState()
    if mirror.level > 0.02 {                                                // свет дышит громкостью: внешнее кольцо ярче
      g.setStrokeColor(col(ParLook.gold, CGFloat(min(1, mirror.level)) * 0.55)); g.setLineWidth(1); g.beginPath()
      g.addArc(center: CGPoint(x: G.light.x, y: G.light.y), radius: 84 + 6 * CGFloat(mirror.level), startAngle: .pi, endAngle: .pi * 1.5, clockwise: false)
      g.strokePath()
    }
  }
}
