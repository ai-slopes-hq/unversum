import CoreGraphics
import Foundation

// геометрия Hymenaeus по макету mockups/hymenaeus-v2.html (вариант C): сцена 390×844, масштаб S = min(W/390, H/844), по центру на широком экране.
// Сверху вниз: облака (с линзой пересечения) · mix · две нити (resonans, chorda: тонкая линия с бегунком, удержание — перебросить) · переплетённые кольца · печати-локации.
struct HymGeo {
  let W, H, S, ox, oy: CGFloat
  let sun: (x: CGFloat, y: CGFloat, r: CGFloat)
  // нити: y линии каждой нити; бегунок идёт от staveX0 до staveX1 (по шагам петли), застёжка справа; зона касания — ±staveHit
  static let staveLine: [CGFloat] = [384, 478], staveX0: CGFloat = 42, staveX1: CGFloat = 332, staveL: CGFloat = 22, staveR: CGFloat = 370, claspX: CGFloat = 356, staveHit: CGFloat = 22
  // кольца: нить t играет кольцо t; центры по одной горизонтали, обода пересекаются
  static let ringCY: CGFloat = 640, ringX: [CGFloat] = [158, 232], ringRad: CGFloat = 50, ringW: CGFloat = 11
  static let mixY: CGFloat = 298, mixX0: CGFloat = 70, mixX1: CGFloat = 320
  static let sealY: CGFloat = 722, sealX0: CGFloat = 105, sealDX: CGFloat = 62, sealR: CGFloat = 15

  init(W: CGFloat, H: CGFloat, topY: CGFloat) {
    self.W = W; self.H = H
    S = min(W / 390, H / 844); ox = (W - 390 * S) / 2; oy = (H - 844 * S) / 2
    sun = (0, topY + 2, 46 * S)
  }

  func pt(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: ox + x * S, y: oy + y * S) }
  func rect(_ x0: CGFloat, _ y0: CGFloat, _ x1: CGFloat, _ y1: CGFloat) -> CGRect { CGRect(x: ox + x0 * S, y: oy + y0 * S, width: (x1 - x0) * S, height: (y1 - y0) * S) }

  var cloudsRect: CGRect { rect(0, 96, 390, 282) }
  var lensRect: CGRect { rect(150, 150, 232, 214) }                          // линза пересечения двух облаков — внутри cloudsRect
  var mixTrack: CGRect { rect(HymGeo.mixX0 - 10, HymGeo.mixY - 12, HymGeo.mixX1 + 10, HymGeo.mixY + 12) }
  func mixValue(_ x: CGFloat) -> Double { Hym.clamp(Double((x - pt(HymGeo.mixX0, 0).x) / ((HymGeo.mixX1 - HymGeo.mixX0) * S))) }
  func staveY(_ t: Int) -> CGFloat { pt(0, HymGeo.staveLine[t]).y }         // линия нити t
  func staveRect(_ t: Int) -> CGRect { rect(HymGeo.staveL - 4, HymGeo.staveLine[t] - HymGeo.staveHit, HymGeo.staveR + 4, HymGeo.staveLine[t] + HymGeo.staveHit) }
  var ringsRect: CGRect { rect(HymGeo.ringX[0] - HymGeo.ringRad - HymGeo.ringW, HymGeo.ringCY - HymGeo.ringRad - HymGeo.ringW, HymGeo.ringX[1] + HymGeo.ringRad + HymGeo.ringW, HymGeo.ringCY + HymGeo.ringRad + HymGeo.ringW) }
  func sealRect(_ loc: Int) -> CGRect {                                       // кружок печати и римская цифра под ним
    let x = HymGeo.sealX0 + CGFloat(loc) * HymGeo.sealDX
    return rect(x - HymGeo.sealR - 2, HymGeo.sealY - HymGeo.sealR - 2, x + HymGeo.sealR + 2, HymGeo.sealY + HymGeo.sealR + 18)
  }

  func ringCenter(_ i: Int) -> CGPoint { pt(HymGeo.ringX[i], HymGeo.ringCY) }
  func ringR(_ i: Int) -> CGFloat { HymGeo.ringRad * S }

  // кольцо под пальцем и место удара 0…1 (по часовой от верха): ближе к ободу — выбирается; внутри и у края — тоже кольцо
  func ringAt(_ x: CGFloat, _ y: CGFloat) -> (ring: Int, pos: Double)? {
    var best: Int?, bd = CGFloat.infinity
    for i in 0..<Hym.nr {
      let c = ringCenter(i), d = hypot(x - c.x, y - c.y), edge = abs(d - ringR(i))
      if d <= ringR(i) + (HymGeo.ringW * 1.5 + 6) * S && edge < bd { bd = edge; best = i }
    }
    guard let i = best else { return nil }
    let c = ringCenter(i)
    var a = atan2(Double(x - c.x), -Double(y - c.y)) / (2 * .pi)      // 0 вверху, по часовой
    a -= a.rounded(.down)
    return (i, a)
  }

  // бегунок на нити t: шаг i из n — x равномерно от staveX0 до staveX1 (u 0…1), y — на линии; высота ноты — только у летящей звёздочки (pitchOffset)
  func playU(_ i: Int, n: Int) -> Double { n > 1 ? Double(i) / Double(n - 1) : 0.5 }
  func playheadPoint(_ t: Int, u: Double) -> CGPoint { pt(HymGeo.staveX0 + CGFloat(u) * (HymGeo.staveX1 - HymGeo.staveX0), HymGeo.staveLine[t]) }
  func pitchOffset(_ pitch: Double) -> CGFloat { -CGFloat(Hym.clamp(pitch) - 0.5) * 22 * S }   // выше нота — выше звёздочка над линией
  func claspPoint(_ t: Int) -> CGPoint { pt(HymGeo.claspX, HymGeo.staveLine[t]) }

  func cornerHit(_ x: CGFloat, _ y: CGFloat) -> Bool {              // солнце-четверть слева сверху ведёт на карту
    y > sun.y && hypot(x - sun.x, y - sun.y) < sun.r + 6
  }
}
