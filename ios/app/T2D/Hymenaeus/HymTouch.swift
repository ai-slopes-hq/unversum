import CoreGraphics
import Foundation

// жесты Hymenaeus по зонам: облака (X — positio, Y — granum), полоса echo внизу облаков, кольца (↕ яркость, ↔ затухание, щипок — structure),
// застёжка нити (déjà vu), жемчужины (тап — пауза, ↕ — высота). Чистые функции: вид только вызывает их и шлёт действие в ядро.
enum HymGesture: Equatable {
  case cloudDrag(x: Double, y: Double), cloudHold, echoDrag(x: Double)
  case ringStrike(ring: Int, pos: Double), ringAxes(ring: Int, bright: Double, damp: Double), ringPinch(ring: Int, structure: Double)
  case claspDrag(Double), pearlTap(Int), pearlPitch(Int, Double)
}

enum HymZone: Equatable {
  case cloud, echo, ring(Int, pos: Double), clasp, pearl(Int)
}

// исходные значения, от которых отсчитывается относительный жест
struct HymBase { var bright = 0.5, damp = 0.5, structure = 0.5, dejavu = 0.5, pitch = 0.5 }

enum HymTouch {
  static let echoStrip = 0.9                              // нижняя десятая облаков — полоса echo
  static let dragRing: CGFloat = 130, dragThread: CGFloat = 130, dragPearl: CGFloat = 110, pinchSpan: CGFloat = 120   // пальцем на всю шкалу, в пунктах макета

  static func ok(_ p: CGPoint) -> Bool { p.x.isFinite && p.y.isFinite }

  static func zone(at p: CGPoint, geo: HymGeo, len: Int, dejavu: Double) -> HymZone? {
    guard ok(p) else { return nil }
    if let h = geo.ringAt(p.x, p.y) { return .ring(h.ring, pos: h.pos) }
    let n = max(1, len), hit = 14 * geo.S
    for i in 0..<n where hypot(p.x - geo.pearlPoint(i, n: n, dejavu: dejavu).x, p.y - geo.pearlPoint(i, n: n, dejavu: dejavu).y) < hit { return .pearl(i) }
    for c in geo.claspPoints(dejavu: dejavu) where hypot(p.x - c.x, p.y - c.y) < 16 * geo.S { return .clasp }
    let cr = geo.cloudsRect
    if cr.contains(p) { return p.y > cr.minY + cr.height * echoStrip ? .echo : .cloud }
    return nil
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
    case .ring(let r, _): return .ringAxes(ring: r, bright: Hym.clamp(base.bright - Double(dy / dragRing)), damp: Hym.clamp(base.damp + Double(dx / dragRing)))
    case .clasp: return .claspDrag(Hym.clamp(base.dejavu + Double(dx / dragThread)))
    case .pearl(let i): return .pearlPitch(i, Hym.clamp(base.pitch - Double(dy / dragPearl)))
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
    case .ringStrike, .ringAxes, .ringPinch: return "anuli"
    case .claspDrag: return "déjà vu"
    case .pearlTap, .pearlPitch: return "margaritae"
    }
  }
}
