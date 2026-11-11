import UIKit
import T2DCore

// Paradisus (VI) — касания и состояние: девять сфер вокруг розы, ангел поёт, пока держишь; двойной тап оставляет его.
// Модель и звук — в ядре (core/worlds/paradisus), картинка — ParDraw.swift, жесты — чистые функции ParTouch. В вебе мир в архиве.
// Закрепление (ACT PIN) даёт ангелу новый id: id пальца после него мёртв — жест кончается, TOUCH и RELEASE с ним больше не шлём.
final class ParadisusView: World {
  static let slotN = 6
  typealias Act = ParAct

  let slot = ParadisusView.slotN
  let theme = Worlds.info(ParadisusView.slotN)?.theme ?? .base
  let title = "Paradisus", sub = "sphaerae · chorus"
  private(set) lazy var fx: SheetSpec? = fxSpec             // веер собран раз: замыкания читают зеркало вживую
  var openSheet: ((SheetSpec) -> Void)?
  var objGone: ((Int) -> Void)?
  var rowLfoOff: ((Int) -> Void)?

  let legend: [LegendRow] = [
    LegendRow(glyph: .tap, gesture: "touch a sphere", does: "an angel sings while held"),
    LegendRow(glyph: .drag, gesture: "drag along a sphere", does: "the vowel: a, e, i, o, u"),
    LegendRow(glyph: .drag, gesture: "drag across", does: "to another sphere — another harmonic"),
    LegendRow(glyph: .double, gesture: "double tap", does: "the angel stays and sings · again — let go"),
    LegendRow(glyph: .hold, gesture: "hold a staying angel", does: "its sheet: choir, detune, vibrato, breath, register"),
    LegendRow(glyph: .tap, gesture: "tap the rose", does: "a shower of grains"),
    LegendRow(glyph: .tap, gesture: "tap the light", does: "map of worlds"),
  ]

  var geo: ParGeo
  private(set) var mirror = ParMirror()
  let core: Core
  typealias Reader = (inout [Double]) -> Int
  private let readMirror: Reader
  private let readHeard: Reader
  private var mbuf = [Double](repeating: 0, count: Par.mirrorN), hbuf = [Double](repeating: 0, count: 72)
  private var now = 0.0
  let dr = ParDrawState()
  private var rng = SystemRandomNumberGenerator()

  struct Hold {
    enum Kind { case none, temp, pinned }
    var kind = Kind.none
    var id = 0, k = 0
    var v = 0.0
    var p0 = CGPoint.zero, cur = CGPoint.zero
    var downAt = 0.0
    var moved = false, sheet = false
  }
  private(set) var holds: [ObjectIdentifier: Hold] = [:]
  private(set) var lastTap: ParTap?
  private var issued = 0                                      // следующий id касания (>= 1): ядро принимает TOUCH только с id >= 0
  private var pinChecks: [(id: Int, at: Double)] = []         // закрепления, за которыми смотрим: ядро могло отказать (пятый ангел)
  private var prevPinned: Set<Int> = []

  init(core: Core, readMirror: Reader? = nil, readHeard: Reader? = nil) {
    self.core = core; geo = ParGeo(size: CGSize(width: 390, height: 844), safeBot: 34, topY: 59)
    self.readMirror = readMirror ?? { b in Int(t2d_app_mirror(Int32(ParadisusView.slotN), &b, Int32(b.count))) }
    self.readHeard = readHeard ?? { b in Int(t2d_app_heard(Int32(ParadisusView.slotN), &b, Int32(b.count))) }
  }

  func layout(W: CGFloat, H: CGFloat, topY: CGFloat, safeBot: CGFloat) { geo = ParGeo(size: CGSize(width: W, height: H), safeBot: safeBot, topY: topY) }
  func cornerHit(_ x: CGFloat, _ y: CGFloat) -> Bool { hypot(x - geo.light.x, y - geo.light.y) < geo.light.r }   // божественный свет — карта
  func dice() { core.action(Act.dice.rawValue, 0, [], touchTime: 0) }
  func clear() { core.action(Act.clear.rawValue, 0, [], touchTime: 0) }
  func restored() {
    holds.removeAll(); lastTap = nil; pinChecks.removeAll(); prevPinned.removeAll()
    dr.flashes.removeAll(); dr.glitter.removeAll()
  }
  func idle() { while readHeard(&hbuf) > 0 {} }

  private func pullMirror() {
    let len = min(max(0, readMirror(&mbuf)), mbuf.count)
    if len >= Par.mirrorN { mirror.load(mbuf) } else { mirror = ParMirror() }
  }

  private func send(_ a: Act, _ i: Int, _ args: [Double], _ t: Double = 0) { core.action(a.rawValue, i, args, touchTime: t) }
  private func newId() -> Int { issued = issued % 1_000_000 + 1; return issued }

  // ---------- кадр ----------
  func tick(now: Double, dt: Double) {
    self.now = now
    pullMirror()
    dr.flashes.removeAll { now - $0.t > ParDrawState.flashLife }
    dr.glitter.removeAll { now - $0.t > ParDrawState.grainLife }
    for _ in 0..<4 {                                         // heard: [k, (тип, арг) × k]; тип 0 звон (арг — id ангела), 1 зерно (арг — панорама)
      let hn = min(max(0, readHeard(&hbuf)), hbuf.count)
      if hn < 3 { break }
      let k = min(Int(hbuf[0]), (hn - 1) / 2)
      for j in 0..<max(0, k) {
        let type = Int(hbuf[1 + 2 * j]), arg = hbuf[2 + 2 * j]
        if type == 0 {
          guard let a = mirror.angels.first(where: { $0.id == Int(arg) }) else { continue }
          let p = geo.angelPos(a.k, a.v)
          dr.flashes.append(ParDrawState.Flash(t: now, x: p.x, y: p.y))
          if dr.flashes.count > ParDrawState.flashMax { dr.flashes.removeFirst() }
        } else if type == 1 {
          let base: CGFloat = arg >= 0 ? -.pi / 2 : .pi / 2   // панорама ведёт зерно в свою половину розы
          let a = base + CGFloat.random(in: 0...(.pi), using: &rng)
          dr.glitter.append(ParDrawState.Grain(t: now, a: a, r: CGFloat.random(in: 0...1, using: &rng)))
          if dr.glitter.count > ParDrawState.grainMax { dr.glitter.removeFirst() }
        }
      }
    }
    let lv = mirror.angels.reduce(0.0) { $0 + $1.lvl } / 3 + mirror.fx[5] * 0.3     // роза раскрывается с хором
    dr.openness += (Par.clamp(lv) - dr.openness) * (1 - exp(-max(0, dt) / 1.2))

    for (key, h) in holds where h.kind == .pinned && !h.moved && !h.sheet && now - h.downAt > Touch.holdMs / 1000 {     // держишь закреплённого — плашка
      holds[key]?.sheet = true
      openSheet?(angelSpec(h.id))
    }
    pinChecks.removeAll { c in                                // закрепление не прошло (пятое): временный ангел остался — отпускаем
      if now - c.at < 0.3 { return false }
      if ParTouch.stuckTemp(mirror, id: c.id) { send(.release, c.id, []) }
      return true
    }
    let pinned = Set(mirror.angels.filter(\.pinned).map(\.id))
    for id in prevPinned.subtracting(pinned) { objGone?(id) } // ангел снят — его плашка закрывается
    prevPinned = pinned
  }

  // подпись, пока трогаешь ангела: имя сферы · гармоника, гласная
  var activeLabels: [(title: String, sub: String, at: CGPoint, align: Align)] {
    holds.values.compactMap { h in
      guard h.kind != .none, !h.sheet, h.kind == .temp || h.moved else { return nil }   // закреплённого подписываем, только пока ведут; под плашкой — нет
      let a = mirror.angels.first { $0.id == h.id }
      let k = a?.k ?? h.k, v = a?.v ?? h.v, p = geo.angelPos(k, v), right = p.x < geo.cx
      return ("\(ParLook.name(k)) · \(ParLook.harm(k))", "\(ParLook.vowelName(v)) · ×\(ParLook.harm(k))",
              CGPoint(x: p.x + (right ? 30 : -30), y: p.y), right ? .left : .right)
    }
  }

  // ---------- касания ----------
  func down(_ f: Finger) {
    let p = CGPoint(x: f.x, y: f.y)
    guard ParTouch.ok(p) else { return }
    pullMirror()
    var h = Hold(p0: p, cur: p, downAt: now)
    switch ParTouch.hit(p, geo: geo, m: mbuf) {
    case .rose:
      send(.rose, 0, [], f.touchTime)                          // россыпь зёрен; пальцем ничего не ведём
    case .pinned(let id):
      if ParTouch.isDouble(last: lastTap, id: id, at: p, now: now) {
        lastTap = nil; send(.pin, id, [], f.touchTime)         // двойной тап по закреплённому — отпустить (ядро снимает ангела)
      } else {
        lastTap = ParTap(id: id, t: now, p: p)
        let a = mirror.angels.first { $0.id == id }
        h.kind = .pinned; h.id = id; h.k = a?.k ?? 0; h.v = a?.v ?? 0
      }
    case .angel, .sphere, .none:
      guard let k = geo.sphereAt(p) else { holds[f.id] = h; return }
      let v = geo.vowel(k, p), id = newId()
      send(.touch, id, [Double(k), v], f.touchTime)
      if ParTouch.isDouble(last: lastTap, id: nil, at: p, now: now) {
        lastTap = nil
        send(.pin, id, [], f.touchTime)                        // двойной тап — ангел остаётся; id пальца после этого мёртв, жест кончен
        pinChecks.append((id: id, at: now))
      } else {
        lastTap = ParTap(id: nil, t: now, p: p)
        h.kind = .temp; h.id = id; h.k = k; h.v = v           // поёт, пока держишь
      }
    }
    holds[f.id] = h
  }

  func move(_ f: Finger) {
    guard var h = holds[f.id], h.kind != .none, !h.sheet else { return }
    let p = CGPoint(x: f.x, y: f.y)
    guard ParTouch.ok(p) else { return }
    h.cur = p
    if !h.moved && !ParTouch.moved(from: h.p0, to: p) { holds[f.id] = h; return }
    h.moved = true
    let g = ParTouch.drag(to: p, k: h.k, geo: geo)
    switch g {
    case .touch(let k, let v)?: h.k = k; h.v = v; send(.touch, h.id, [Double(k), v], f.touchTime)     // поперёк — на ближайшую сферу
    case .vowel(let v)?: h.v = v; send(.vowel, h.id, [v], f.touchTime)                                // вдоль — гласная
    case nil: break
    }
    holds[f.id] = h
  }

  func up(_ f: Finger) {
    guard let h = holds.removeValue(forKey: f.id) else { return }
    if h.kind == .temp { send(.release, h.id, [], f.touchTime) }       // временный ангел гаснет; закреплённого RELEASE не трогает
  }

  func cancel(_ f: Finger) {
    guard let h = holds.removeValue(forKey: f.id) else { return }
    if h.kind == .temp { send(.release, h.id, []) }                    // система отняла палец: ангела не оставляем висеть
  }

  func draw(_ d: Draw, now: Double) { frame(d, now, light: true) }
  func drawShot(_ d: Draw, now: Double) { frame(d, now, light: false) }
}
