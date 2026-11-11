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

// скорость головы, отрезков в секунду: спокойная, привязана к фазе луны (1…2)
func sidHeadRate(step: Double) -> Double { 1 + Sid.clamp(step) }

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

  private func drawVoice(_ d: Draw, _ v: Int) {
    let G = geo, g = d.g, voice = mirror.voices[v], S = G.S
    let r = G.bandRect(v), lv = CGFloat(0.35 + 0.65 * min(1, voice.lvl * 1.6))     // тише голос — тусклее созвездие
    d.hairBox(r.minX, r.minY, r.width, r.height, 0.16)
    g.setStrokeColor(d.ink(0.07)); g.setLineWidth(1); g.beginPath()
    g.move(to: CGPoint(x: r.minX + 4, y: G.inner(v).midY)); g.addLine(to: CGPoint(x: r.maxX - 4, y: G.inner(v).midY)); g.strokePath()
    let n = voice.n
    // рамки (пунктиром) и следы от центра к звезде
    g.setLineWidth(1); g.setLineDash(phase: 0, lengths: [2, 3])
    for i in 0..<n {
      let b = G.boxRect(v, voice, i)
      g.setStrokeColor(d.ink(0.13 * lv)); g.stroke(b)
    }
    g.setLineDash(phase: 0, lengths: [])
    for i in 0..<n {
      let c = G.homePoint(v, voice, i), p = G.starPoint(v, voice, i)
      g.setStrokeColor(d.ink(0.16 * lv)); g.beginPath(); g.move(to: c); g.addLine(to: p); g.strokePath()
    }
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
    if labelOn[v] {                                                                               // подпись — только пока жест идёт
      d.mapText("\(sidNoteName(voice.note)) · \(voice.n)", r.maxX - 8, r.minY + 14, 12, 0.8, align: .right)
    }
  }

  // луна: четверть диска в углу; фаза — шаг блужданий (серп → полнолуние), свечение и толщина серпа — уровень выхода
  private func drawMoon(_ d: Draw) {
    let G = geo, g = d.g, c = G.moon.c, r = G.moon.r
    let lv = CGFloat(min(1, mirror.level * 3)), p = CGFloat(Sid.clamp(mirror.step))
    d.glowAt(c.x + r * 0.45, c.y + r * 0.45, r * (1.3 + lv * 0.6), 0.05 + 0.3 * lv)
    g.saveGState()
    g.clip(to: CGRect(x: c.x, y: c.y, width: r * 3, height: r * 3))                               // видна четверть вправо-вниз
    g.setFillColor(d.ink(0.12 + 0.6 * lv)); g.fillEllipse(in: CGRect(x: c.x - r, y: c.y - r, width: 2 * r, height: 2 * r))
    let cut = r * 1.75 * (1 - p) * (1 - 0.4 * lv)                                                 // сдвиг тени: 0 — полный диск
    g.setFillColor(theme.bg.cgColor)
    g.fillEllipse(in: CGRect(x: c.x - r + cut * 0.7, y: c.y - r + cut * 0.7, width: 2 * r, height: 2 * r))
    g.setStrokeColor(d.ink(0.85)); g.setLineWidth(1.4); g.strokeEllipse(in: CGRect(x: c.x - r, y: c.y - r, width: 2 * r, height: 2 * r))
    g.restoreGState()
    // дуга фазы: за неё тянут луну; точка — где сейчас шаг
    g.setStrokeColor(d.ink(0.2)); g.setLineWidth(1)
    g.beginPath(); g.addArc(center: c, radius: r * 1.28, startAngle: 0, endAngle: .pi / 2, clockwise: false); g.strokePath()
    let ang = Double(p) * .pi / 2
    d.dot(c.x + r * 1.28 * CGFloat(cos(ang)), c.y + r * 1.28 * CGFloat(sin(ang)), 2 * G.S + 0.6, 0.7)
  }

  func frame(_ d: Draw, _ now: Double, moon: Bool) {
    let G = geo
    backStars(d)
    if moon { drawMoon(d) }                                                                        // под полем: тёмный диск не стирает рамки и подписи
    let f = G.field
    d.hairBox(f.minX - 6, f.minY - 6, f.width + 12, f.height + 12, 0.28)                          // двойная рамка
    d.hairBox(f.minX - 3, f.minY - 3, f.width + 6, f.height + 6, 0.12)
    for v in 0..<Sid.nv { drawVoice(d, v) }
  }
}
