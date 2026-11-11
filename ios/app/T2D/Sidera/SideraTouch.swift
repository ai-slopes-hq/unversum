import CoreGraphics
import Foundation

// жесты Sidera — чистые функции; вид только вызывает их и шлёт действие в ядро
enum SidPick: Equatable { case star(voice: Int, pt: Int), box(Int), moon, none }

private func finite(_ p: CGPoint) -> Bool { p.x.isFinite && p.y.isFinite }

// что под пальцем: луна, ближайшая звезда (в пределах захвата), полоса голоса
func sidPick(_ g: SidGeo, _ m: SidMirror, _ p: CGPoint) -> SidPick {
  guard finite(p) else { return .none }
  if g.cornerHit(p.x, p.y) { return .moon }
  var best: (d: CGFloat, v: Int, i: Int)?
  let reach = 26 * g.S
  for v in 0..<Sid.nv where g.bandRect(v).insetBy(dx: -reach, dy: -reach).contains(p) {
    for i in 0..<m.voices[v].n {
      let s = g.starPoint(v, m.voices[v], i), d = hypot(s.x - p.x, s.y - p.y)
      if d <= reach && d < (best?.d ?? .infinity) { best = (d, v, i) }
    }
  }
  if let b = best { return .star(voice: b.v, pt: b.i) }
  if let v = g.bandAt(p) { return .box(v) }
  return .none
}

// куда сдвинуть центр рамки звезды pt под пальцем: ha — по вертикали; по горизонтали меняется длительность предыдущей звезды (prev),
// так что доля звезды pt в периоде равна доле пальца. Звезда 0 стоит на левом краю — prev nil
struct SidHome { var ha: Double; var prev: (pt: Int, hd: Double)? }

func sidHomeFromPoint(_ g: SidGeo, voice v: Int, _ voice: SidVoice, pt: Int, _ p: CGPoint) -> SidHome {
  let r = g.inner(v), n = min(max(voice.n, 1), Sid.nptMax)
  guard finite(p), r.width > 0, r.height > 0 else { return SidHome(ha: voice.stars[min(max(pt, 0), Sid.nptMax - 1)].ha, prev: nil) }
  let ha = Sid.clamp(Double((r.midY - p.y) / (r.height / 2)), -1, 1)
  guard pt >= 1, pt < n else { return SidHome(ha: ha, prev: nil) }
  let f = Sid.clamp(Double((p.x - r.minX) / r.width), 0.02, 0.98)
  var a = 0.0, rest = 0.0
  for j in 0..<n { if j < pt - 1 { a += voice.stars[j].hd } else if j >= pt { rest += voice.stars[j].hd } }
  let x = f * rest / (1 - f) - a
  return SidHome(ha: ha, prev: (pt - 1, Sid.clamp(x, Sid.dMin, 1)))
}

// положение пальца на дуге луны: 0 — вправо от центра, 1 — вниз
func sidMoonStep(_ g: SidGeo, _ p: CGPoint) -> Double {
  guard finite(p) else { return 0 }
  return Sid.clamp(atan2(Double(p.y - g.moon.c.y), Double(p.x - g.moon.c.x)) / (Double.pi / 2))
}

// два пальца на полосе: расстояние по горизонтали ведёт ширину рамок (wd), по вертикали — высоту (wa)
func sidPinch(_ g: SidGeo, voice v: Int, base: (wd: Double, wa: Double), d0: (dx: CGFloat, dy: CGFloat), d: (dx: CGFloat, dy: CGFloat)) -> (wd: Double, wa: Double) {
  let r = g.inner(v)
  guard d0.dx.isFinite, d0.dy.isFinite, d.dx.isFinite, d.dy.isFinite, r.width > 0, r.height > 0 else { return base }
  return (Sid.clamp(base.wd + Double((d.dx - d0.dx) / r.width)), Sid.clamp(base.wa + Double((d.dy - d0.dy) / r.height)))
}

// ---------- в виде ----------
struct SidHold {
  enum Kind { case star(Int, Int), band(Int), moon }
  var kind: Kind; var downAt: Double
  var moved = false, pinching = false, dead = false
  var x: CGFloat, y: CGFloat
}
struct SidPulse { var v: Int; var i: Int; var t0: Double }     // вспышка встряски: кольцо расходится от звезды (i = −1 — от всех звёзд голоса)
struct SidPinchState { var ids: [ObjectIdentifier]; var base: (wd: Double, wa: Double); var d0: (dx: CGFloat, dy: CGFloat) }

extension SideraView {
  private func voiceOf(_ k: SidHold.Kind) -> Int? {
    switch k { case .star(let v, _), .band(let v): return v; case .moon: return nil }
  }
  private func send(_ kind: SidAct, _ i: Int, _ a: [Double]) { core.action(kind.rawValue, i, a, touchTime: 0) }

  func down(_ f: Finger) {
    let p = CGPoint(x: f.x, y: f.y)
    let kind: SidHold.Kind
    switch sidPick(geo, mirror, p) {
    case .moon: kind = .moon
    case .star(let v, let i): kind = .star(v, i)
    case .box(let v): kind = .band(v)
    case .none: return
    }
    var h = SidHold(kind: kind, downAt: now, x: f.x, y: f.y)
    if let v = voiceOf(kind), let (oid, o) = holds.first(where: { voiceOf($0.value.kind) == v && !$0.value.dead && !$0.value.pinching }) {
      let m = mirror.voices[v]
      pinches[v] = SidPinchState(ids: [oid, f.id], base: (m.wd, m.wa), d0: (abs(o.x - f.x), abs(o.y - f.y)))   // второй палец на той же полосе — щипок
      holds[oid]?.pinching = true; holds[oid]?.moved = true; h.pinching = true; h.moved = true
      labelOn[v] = true
    }
    holds[f.id] = h
  }

  func move(_ f: Finger) {
    guard var h = holds[f.id], !h.dead else { return }
    h.x = f.x; h.y = f.y
    defer { holds[f.id] = h }
    let p = CGPoint(x: f.x, y: f.y)
    if h.pinching, let v = voiceOf(h.kind), let st = pinches[v], let other = st.ids.first(where: { $0 != f.id }).flatMap({ holds[$0] }) {
      let r = sidPinch(geo, voice: v, base: st.base, d0: st.d0, d: (abs(other.x - f.x), abs(other.y - f.y)))
      send(.voice, v, [Double(Sid.Key.wd), r.wd]); send(.voice, v, [Double(Sid.Key.wa), r.wa])
      return
    }
    guard f.moved else { return }
    switch h.kind {
    case .moon:
      h.moved = true; send(.moon, 0, [sidMoonStep(geo, p)])
    case .star(let v, let i):
      h.moved = true; labelOn[v] = true
      let vo = mirror.voices[v], home = sidHomeFromPoint(geo, voice: v, vo, pt: i, p)
      send(.star, v, [Double(i), vo.stars[i].hd, home.ha])
      if let pr = home.prev { send(.star, v, [Double(pr.pt), pr.hd, vo.stars[pr.pt].ha]) }
    case .band: h.moved = true
    }
  }

  func up(_ f: Finger) {
    guard let h = holds.removeValue(forKey: f.id) else { return }
    if case .moon = h.kind { if !h.moved { goMap?() }; return }
    guard let v = voiceOf(h.kind) else { return }
    if !h.moved && !h.pinching && !h.dead {                                   // короткий тап: звезда — встряска одной звезды, полоса — всего созвездия
      switch h.kind {
      case .star(_, let i): send(.kick, v, [Double(i)]); pulses.append(SidPulse(v: v, i: i, t0: now))
      default: send(.kick, v, [-1]); pulses.append(SidPulse(v: v, i: -1, t0: now))
      }
      if pulses.count > 12 { pulses.removeFirst(pulses.count - 12) }
    }
    if h.pinching, let st = pinches[v] {
      pinches[v] = nil
      for id in st.ids where id != f.id { holds[id]?.pinching = false; holds[id]?.dead = true }
    }
    if !holds.values.contains(where: { voiceOf($0.kind) == v }) { labelOn[v] = false }
  }

  func cancel(_ f: Finger) {
    guard let h = holds.removeValue(forKey: f.id) else { return }
    if let v = voiceOf(h.kind) {
      if h.pinching, let st = pinches[v] { pinches[v] = nil; for id in st.ids where id != f.id { holds[id]?.pinching = false; holds[id]?.dead = true } }
      if !holds.values.contains(where: { voiceOf($0.kind) == v }) { labelOn[v] = false }
    }
  }

  // удержание на звезде или полосе — плашка голоса
  func tickHolds() {
    for (k, h) in holds where !h.moved && !h.pinching && !h.dead && now - h.downAt > Touch.holdMs / 1000 {
      guard let v = voiceOf(h.kind) else { continue }
      holds[k]?.moved = true; openSheet?(voiceSpec(v))
    }
  }
}
