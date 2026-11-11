import UIKit

// окно линии: голова бежит по ломаной (звезда i → i+1, по кругу), видны последние count отрезков; alpha растёт от хвоста к голове
enum SidWindow {
  static func segments(head: Double, n: Int, count: Int) -> [(from: Int, to: Int, alpha: Double)] {
    guard n > 0 else { return [] }
    let cnt = max(1, min(count, n))
    let h = head.isFinite ? Int(fmod(head, Double(n)).rounded(.down)) : 0
    let cur = ((h % n) + n) % n
    return (0..<cnt).map { j in
      let from = ((cur - (cnt - 1 - j)) % n + n) % n
      return (from, (from + 1) % n, Double(j + 1) / Double(cnt))
    }
  }
}

// фаза луны 0…1: 0 и 1 — полнолуние, 0,5 — затмение; блуждание — расстояние от затмения, 0…1
func sidWander(_ phase: Double) -> Double { phase.isFinite ? abs(2 * Sid.clamp(phase) - 1) : 0 }
// ascend (фаза > 0,5): голова бежит вперёд по ломаной, descend — назад
func sidMoonName(_ phase: Double) -> String {
  let w = sidWander(phase)
  if w < 0.08 { return "eclipsis" }
  if w > 0.92 { return "luna plena" }
  return phase > 0.5 ? "ascendens" : "descendens"
}
// скорость головы, отрезков в секунду, со знаком: спокойная (1…2 от блуждания), вперёд при ascend, назад при descend
func sidHeadRate(phase: Double) -> Double { (1 + sidWander(phase)) * (phase.isFinite && phase < 0.5 ? -1 : 1) }

// нота голоса: полутоны от A1 (55 Гц) → имя и октава (MIDI 33 + полутоны)
func sidNoteName(_ semi: Double) -> String {
  let names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
  let midi = 33 + Int((semi.isFinite ? semi : 0).rounded())
  return names[((midi % 12) + 12) % 12] + String(midi / 12 - 1)
}

extension SideraView {
  // фоновые звёзды: детерминированные, едва заметные
  private func backStars(_ d: Draw) {
    var x: UInt32 = 2463534242
    func rnd() -> CGFloat { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return CGFloat(x % 10000) / 10000 }
    for _ in 0..<70 { let px = rnd() * geo.W, py = rnd() * geo.H, a = 0.06 + rnd() * 0.12; d.dot(px, py, 0.7 * geo.S + 0.2, a) }
  }

  private func drawVoice(_ d: Draw, _ v: Int, _ now: Double) {
    let G = geo, g = d.g, voice = mirror.voices[v], S = G.S
    let r = G.bandRect(v), lv = CGFloat(0.35 + 0.65 * min(1, voice.lvl * 1.6))     // тише голос — тусклее созвездие
    let n = voice.n
    // окно линии
    let head = heads[v], win = SidWindow.segments(head: head, n: n, count: 3)
    let frac = CGFloat(head.isFinite ? head - head.rounded(.down) : 0)
    var inWin = Set<Int>()
    for (j, s) in win.enumerated() {
      inWin.insert(s.from); inWin.insert(s.to)
      let a = G.starPoint(v, voice, s.from), b = G.starPoint(v, voice, s.to)
      let isHead = j == win.count - 1
      let end = isHead ? CGPoint(x: a.x + (b.x - a.x) * frac, y: a.y + (b.y - a.y) * frac) : b     // голова дорисована до своей доли
      if isHead { inWin.remove(s.to) }
      let pieces = 6
      for k in 0..<pieces {                                                                        // градиент — шесть кусков с растущей яркостью
        let t0 = CGFloat(k) / CGFloat(pieces), t1 = CGFloat(k + 1) / CGFloat(pieces)
        let base = CGFloat(s.alpha) - 1 / CGFloat(win.count) + t1 / CGFloat(win.count)               // 0…1 вдоль всего окна
        let al = (0.04 + 0.9 * pow(base, 1.5)) * lv
        let p0 = CGPoint(x: a.x + (end.x - a.x) * t0, y: a.y + (end.y - a.y) * t0), p1 = CGPoint(x: a.x + (end.x - a.x) * t1, y: a.y + (end.y - a.y) * t1)
        g.setStrokeColor(d.ink(0.07 * al)); g.setLineWidth(5 * S); g.setLineCap(.round)           // бледное свечение — широкий штрих, без shadowBlur
        g.beginPath(); g.move(to: p0); g.addLine(to: p1); g.strokePath()
        g.setStrokeColor(d.ink(al)); g.setLineWidth(1.3 * S)
        g.beginPath(); g.move(to: p0); g.addLine(to: p1); g.strokePath()
      }
    }
    g.setLineCap(.butt)
    if holds.values.contains(where: { !$0.dead && voiceOf($0.kind) == v }) { drawOutline(d, v, voice) }   // тронули группу — золотой контур
    // звёзды
    for i in 0..<n {
      let p = G.starPoint(v, voice, i), on = inWin.contains(i)
      d.dot(p.x, p.y, (on ? 2.4 : 1.7) * S, (on ? 1 : 0.5) * lv)
      if on { d.ring(p.x, p.y, 5.5 * S, 0.55 * lv, 1) }
      else {
        g.setStrokeColor(d.ink(0.3 * lv)); g.setLineWidth(1); g.beginPath()
        g.move(to: CGPoint(x: p.x - 4 * S, y: p.y)); g.addLine(to: CGPoint(x: p.x + 4 * S, y: p.y))
        g.move(to: CGPoint(x: p.x, y: p.y - 4 * S)); g.addLine(to: CGPoint(x: p.x, y: p.y + 4 * S)); g.strokePath()
      }
    }
    for pl in pulses where pl.v == v {                                                            // встряска: кольцо расходится и гаснет за 0,6 с
      let t = CGFloat((now - pl.t0) / 0.6)
      guard t >= 0, t < 1 else { continue }
      for i in (pl.i >= 0 ? [pl.i] : Array(0..<n)) where i < n {
        let p = G.starPoint(v, voice, i)
        d.ring(p.x, p.y, (4 + 22 * t) * S, 0.7 * (1 - t) * lv, 1)
      }
    }
    if labelOn[v] {                                                                               // подпись — только пока жест идёт
      d.mapText("\(sidNoteName(voice.note)) · \(voice.n)", r.maxX - 8, r.minY + 14, 12, 0.8, align: .right)
    }
  }

  // контур созвездия под пальцем: золотая замкнутая кривая через раздутую оболочку звёзд (не прямоугольник)
  private func drawOutline(_ d: Draw, _ v: Int, _ voice: SidVoice) {
    let G = geo, g = d.g
    let pts = sidOutline((0..<voice.n).map { G.starPoint(v, voice, $0) }, pad: 16 * G.S)
    guard pts.count >= 3 else { return }
    let m = pts.count
    func mid(_ a: CGPoint, _ b: CGPoint) -> CGPoint { CGPoint(x: (a.x + b.x) / 2, y: (a.y + b.y) / 2) }
    g.beginPath(); g.move(to: mid(pts[m - 1], pts[0]))
    for i in 0..<m { g.addQuadCurve(to: mid(pts[i], pts[(i + 1) % m]), control: pts[i]) }     // сглаживание: середины сторон, углы — опорные
    g.closePath()
    g.setStrokeColor(Draw.color(Tria.gold, 0.12)); g.setLineWidth(5 * G.S); g.setLineJoin(.round); g.strokePath()
    g.beginPath(); g.move(to: mid(pts[m - 1], pts[0]))
    for i in 0..<m { g.addQuadCurve(to: mid(pts[i], pts[(i + 1) % m]), control: pts[i]) }
    g.closePath()
    g.setStrokeColor(Draw.color(Tria.gold, 0.85)); g.setLineWidth(1.2 * G.S); g.strokePath()
  }

  // луна: тёмный круг стоит по центру, золотой ездит за пальцем. Золотой под тёмным: рядом — серп (ascend справа, descend слева),
  // друг на друге — затмение (только золотая корона), врозь — полнолуние. Свечение и яркость золота — уровень выхода
  private func drawMoon(_ d: Draw) {
    let G = geo, g = d.g, c = G.moon.c, r = G.moon.r
    let lv = CGFloat(min(1, mirror.level * 3)), ph = CGFloat(Sid.clamp(mirror.step))
    let off = (ph - 0.5) * 2 * G.moonSpan, gc = CGPoint(x: c.x + off, y: c.y)
    let w = CGFloat(sidWander(Double(ph)))
    g.setStrokeColor(d.ink(0.12)); g.setLineWidth(1); g.beginPath()                                // ход золотого круга
    g.move(to: CGPoint(x: c.x - G.moonSpan, y: c.y)); g.addLine(to: CGPoint(x: c.x + G.moonSpan, y: c.y)); g.strokePath()
    d.glowAt(gc.x, gc.y, r * (1.3 + lv * 0.4), (0.04 + 0.2 * lv) * (0.3 + 0.7 * w))
    g.setFillColor(Draw.color(Tria.gold, 0.55 + 0.4 * lv)); g.fillEllipse(in: CGRect(x: gc.x - r, y: gc.y - r, width: 2 * r, height: 2 * r))
    g.setStrokeColor(Draw.color(Tria.gold, 0.9)); g.setLineWidth(1.2); g.strokeEllipse(in: CGRect(x: gc.x - r, y: gc.y - r, width: 2 * r, height: 2 * r))
    let dark = CGRect(x: c.x - r, y: c.y - r, width: 2 * r, height: 2 * r)
    g.setFillColor(theme.bg.cgColor); g.fillEllipse(in: dark)                                      // тёмный круг закрывает золотой
    g.setFillColor(d.ink(0.07)); g.fillEllipse(in: dark)
    g.setStrokeColor(d.ink(0.6)); g.setLineWidth(1.2); g.strokeEllipse(in: dark)
    if w < 0.5 {                                                                                   // затмение: корона вокруг тёмного
      g.setStrokeColor(Draw.color(Tria.gold, 0.6 * (1 - 2 * w))); g.setLineWidth(2.2); g.strokeEllipse(in: dark.insetBy(dx: -1.5, dy: -1.5))
    }
    if holds.values.contains(where: { if case .moon = $0.kind { return true } else { return false } }) {   // имя фазы — пока тянешь
      d.mapText(sidMoonName(Double(ph)), c.x, c.y + r + 16, 12, 0.8, align: .center)
    }
  }

  func frame(_ d: Draw, _ now: Double, moon: Bool) {
    let G = geo
    backStars(d)
    if moon { drawMoon(d) }                                                                        // под полем: тёмный диск не стирает подписи
    pulses.removeAll { now - $0.t0 > 0.6 }
    for v in 0..<Sid.nv { drawVoice(d, v, now) }
  }
}
