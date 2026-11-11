import CoreGraphics
import Foundation

// жесты Hymenaeus по зонам: облака (X — positio, Y — granum), полоса echo внизу облаков, mix, кольца (↕ яркость, ↔ затухание, щипок — structure),
// застёжка нити (déjà vu), нить (удержание — перебросить эту нить), печати (тап — локация). Жемчужин пальцем не правят — ни высоту, ни паузу, ни диапазон.
// Чистые функции: вид только вызывает их и шлёт действие в ядро.
enum HymGesture: Equatable {
  case cloudDrag(x: Double, y: Double), cloudHold, echoDrag(x: Double), mixDrag(Double)
  case ringStrike(ring: Int, pos: Double), ringAxes(ring: Int, bright: Double, damp: Double), ringPinch(ring: Int, structure: Double)
  case claspDrag(thread: Int, Double), sealTap(Int), staveDice(thread: Int)
}

// что под пальцем (первое совпадение сверху вниз по приоритету: застёжка, печать, кольцо, mix, облака, нить)
enum HymHit: Equatable {
  case clasp(Int), stave(Int), seal(Int), ring(Int, pos: Double), cloud, echo, mix, none
}
typealias HymZone = HymHit

// исходные значения, от которых отсчитывается относительный жест
struct HymBase { var bright = 0.5, damp = 0.5, structure = 0.5, dejavu = 0.5 }

enum HymTouch {
  static let echoStrip = 0.9                              // нижняя десятая облаков — полоса echo
  static let dragRing: CGFloat = 130, dragThread: CGFloat = 130, pinchSpan: CGFloat = 120   // пальцем на всю шкалу, в пунктах макета

  static func ok(_ p: CGPoint) -> Bool { p.x.isFinite && p.y.isFinite }

  static func hit(_ p: CGPoint, geo: HymGeo) -> HymHit {
    guard ok(p) else { return .none }
    for t in 0..<Hym.nr {
      let c = geo.claspPoint(t)
      if hypot(p.x - c.x, p.y - c.y) < 16 * geo.S { return .clasp(t) }
    }
    for l in 0..<Hym.locN where geo.sealRect(l).contains(p) { return .seal(l) }
    if let h = geo.ringAt(p.x, p.y) { return .ring(h.ring, pos: h.pos) }
    if geo.mixTrack.contains(p) { return .mix }
    let cr = geo.cloudsRect
    if cr.contains(p) { return p.y > cr.minY + cr.height * echoStrip ? .echo : .cloud }
    for t in 0..<Hym.nr where geo.staveRect(t).contains(p) { return .stave(t) }
    return .none
  }

  // как hit, но nil вместо .none
  static func zone(at p: CGPoint, geo: HymGeo) -> HymZone? {
    let h = hit(p, geo: geo)
    return h == HymHit.none ? nil : h
  }

  private static func unit(_ v: CGFloat) -> Double { Hym.clamp(Double(v)) }

  // жест перетаскивания от точки касания `from` до `to`; nil — зона без перетаскивания или точка не число
  static func drag(_ z: HymZone, from: CGPoint, to: CGPoint, geo: HymGeo, base: HymBase) -> HymGesture? {
    guard ok(from), ok(to) else { return nil }
    let dx = (to.x - from.x) / geo.S, dy = (to.y - from.y) / geo.S
    let cr = geo.cloudsRect
    switch z {
    case .cloud: return .cloudDrag(x: unit((to.x - cr.minX) / cr.width), y: unit(1 - (to.y - cr.minY) / (cr.height * CGFloat(echoStrip))))
    case .echo: return .echoDrag(x: unit((to.x - cr.minX) / cr.width))
    case .mix: return .mixDrag(geo.mixValue(to.x))
    case .ring(let r, _): return .ringAxes(ring: r, bright: Hym.clamp(base.bright - Double(dy / dragRing)), damp: Hym.clamp(base.damp + Double(dx / dragRing)))
    case .clasp(let t): return .claspDrag(thread: t, Hym.clamp(base.dejavu + Double(dx / dragThread)))
    case .stave, .seal, .none: return nil
    }
  }

  // щипок двумя пальцами на кольце: расстояние d0 → d меняет structure
  static func pinch(ring: Int, d0: CGFloat, d: CGFloat, geo: HymGeo, base: HymBase) -> HymGesture? {
    guard d0.isFinite, d.isFinite else { return nil }
    return .ringPinch(ring: ring, structure: Hym.clamp(base.structure + Double((d - d0) / geo.S / pinchSpan)))
  }

  // подпись, пока жест идёт
  static func label(_ g: HymGesture) -> String {
    switch g {
    case .cloudDrag: return "positio · granum"
    case .cloudHold: return "glacies · freeze"
    case .echoDrag: return "echo"
    case .mixDrag: return "sicca · umida"
    case .ringStrike, .ringAxes, .ringPinch: return "anuli"
    case .claspDrag: return "déjà vu"
    case .sealTap: return "loca"
    case .staveDice(let t): return Hym.ringNames[t]
    }
  }
}
