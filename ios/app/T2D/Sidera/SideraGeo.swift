import CoreGraphics
import Foundation

// геометрия Sidera: три полосы голосов друг под другом, луна в левом верхнем углу (четверть диска, как солнце других миров).
// Звезда i голоса — вершина ломаной: x — накопленная длительность (SidVoice.frac), y — амплитуда; рамка звезды — коробка центра (hd, ha).
struct SidGeo {
  let W, H, S, topY, safeBot: CGFloat
  let moon: (c: CGPoint, r: CGFloat)
  static let pad: CGFloat = 10, padY: CGFloat = 12, gap: CGFloat = 12, side: CGFloat = 16

  init(W: CGFloat, H: CGFloat, topY: CGFloat, safeBot: CGFloat) {
    self.W = W; self.H = H; self.topY = topY; self.safeBot = safeBot
    S = min(W / 390, H / 844)
    moon = (CGPoint(x: 0, y: topY + 10 * S), 34 * S)       // ниже заголовка и меньше: ни диск, ни дуга фазы (1,28 r) не лезут под имя мира
  }

  var field: CGRect {                                      // от под луны до над нижним баром
    let top = topY + 10 * S + 34 * S * 1.28 + 10, bottom = H - safeBot - TopBar.barH - 12
    return CGRect(x: SidGeo.side, y: top, width: W - 2 * SidGeo.side, height: max(1, bottom - top))
  }
  func bandRect(_ v: Int) -> CGRect {
    let f = field, h = (f.height - 2 * SidGeo.gap) / 3
    return CGRect(x: f.minX, y: f.minY + CGFloat(min(max(v, 0), Sid.nv - 1)) * (h + SidGeo.gap), width: f.width, height: h)
  }
  func inner(_ v: Int) -> CGRect { bandRect(v).insetBy(dx: SidGeo.pad, dy: SidGeo.padY) }
  func bandAt(_ p: CGPoint) -> Int? { (0..<Sid.nv).first { bandRect($0).insetBy(dx: -4, dy: -SidGeo.gap / 2).contains(p) } }

  private func point(_ v: Int, _ x: Double, _ a: Double) -> CGPoint {
    let r = inner(v)
    return CGPoint(x: r.minX + CGFloat(x) * r.width, y: r.midY - CGFloat(a) * r.height / 2)
  }
  func starPoint(_ v: Int, _ voice: SidVoice, _ i: Int) -> CGPoint { point(v, voice.frac(i), voice.stars[i].a) }
  func homePoint(_ v: Int, _ voice: SidVoice, _ i: Int) -> CGPoint { point(v, voice.frac(i, home: true), voice.stars[i].ha) }
  // рамка звезды: центр — (hd, ha); ширина — размах длительности в долях периода, высота — размах амплитуды
  func boxRect(_ v: Int, _ voice: SidVoice, _ i: Int) -> CGRect {
    let c = homePoint(v, voice, i), r = inner(v)
    let w = max(6, r.width * CGFloat(2 * voice.wd) / CGFloat(max(voice.n, 1))), h = max(6, r.height * CGFloat(voice.wa) / 2)
    return CGRect(x: c.x - w / 2, y: c.y - h / 2, width: w, height: h)
  }

  func cornerHit(_ x: CGFloat, _ y: CGFloat) -> Bool {      // луна-четверть слева сверху
    y > moon.c.y && hypot(x - moon.c.x, y - moon.c.y) < moon.r + 6
  }
}
