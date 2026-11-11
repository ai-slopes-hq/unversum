import CoreGraphics
import Foundation

// геометрия Paradisus по worlds/paradisus/view.js geo(): девять эллипсов-сфер вокруг розы, выше по экрану — шире;
// k = 0 внутри (у розы), k = 8 снаружи. Низ сцены — `H - safeBot - 8`: нижняя панель и корзина лежат ниже.
struct ParGeo {
  let W, H, top, bot, cx, cy, Ws: CGFloat
  let sph: [(rx: CGFloat, ry: CGFloat)]
  let rose: CGFloat
  let light: (x: CGFloat, y: CGFloat, r: CGFloat)   // свет в правом нижнем углу
  static let hitSphere: CGFloat = 30, hitAngel: CGFloat = 26

  init(size: CGSize, safeBot: CGFloat, topY: CGFloat = 0) {
    W = size.width; H = size.height
    top = topY + 6; bot = H - safeBot - 8
    let h = bot - top
    cx = W / 2; cy = top + h * 0.48; Ws = min(W, (H - safeBot) * 0.62)
    let RX: CGFloat = Ws / 2 - 14, RY: CGFloat = min(cy - top - 10, bot - cy - 10)
    let rx0: CGFloat = Ws * 0.13, ry0: CGFloat = rx0 * 1.6
    var s: [(rx: CGFloat, ry: CGFloat)] = []
    for k in 0..<Par.nSph {
      let f = CGFloat(k) / 8
      s.append((rx: rx0 + (RX - rx0) * f, ry: ry0 + (RY - ry0) * f))
    }
    sph = s
    rose = rx0 * 0.72
    light = (W, H - safeBot, min(100, Ws * 0.26))
  }

  var center: CGPoint { CGPoint(x: cx, y: cy) }
  func sphereRect(_ k: Int) -> CGRect { CGRect(x: cx - sph[k].rx, y: cy - sph[k].ry, width: sph[k].rx * 2, height: sph[k].ry * 2) }
  func inRose(_ p: CGPoint) -> Bool { hypot(p.x - cx, p.y - cy) < rose }

  // угол 0…2π от верха по часовой на сфере k и гласная v 0…5
  func angle(_ k: Int, _ p: CGPoint) -> Double {
    let s = sph[k], a = atan2(Double((p.x - cx) / s.rx), -Double((p.y - cy) / s.ry))
    return a < 0 ? a + 2 * .pi : a
  }
  func vowel(_ k: Int, _ p: CGPoint) -> Double { angle(k, p) / (2 * .pi) * 5 }
  func angelPos(_ k: Int, _ v: Double) -> CGPoint {
    let s = sph[min(8, max(0, k))], a = v / 5 * 2 * .pi
    return CGPoint(x: cx + CGFloat(sin(a)) * s.rx, y: cy - CGFloat(cos(a)) * s.ry)
  }
  // зона касания ангела: квадрат ±hitAngel вокруг точки на сфере
  func angelRect(k: Int, v: Double) -> CGRect {
    let p = angelPos(k, v); return CGRect(x: p.x - ParGeo.hitAngel, y: p.y - ParGeo.hitAngel, width: ParGeo.hitAngel * 2, height: ParGeo.hitAngel * 2)
  }
  func angelRect(_ a: ParAngel) -> CGRect { angelRect(k: a.k, v: a.v) }

  // ближайшая сфера (расстояние до эллипса вдоль луча из центра); дальше hitSphere — nil
  func sphereAt(_ p: CGPoint) -> Int? {
    let dx = p.x - cx, dy = p.y - cy, r = hypot(dx, dy)
    var best: Int?, bd = CGFloat.infinity
    for (k, s) in sph.enumerated() {
      let rho = hypot(dx / s.rx, dy / s.ry), d = abs(r - r / max(rho, 1e-6))
      if d < bd { bd = d; best = k }
    }
    return bd < ParGeo.hitSphere ? best : nil
  }
}
