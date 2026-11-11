import CoreGraphics
import Foundation

// геометрия Hymenaeus по макету mockups/hymenaeus-v2.html (вариант C): сцена 390×844, масштаб S = min(W/390, H/844), по центру на широком экране.
// Сверху вниз: облака (с линзой пересечения) · mix · переплетённые кольца. Нитей-нотоносцев и печатей-локаций на экране нет: ноты стартуют над своим кольцом (launchPoint).
struct HymGeo {
  let W, H, S, ox, oy: CGFloat
  let sun: (x: CGFloat, y: CGFloat, r: CGFloat)
  // кольца: нить t играет кольцо t; центры по одной горизонтали, обода пересекаются
  static let ringCY: CGFloat = 640, ringX: [CGFloat] = [158, 232], ringRad: CGFloat = 50, ringW: CGFloat = 11
  static let mixY: CGFloat = 298, mixX0: CGFloat = 70, mixX1: CGFloat = 320
  static let launchY: CGFloat = 450, launchSpan: CGFloat = 110            // откуда стартует звёздочка: высота над кольцами и разброс по высоте ноты (в пунктах макета)

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
  var ringsRect: CGRect { rect(HymGeo.ringX[0] - HymGeo.ringRad - HymGeo.ringW, HymGeo.ringCY - HymGeo.ringRad - HymGeo.ringW, HymGeo.ringX[1] + HymGeo.ringRad + HymGeo.ringW, HymGeo.ringCY + HymGeo.ringRad + HymGeo.ringW) }

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

  // старт звёздочки ноты: над своим кольцом, левее — ниже нота, правее — выше (pitch 0…1); оттуда она летит к месту удара на ободе
  func launchPoint(_ r: Int, pitch: Double) -> CGPoint { pt(HymGeo.ringX[r] + CGFloat(Hym.clamp(pitch) - 0.5) * HymGeo.launchSpan, HymGeo.launchY) }

  func cornerHit(_ x: CGFloat, _ y: CGFloat) -> Bool {              // солнце-четверть слева сверху ведёт на карту
    y > sun.y && hypot(x - sun.x, y - sun.y) < sun.r + 6
  }
}
