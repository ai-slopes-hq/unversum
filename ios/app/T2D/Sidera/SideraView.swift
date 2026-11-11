import UIKit
import T2DCore

// Sidera (XIV) — связка ядра и Swift: три голоса GENDYN, звёзды — точки излома формы волны. Модель и звук — в ядре
// (core/worlds/sidera), картинка и касания — Sid*.swift. Только нативный мир: в вебе его нет.
final class SideraView: World {
  static let slotN = 14
  typealias Act = SidAct

  let slot = SideraView.slotN
  let theme = Worlds.info(SideraView.slotN)?.theme ?? .base
  let title = "Sidera", sub = "stellae · luna"
  private(set) lazy var fx: SheetSpec? = fxSpec            // веер собран раз: замыкания читают зеркало вживую; плашка голоса — удержанием
  var openSheet: ((SheetSpec) -> Void)?
  var objGone: ((Int) -> Void)?
  var rowLfoOff: ((Int) -> Void)?
  var goMap: (() -> Void)?

  let legend: [LegendRow] = [
    LegendRow(glyph: .tap, gesture: "tap a star or a band", does: "shakes it: the star (or the whole constellation) jumps"),
    LegendRow(glyph: .drag, gesture: "drag a star", does: "moves its box: pitch of the bend, length of the step"),
    LegendRow(glyph: .two, gesture: "two fingers on a band", does: "← → box width, ↑ ↓ box height"),
    LegendRow(glyph: .hold, gesture: "hold a band or a star", does: "its sheet: note, stars, level, pan, law"),
    LegendRow(glyph: .drag, gesture: "drag the moon", does: "along the arc: how fast the stars wander"),
    LegendRow(glyph: .tap, gesture: "tap the moon", does: "map of worlds"),
  ]

  let core: Core
  private(set) var geo: SidGeo
  private(set) var mirror = SidMirror()
  typealias Reader = (inout [Double]) -> Int
  private let readMirror: Reader
  private var mbuf = [Double](repeating: 0, count: 256)
  private(set) var now = 0.0
  private var started = false
  var heads = [0.0, 1.7, 3.1]                           // позиция головы окна линии по голосам, в отрезках
  var pulses: [SidPulse] = []
  var holds: [ObjectIdentifier: SidHold] = [:]
  var pinches: [Int: SidPinchState] = [:]
  var labelOn = [Bool](repeating: false, count: Sid.nv)   // подпись «нота · N» — пока жест идёт (Task 6)

  init(core: Core, readMirror: Reader? = nil) {
    self.core = core
    geo = SidGeo(W: 390, H: 844, topY: 59, safeBot: 34)
    self.readMirror = readMirror ?? { b in Int(t2d_app_mirror(Int32(SideraView.slotN), &b, Int32(b.count))) }
  }

  func layout(W: CGFloat, H: CGFloat, topY: CGFloat, safeBot: CGFloat) { geo = SidGeo(W: W, H: H, topY: topY, safeBot: safeBot) }
  func cornerHit(_ x: CGFloat, _ y: CGFloat) -> Bool { false }   // луна сама ведёт касание: тап — карта, тянешь — фаза
  func dice() { core.action(Act.dice.rawValue, 0, [], touchTime: 0) }
  func clear() { core.action(Act.clear.rawValue, 0, [], touchTime: 0) }
  func enter() {                                          // каждый вход шлёт RUN 1: reinit ядра сбрасывает run, мир без него молчит
    started = true; core.action(Act.run.rawValue, 0, [1], touchTime: 0)
  }
  func scale(pcs: [Int]?, root: Int) {}                   // лад читает ядро
  func restored() { started = true; pulses = []; holds = [:]; pinches = [:]; labelOn = [Bool](repeating: false, count: Sid.nv) }
  func idle() {}
  func tick(now: Double, dt: Double) {
    self.now = now; pullMirror(); tickHolds()
    let step = min(max(dt, 0), 0.1) * sidHeadRate(step: mirror.step)
    for v in 0..<Sid.nv { let n = Double(max(mirror.voices[v].n, 1)); heads[v] = fmod(heads[v] + step, n) }
  }
  func pullMirror() {
    let len = min(max(0, readMirror(&mbuf)), mbuf.count)
    mirror.load(mbuf, n: len)
  }
  func draw(_ d: Draw, now: Double) { frame(d, now, moon: true) }
  func drawShot(_ d: Draw, now: Double) { frame(d, now, moon: false) }
}
