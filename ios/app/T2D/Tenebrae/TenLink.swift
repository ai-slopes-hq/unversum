import UIKit

// Нити Tenebrae: мост двух пальцев, плашка связи (порт app.js:325-349, 525-540, 854-858, 885-982).
// Время — часы тика (now). Связь, которой нет в виде, кольцу и мосту уже не принадлежит: ничего по ней не шлём

// мост: палец fa держит A, палец fb — B; B модулирует A. d0 — расстояние между пальцами на старте, depth0 — глубина связи тогда
struct TenBridge {
  let a, b: Int
  let fa, fb: ObjectIdentifier
  let d0, depth0: Double
  var depth: Double
}

// плашка связи: какую нить правит (dst ← src); finger — палец, что ведёт ползунок глубины
struct TenRing {
  let dst, src: Int
  var finger: ObjectIdentifier?
}

// геометрия плашки связи: шапка (подпись, close), пять фишек (fm tzfm am rm cut), ползунок глубины
struct TenLinkGeo {
  static let height = 134.0
  let x, w, top, bottom, tx0, tx1, sliderY: Double
  let close: CGRect
  let chips: [CGRect]
  init(W: Double, H: Double, safeBot: Double) {
    bottom = H - safeBot - 8; x = 16; w = W - 32; top = bottom - TenLinkGeo.height
    close = CGRect(x: x + w - 64, y: top + 12, width: 64, height: 30)
    let gap = 8.0, cw = (w - 4 * gap) / 5
    let x0 = x, y0 = top + 52
    chips = (0..<5).map { CGRect(x: x0 + Double($0) * (cw + gap), y: y0, width: cw, height: 32) }
    tx0 = x + 20; tx1 = x + w - 20; sliderY = top + 108
  }
}

// точка квадратичной кривой нити при t = 0…1 (bez, app.js:536)
func tenBez(_ c: (x0: Double, y0: Double, cx: Double, cy: Double, x1: Double, y1: Double), _ t: Double) -> (x: Double, y: Double) {
  let u = 1 - t
  return (u * u * c.x0 + 2 * u * t * c.cx + t * t * c.x1, u * u * c.y0 + 2 * u * t * c.cy + t * t * c.y1)
}

extension TenebraeView {
  // ---------- вспомогательное ----------

  func linkMod(_ dst: Int, _ src: Int) -> TenMod? {
    guard (0..<Ten.voices).contains(dst), voice(dst).active else { return nil }
    return voice(dst).mods.first { $0.src == src }
  }
  private func linkAlive(_ r: TenRing) -> Bool { linkMod(r.dst, r.src) != nil }

  // нить связи (linkCurve, app.js:527): квадратичная кривая src → dst, прогнутая к солнцу; глубже связь — сильнее прогиб
  func linkCurve(dst: Int, src: Int) -> (x0: Double, y0: Double, cx: Double, cy: Double, x1: Double, y1: Double) {
    let p1 = screenPos(src), p2 = screenPos(dst)
    let mx = (p1.x + p2.x) / 2, my = (p1.y + p2.y) / 2
    let L = hypot(p2.x - p1.x, p2.y - p1.y)
    let len = L == 0 ? 1 : L
    var nx = -(p2.y - p1.y) / len, ny = (p2.x - p1.x) / len
    if nx * (geo.ox - mx) + ny * (geo.oy - my) < 0 { nx = -nx; ny = -ny }
    let depth = linkMod(dst, src)?.depth ?? 0
    let bend = min(0.3 * L, 64) * (0.5 + 0.5 * tenClamp(depth))
    return (p1.x, p1.y, mx + nx * bend, my + ny * bend, p2.x, p2.y)
  }

  // середина нити — ручка (app.js:539)
  func linkMid(dst: Int, src: Int) -> (x: Double, y: Double) { tenBez(linkCurve(dst: dst, src: src), 0.5) }

  var linkGeo: TenLinkGeo { TenLinkGeo(W: area.W, H: area.H, safeBot: area.safeBot) }

  // глубина по ползунку: x пальца → 0…1
  private func setDepth(_ x: Double) {
    guard let r = ring, linkMod(r.dst, r.src) != nil else { return }
    let g = linkGeo
    send(Act.linkDepth, r.dst, [Double(r.src), tenClamp((x - g.tx0) / (g.tx1 - g.tx0))])
  }

  // ---------- касание: плашка связи (после плашки планеты) ----------

  func linkDown(_ f: Finger) -> Bool {
    guard let p = pt(f) else { return false }
    if var r = ring {                                         // плашка берёт любое касание: внутри — фишка / ползунок, выше — закрыть
      let g = linkGeo
      if p.y < g.top || inBoxPlate(p, g.close) { ring = nil; return true }
      for (k, c) in g.chips.enumerated() where inBoxPlate(p, c) {
        if k == 4 { send(Act.unlink, r.dst, [Double(r.src)]); ring = nil }
        else if let m = linkMod(r.dst, r.src), m.type != k { send(Act.linkType, r.dst, [Double(r.src), Double(k)]) }
        return true
      }
      if p.y >= g.sliderY - 22 { r.finger = f.id; ring = r; setDepth(p.x) }
      return true
    }
    for dst in 0..<Ten.voices where voice(dst).active {       // ручка на середине нити: не auto, источник активен
      for m in voice(dst).mods where voice(m.src).active && !m.auto {
        let c = linkMid(dst: dst, src: m.src)
        if hypot(p.x - c.x, p.y - c.y) < Ten.handleR { openLink(dst: dst, src: m.src, touchTime: f.touchTime); return true }
      }
    }
    return false
  }

  private func inBoxPlate(_ p: (x: Double, y: Double), _ r: CGRect) -> Bool {
    p.x >= r.minX - 4 && p.x <= r.maxX + 4 && p.y >= r.minY - 4 && p.y <= r.maxY + 4
  }

  // тап по ручке: связь хаоса пользователь забирает себе (LINK), открывается плашка
  private func openLink(dst: Int, src: Int, touchTime: Double) {
    send(Act.link, dst, [Double(src)], touchTime: touchTime)
    ring = TenRing(dst: dst, src: src, finger: nil)
  }

  func linkMove(_ f: Finger) {
    guard let r = ring, r.finger == f.id, let p = pt(f) else { return }
    setDepth(p.x)
  }

  func linkUp(_ f: Finger) {
    guard var r = ring, r.finger == f.id else { return }
    r.finger = nil; ring = r
  }

  func linkCancel(_ f: Finger) { linkUp(f) }

  // ---------- мост ----------

  // второй палец на планете b, пока первый держит a одним пальцем: b модулирует a. false — не мост
  func bridgeDown(_ f: Finger, first: ObjectIdentifier, b: Int) -> Bool {
    guard bridge == nil, let p = pt(f), let fa = fingers[first], let a = fa.v, a != b, !fa.opened,
          voice(a).active, voice(b).active, holders(a) == 1, holders(b) == 0 else { return false }
    var t = newFinger(f, p)
    t.v = b; t.caught = true; t.wasBridge = true
    let pp = screenPos(b)
    t.ox = pp.x - p.x; t.oy = pp.y - p.y
    fingers[f.id] = t
    fingers[first]?.wasBridge = true
    if !voice(b).pinned { send(Act.attack, b, touchTime: f.touchTime) }
    send(Act.grab, b)
    send(Act.link, a, [Double(b)])
    let d0 = max(30, hypot(fa.x - p.x, fa.y - p.y)), depth0 = linkMod(a, b)?.depth ?? Ten.linkNew
    bridge = TenBridge(a: a, b: b, fa: first, fb: f.id, d0: d0, depth0: depth0, depth: depth0)
    return true
  }

  func bridgeMove(_ br: TenBridge) {
    guard let fa = fingers[br.fa], let fb = fingers[br.fb], voice(br.a).active, voice(br.b).active else { return }
    let depth = tenClamp(br.depth0 + hypot(fa.x - fb.x, fa.y - fb.y) / br.d0 - 1)
    bridge?.depth = depth
    send(Act.linkDepth, br.a, [Double(br.b), depth])
  }

  // палец моста поднят: тонкая связь рвётся; оставшийся палец ведёт свою планету дальше, без прыжка
  func endBridge(_ br: TenBridge) {
    bridge = nil
    if br.depth < Ten.bridgeCut, voice(br.a).active, voice(br.b).active { send(Act.unlink, br.a, [Double(br.b)]) }
    for id in [br.fa, br.fb] {
      guard let t = fingers[id], let v = t.v, voice(v).active else { continue }
      let pp = screenPos(v)
      fingers[id]?.moved = true
      fingers[id]?.ox = pp.x - t.x; fingers[id]?.oy = pp.y - t.y
    }
  }

  // ---------- тик ----------

  func linkTick(now: Double) {
    if let br = bridge, !voice(br.a).active || !voice(br.b).active { bridge = nil }
    if let r = ring, !linkAlive(r) { ring = nil }
  }

  func linkReset() { bridge = nil; ring = nil }
}
