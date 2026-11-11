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
  private(set) lazy var fx: SheetSpec? = fxSpec        // веер собран раз: замыкания читают зеркало вживую
  var openSheet: ((SheetSpec) -> Void)?
  var objGone: ((Int) -> Void)?
  var rowLfoOff: ((Int) -> Void)?
  var tempoSet: ((Double) -> Void)?

  let legend: [LegendRow] = [
    LegendRow(glyph: .tap, gesture: "tap a ring", does: "it sounds, where you touch the rim"),
    LegendRow(glyph: .drag, gesture: "drag the clouds", does: "← → position, ↑ ↓ grain; bottom strip — echo"),
    LegendRow(glyph: .hold, gesture: "hold the clouds", does: "freeze while you hold"),
    LegendRow(glyph: .drag, gesture: "drag a ring", does: "↑ ↓ brightness, ← → damping"),
    LegendRow(glyph: .pinch, gesture: "pinch a ring", does: "structure"),
    LegendRow(glyph: .hold, gesture: "hold a ring", does: "its sheet: mode, voices, level"),
    LegendRow(glyph: .tap, gesture: "tap a pearl", does: "rest on / off"),
    LegendRow(glyph: .drag, gesture: "drag a pearl or the clasp", does: "↑ ↓ pitch · clasp ← → déjà vu"),
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
  private var mbuf = [Double](repeating: 0, count: 128), hbuf = [Double](repeating: 0, count: 80)
  private var now = 0.0
  let dr = HymDrawState()

  struct Hold {
    var zone: HymZone?; var p0 = CGPoint.zero; var base = HymBase(); var downAt = 0.0
    var cur = CGPoint.zero                                 // где палец сейчас: щипку нужны оба конца
    var moved = false, frozen = false, pinching = false
    var last: HymGesture?                                  // жест, который идёт сейчас: по нему подпись
  }
  private(set) var holds: [ObjectIdentifier: Hold] = [:]
  var activeLabels: [(text: String, at: CGPoint)] {
    holds.values.compactMap { h in
      guard let g = h.last else { return nil }
      let cr = geo.cloudsRect
      let y: CGFloat
      switch h.zone {
      case .cloud?, .echo?: y = cr.minY + 8 * geo.S
      case .mix?: y = geo.pt(0, HymGeo.mixY).y - 18 * geo.S
      case .ring?: y = geo.ringsRect.minY - 8 * geo.S
      case .seal?: y = geo.sealRect(0).minY - 6 * geo.S
      case .pearl(let t, _)?, .clasp(let t)?, .stave(let t)?: y = geo.staveRect(t).minY + 2 * geo.S
      default: y = geo.staveRect(0).minY - 6 * geo.S
      }
      var text = HymTouch.label(g)
      if case .pearlPitch(let t, _, let v) = g, mirror.hi[t] > mirror.lo[t] {          // высота жемчужины — нота MIDI в диапазоне нити
        text += " · " + HymenaeusView.noteName(mirror.lo[t] + Int((v * Double(mirror.hi[t] - mirror.lo[t])).rounded()))
      }
      return (text, CGPoint(x: geo.W / 2, y: y))
    }
  }

  init(core: Core, readMirror: Reader? = nil, readHeard: Reader? = nil) {
    self.core = core; geo = HymGeo(W: 390, H: 844, topY: 59)
    self.readMirror = readMirror ?? { b in Int(t2d_app_mirror(Int32(HymenaeusView.slotN), &b, Int32(b.count))) }
    self.readHeard = readHeard ?? { b in Int(t2d_app_heard(Int32(HymenaeusView.slotN), &b, Int32(b.count))) }
  }

  func layout(W: CGFloat, H: CGFloat, topY: CGFloat, safeBot: CGFloat) { geo = HymGeo(W: W, H: H, topY: topY) }
  func cornerHit(_ x: CGFloat, _ y: CGFloat) -> Bool { geo.cornerHit(x, y) }
  func dice() { core.action(Act.dice.rawValue, 0, [], touchTime: 0) }                 // кубик «ноты»: обе нити активной локации, звук не трогает
  func timbre() { core.action(Act.timbre.rawValue, 0, [], touchTime: 0) }             // кубик «тембр»: кольца и облака, ноты не трогает
  var hasTimbre: Bool { true }                                                         // хост рисует вторую кость в нижнем баре
  func clear() { core.action(Act.clear.rawValue, 0, [], touchTime: 0) }
  private var started = false
  func enter() {                                          // нить играет сама при первом входе; остановленную и восстановленную не будим
    guard !started else { return }
    started = true; core.action(Act.run.rawValue, 0, [1], touchTime: 0)
  }
  func restored() {
    started = true
    holds.removeAll()
    for i in ringHit.indices { ringHit[i] = -1e9 }
    for i in notes.indices { notes[i].born = -1e9 }
  }
  func idle() { while readHeard(&hbuf) > 0 {} }

  func tick(now: Double, dt: Double) {
    self.now = now
    pullMirror()
    for (k, h) in holds where h.zone == .cloud && !h.moved && !h.frozen && now - h.downAt > Touch.holdMs / 1000 {
      holds[k]?.frozen = true; holds[k]?.last = .cloudHold; send(.cloudHold, touchTime: 0)
    }
    for (k, h) in holds where !h.moved && !h.pinching && now - h.downAt > Touch.holdMs / 1000 {     // удержание на кольце — его плашка
      if case .ring(let r, _)? = h.zone { holds[k]?.moved = true; openSheet?(ringSpec(r)) }
    }
    for (k, h) in holds where !h.moved && !h.pinching && now - h.downAt > Touch.holdMs / 1000 {     // удержание на стане — перебросить только эту нить
      if case .stave(let t)? = h.zone { holds[k]?.moved = true; holds[k]?.last = .staveDice(thread: t); core.action(Act.dice.rawValue, 0, [Double(t)], touchTime: 0) }
    }
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
  private func baseAt(_ z: HymZone?) -> HymBase {
    var b = HymBase()
    switch z {
    case .ring(let r, _)?: let q = mirror.rings[r]; b.bright = q.bright; b.damp = q.damp; b.structure = q.structure
    case .clasp(let t)?: b.dejavu = mirror.dejavu[t]
    case .pearl(let t, let i)?: b.pitch = mirror.pearls[t][i].pitch
    default: break
    }
    return b
  }

  func down(_ f: Finger) {
    let p = CGPoint(x: f.x, y: f.y)
    let z = HymTouch.zone(at: p, geo: geo, mirror: mirror)
    var h = Hold(zone: z, p0: p, base: baseAt(z), downAt: now); h.cur = p
    if case .seal(let l)? = z { h.last = .sealTap(l) }                  // подпись «loca», пока палец на печати
    if case .ring(let r, _)? = z, let (k, o) = holds.first(where: { sameRing($0.value.zone, r) }) {
      holds[k]?.pinching = true; holds[k]?.moved = true; h.pinching = true; h.moved = true     // второй палец на том же кольце — щипок
      h.base = o.base
    }
    holds[f.id] = h
  }

  private func sameRing(_ z: HymZone?, _ r: Int) -> Bool { if case .ring(let q, _)? = z { return q == r } else { return false } }

  private func send(_ g: HymGesture, touchTime: Double) {
    switch g {
    case .cloudDrag(let x, let y):
      core.action(Act.cloud.rawValue, 0, [Double(Hym.Cloud.pos), x], touchTime: touchTime)
      core.action(Act.cloud.rawValue, 0, [Double(Hym.Cloud.size), y], touchTime: touchTime)
    case .cloudHold: core.action(Act.cloud.rawValue, 0, [Double(Hym.Cloud.freeze), 1], touchTime: touchTime)
    case .echoDrag(let x): core.action(Act.cloud.rawValue, 0, [Double(Hym.Cloud.echo), x], touchTime: touchTime)
    case .ringStrike(let r, let pos): core.action(Act.strike.rawValue, r, [pos, 1], touchTime: touchTime)
    case .ringAxes(let r, let b, let d):
      core.action(Act.ring.rawValue, r, [Double(Hym.Ring.bright), b], touchTime: touchTime)
      core.action(Act.ring.rawValue, r, [Double(Hym.Ring.damp), d], touchTime: touchTime)
    case .ringPinch(let r, let v): core.action(Act.ring.rawValue, r, [Double(Hym.Ring.structure), v], touchTime: touchTime)
    case .claspDrag(let t, let v): core.action(Act.thread.rawValue, t, [Double(Hym.Thread.dejavu), v], touchTime: touchTime)
    case .pearlTap(let t, let i): core.action(Act.pearl.rawValue, i, [Double(t), 1, mirror.pearls[t][i].rest ? 0 : 1], touchTime: touchTime)
    case .pearlPitch(let t, let i, let v): core.action(Act.pearl.rawValue, i, [Double(t), 0, v], touchTime: touchTime)
    case .mixDrag(let v): core.action(Act.mix.rawValue, 0, [v], touchTime: touchTime)
    case .staveDice: break                                                  // бросок уходит из tick
    case .sealTap(let l): core.action(Act.loc.rawValue, 0, [Double(l)], touchTime: touchTime)
    }
  }

  func move(_ f: Finger) {
    guard var h = holds[f.id], let z = h.zone else { return }
    let p = CGPoint(x: f.x, y: f.y)
    if !HymTouch.ok(p) { return }
    h.cur = p
    if hypot(p.x - h.p0.x, p.y - h.p0.y) > Touch.tapMove { h.moved = true }
    if h.pinching, case .ring(let r, _) = z,
       let (_, o) = holds.first(where: { $0.key != f.id && $0.value.pinching && sameRing($0.value.zone, r) }) {
      let d0 = hypot(h.p0.x - o.p0.x, h.p0.y - o.p0.y), d = hypot(p.x - o.cur.x, p.y - o.cur.y)
      if let g = HymTouch.pinch(ring: r, d0: d0, d: d, geo: geo, base: h.base) { send(g, touchTime: f.touchTime); h.last = g }
    } else if h.moved, !h.pinching, let g = HymTouch.drag(z, from: h.p0, to: p, geo: geo, base: h.base) {
      if h.frozen { h.frozen = false; core.action(Act.cloud.rawValue, 0, [Double(Hym.Cloud.freeze), 0], touchTime: f.touchTime) }
      send(g, touchTime: f.touchTime); h.last = g
    }
    holds[f.id] = h
  }

  func up(_ f: Finger) {
    guard let h = holds.removeValue(forKey: f.id) else { return }
    if h.frozen { core.action(Act.cloud.rawValue, 0, [Double(Hym.Cloud.freeze), 0], touchTime: f.touchTime) }       // glacies только пока держишь
    if h.moved { return }
    switch h.zone {
    case .ring(let r, let pos)?:
      ringHitPos[r] = pos
      send(.ringStrike(ring: r, pos: pos), touchTime: f.touchTime)           // ручной удар поверх нити
    case .pearl(let t, let i)?: send(.pearlTap(thread: t, i), touchTime: f.touchTime)
    case .seal(let l)?: send(.sealTap(l), touchTime: f.touchTime)
    case .mix?: send(.mixDrag(geo.mixValue(CGFloat(h.p0.x))), touchTime: f.touchTime)          // тап по линии — прыжок бегунка
    default: break
    }
  }

  func cancel(_ f: Finger) {
    if let h = holds.removeValue(forKey: f.id), h.frozen { core.action(Act.cloud.rawValue, 0, [Double(Hym.Cloud.freeze), 0], touchTime: 0) }
  }

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
  var armed: Bool { (0..<Hym.nr).contains { t in mirror.pearls[t].prefix(max(1, mirror.len[t])).contains { !$0.rest } } }
}
