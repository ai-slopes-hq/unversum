import UIKit
import T2DCore

// Hymenaeus (XIV) — касания и состояние: жемчужная нить играет сама, кольца звенят, звон уходит в облака.
// Модель и звук — в ядре (core/worlds/hymenaeus), картинка — Hym*.swift. Только нативный мир: в вебе его нет.
final class HymenaeusView: World {
  static let slotN = 14
  typealias Act = HymAct

  let slot = HymenaeusView.slotN
  let theme = Worlds.info(HymenaeusView.slotN)?.theme ?? .base
  let title = "Hymenaeus", sub = "margaritae · anuli"
  var fx: SheetSpec? { nil }                              // веер — шаг 7 плана
  var openSheet: ((SheetSpec) -> Void)?
  var objGone: ((Int) -> Void)?
  var rowLfoOff: ((Int) -> Void)?
  var tempoSet: ((Double) -> Void)?

  let legend: [LegendRow] = [
    LegendRow(glyph: .tap, gesture: "tap a ring", does: "it sounds, where you touch the rim"),
    LegendRow(glyph: .tap, gesture: "tap the sun", does: "map of worlds"),
  ]

  struct Note { var ring = 0; var born = -1e9 }
  static let noteLife = 1.1, noteMax = 12
  private(set) var notes = [Note](repeating: Note(), count: HymenaeusView.noteMax)
  private var noteAt = 0
  private(set) var ringHit = [Double](repeating: -1e9, count: Hym.nr)       // когда ударило кольцо (вспышка)
  private(set) var ringHitPos = [Double](repeating: 0.3, count: Hym.nr)     // место удара на ободе 0…1; у нити — 0.3, как в ядре

  var geo: HymGeo
  private(set) var mirror = HymMirror()
  let core: Core
  typealias Reader = (inout [Double]) -> Int
  private let readMirror: Reader
  private let readHeard: Reader
  private var mbuf = [Double](repeating: 0, count: 72), hbuf = [Double](repeating: 0, count: 80)
  private var now = 0.0
  let dr = HymDrawState()

  struct Hold { var ring: Int?; var pos = 0.0; var x0: CGFloat = 0, y0: CGFloat = 0; var moved = false }
  private(set) var holds: [ObjectIdentifier: Hold] = [:]

  init(core: Core, readMirror: Reader? = nil, readHeard: Reader? = nil) {
    self.core = core; geo = HymGeo(W: 390, H: 844, topY: 59)
    self.readMirror = readMirror ?? { b in Int(t2d_app_mirror(Int32(HymenaeusView.slotN), &b, Int32(b.count))) }
    self.readHeard = readHeard ?? { b in Int(t2d_app_heard(Int32(HymenaeusView.slotN), &b, Int32(b.count))) }
  }

  func layout(W: CGFloat, H: CGFloat, topY: CGFloat, safeBot: CGFloat) { geo = HymGeo(W: W, H: H, topY: topY) }
  func cornerHit(_ x: CGFloat, _ y: CGFloat) -> Bool { geo.cornerHit(x, y) }
  func dice() { core.action(Act.dice.rawValue, 0, [], touchTime: 0) }
  func clear() { core.action(Act.clear.rawValue, 0, [], touchTime: 0) }
  func enter() { core.action(Act.run.rawValue, 0, [1], touchTime: 0) }       // нить играет сама, как только вошли в мир
  func restored() {
    holds.removeAll()
    for i in ringHit.indices { ringHit[i] = -1e9 }
    for i in notes.indices { notes[i].born = -1e9 }
  }
  func idle() { while readHeard(&hbuf) > 0 {} }

  func tick(now: Double, dt: Double) {
    self.now = now
    pullMirror()
    for _ in 0..<4 {                                      // heard: [k, (кольцо, жемчужина) × k]; читаем до пустого, но не бесконечно
      let hn = min(max(0, readHeard(&hbuf)), hbuf.count)
      if hn < 3 { break }
      let k = min(Int(hbuf[0]), (hn - 1) / 2)
      for j in 0..<max(0, k) {
        let r = Int(hbuf[1 + 2 * j]), pearl = Int(hbuf[2 + 2 * j])
        guard r >= 0 && r < Hym.nr else { continue }
        ringHit[r] = now
        if pearl >= 0 { ringHitPos[r] = 0.3 }
        notes[noteAt] = Note(ring: r, born: now); noteAt = (noteAt + 1) % HymenaeusView.noteMax
      }
    }
  }

  // ---------- касания ----------
  func down(_ f: Finger) {
    guard let h = geo.ringAt(f.x, f.y) else { holds[f.id] = Hold(ring: nil); return }
    holds[f.id] = Hold(ring: h.ring, pos: h.pos, x0: f.x, y0: f.y)
  }

  func move(_ f: Finger) {
    guard var t = holds[f.id] else { return }
    if hypot(f.x - t.x0, f.y - t.y0) > Touch.tapMove { t.moved = true }
    holds[f.id] = t
  }

  func up(_ f: Finger) {
    guard let t = holds.removeValue(forKey: f.id), let r = t.ring, !t.moved else { return }
    ringHitPos[r] = t.pos
    core.action(Act.strike.rawValue, r, [t.pos, 1], touchTime: f.touchTime)     // ручной удар поверх нити
  }

  func cancel(_ f: Finger) { holds.removeValue(forKey: f.id) }

  func draw(_ d: Draw, now: Double) { frame(d, now, sun: true) }
  func drawShot(_ d: Draw, now: Double) { frame(d, now, sun: false) }
}

extension HymenaeusView: WorldTempo {
  var tempo: WorldTempo? { self }
  func pullMirror() {
    let len = min(max(0, readMirror(&mbuf)), mbuf.count)
    mirror.load(mbuf, n: len)
  }
  func refresh(now: Double) { self.now = now; pullMirror() }
  var bpm: Double { mirror.bpm }
  var tempoRange: ClosedRange<Double> { 20...300 }
  func setBpm(_ bpm: Double) { core.action(Act.bpm.rawValue, 0, [bpm], touchTime: 0) }
  func play(_ on: Bool) { core.action(Act.run.rawValue, 0, [on ? 1 : 0], touchTime: 0) }
  var playing: Bool { mirror.run }
  var armed: Bool { mirror.pearls.prefix(max(1, mirror.len)).contains { !$0.rest } }
}
