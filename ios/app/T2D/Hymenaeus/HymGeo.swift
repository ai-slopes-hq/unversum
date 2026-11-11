import CoreGraphics
import Foundation

// геометрия Hymenaeus по макету mockups/hymenaeus.html: сцена 390×844, масштаб S = min(W/390, H/844), по центру на широком экране
struct HymGeo {
  let W, H, S, ox, oy: CGFloat
  let sun: (x: CGFloat, y: CGFloat, r: CGFloat)
  // нить: эллипс; кольца внутри неё (0 — большое, низкие ноты; 2 — малое)
  static let neckC = CGPoint(x: 197, y: 656), neckRx: CGFloat = 142, neckRy: CGFloat = 84
  static let ringX: [CGFloat] = [140, 215, 270], ringR: [CGFloat] = [46, 38, 30], ringW: CGFloat = 11

  init(W: CGFloat, H: CGFloat, topY: CGFloat) {
    self.W = W; self.H = H
    S = min(W / 390, H / 844); ox = (W - 390 * S) / 2; oy = (H - 844 * S) / 2
    sun = (0, topY + 2, 46 * S)
  }

  func pt(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: ox + x * S, y: oy + y * S) }
  func rect(_ x0: CGFloat, _ y0: CGFloat, _ x1: CGFloat, _ y1: CGFloat) -> CGRect { CGRect(x: ox + x0 * S, y: oy + y0 * S, width: (x1 - x0) * S, height: (y1 - y0) * S) }

  var cloudsRect: CGRect { rect(0, 96, 390, 420) }
  var neckRect: CGRect { rect(HymGeo.neckC.x - HymGeo.neckRx - 10, HymGeo.neckC.y - HymGeo.neckRy - 10, HymGeo.neckC.x + HymGeo.neckRx + 10, HymGeo.neckC.y + HymGeo.neckRy + 10) }
  var ringsRect: CGRect { rect(HymGeo.ringX[0] - HymGeo.ringR[0] - 6, HymGeo.neckC.y - HymGeo.ringR[0] - 6, HymGeo.ringX[2] + HymGeo.ringR[2] + 6, HymGeo.neckC.y + HymGeo.ringR[0] + 6) }

  func ringCenter(_ i: Int) -> CGPoint { pt(HymGeo.ringX[i], HymGeo.neckC.y) }
  func ringRadius(_ i: Int) -> CGFloat { HymGeo.ringR[i] * S }

  // кольцо под пальцем и место удара 0…1 (по часовой от верха): ближе к ободу — выбирается; внутри и у края — тоже кольцо
  func ringAt(_ x: CGFloat, _ y: CGFloat) -> (ring: Int, pos: Double)? {
    var best: Int?, bd = CGFloat.infinity
    for i in 0..<Hym.nr {
      let c = ringCenter(i), d = hypot(x - c.x, y - c.y), edge = abs(d - ringRadius(i))
      if d <= ringRadius(i) + (HymGeo.ringW * 1.5 + 6) * S && edge < bd { bd = edge; best = i }
    }
    guard let i = best else { return nil }
    let c = ringCenter(i)
    var a = atan2(Double(x - c.x), -Double(y - c.y)) / (2 * .pi)      // 0 вверху, по часовой
    a -= a.rounded(.down)
    return (i, a)
  }

  func cornerHit(_ x: CGFloat, _ y: CGFloat) -> Bool {              // солнце-четверть слева сверху ведёт на карту
    y > sun.y && hypot(x - sun.x, y - sun.y) < sun.r + 6
  }
}
