import Foundation

// плашки Hymenaeus: веер fx (облака и нить) и плашка кольца (удержание на кольце); значения — только из зеркала, запись — те же действия, что у жестов
extension HymenaeusView {
  struct HymRow { let key, label: String; let kind: Kind; let idx: Int; enum Kind { case cloud, ring, thread, mix } }
  static let ringNK = 5                                                                  // NRK ядра (RingKey: STRUCT BRIGHT DAMP LVL POLY)
  static let cloudLfoBase = Hym.nr * HymenaeusView.ringNK                               // ключи LFO облаков идут после колец (10)
  static let lenMin = 4, lenMax = Hym.pearlMax

  static let cloudRows: [HymRow] = [
    HymRow(key: "pos", label: "positio", kind: .cloud, idx: Hym.Cloud.pos), HymRow(key: "size", label: "granum", kind: .cloud, idx: Hym.Cloud.size),
    HymRow(key: "dens", label: "densitas", kind: .cloud, idx: Hym.Cloud.dens), HymRow(key: "pitch", label: "altitudo", kind: .cloud, idx: Hym.Cloud.pitch),
    HymRow(key: "freeze", label: "glacies", kind: .cloud, idx: Hym.Cloud.freeze), HymRow(key: "echo", label: "echo", kind: .cloud, idx: Hym.Cloud.echo),
    HymRow(key: "level", label: "level", kind: .cloud, idx: Hym.Cloud.level), HymRow(key: "send", label: "aether", kind: .cloud, idx: Hym.Cloud.send),
    HymRow(key: "mix", label: "mixtura", kind: .mix, idx: 0),
  ]
  static let ringRows: [HymRow] = [
    HymRow(key: "structure", label: "structura", kind: .ring, idx: Hym.Ring.structure), HymRow(key: "bright", label: "claritas", kind: .ring, idx: Hym.Ring.bright),
    HymRow(key: "damp", label: "damping", kind: .ring, idx: Hym.Ring.damp), HymRow(key: "lvl", label: "level", kind: .ring, idx: Hym.Ring.lvl),
    HymRow(key: "poly", label: "voces", kind: .ring, idx: Hym.Ring.poly),
    HymRow(key: "len", label: "margaritae", kind: .thread, idx: Hym.Thread.len),      // Task 6: слайдер диапазона lo/hi своей нити
  ]

  private func pct(_ v: Double) -> String { "\(v.isFinite ? Int((Hym.clamp(v) * 100).rounded()) : 0)%" }
  static func lenFrom(_ v: Double) -> Int { min(lenMax, max(lenMin, Int((Double(lenMin) + Hym.clamp(v) * Double(lenMax - lenMin)).rounded()))) }

  // ---- облака и нить ----
  var fxSpec: SheetSpec {
    let rows = HymenaeusView.cloudRows
    func row(_ k: String) -> HymRow { rows.first { $0.key == k } ?? rows[0] }
    return SheetSpec(
      title: "nubes", hint: "clouds and mix — tap outside to close",
      rows: rows.map { r in
        SheetRow(key: r.key, label: r.label, code: r.kind == .cloud && r.idx != Hym.Cloud.freeze ? HymenaeusView.cloudLfoBase + r.idx : -1)
      },
      get: { [unowned self] k in
        let r = row(k)
        switch r.kind {
        case .cloud: return mirror.cloud[r.idx]
        case .mix: return mirror.mix
        case .thread, .ring: return 0
        }
      },
      set: { [unowned self] k, v in
        let r = row(k)
        switch r.kind {
        case .cloud: core.action(Act.cloud.rawValue, 0, [Double(r.idx), r.idx == Hym.Cloud.freeze ? (v >= 0.5 ? 1 : 0) : Hym.clamp(v)], touchTime: 0)
        case .mix: core.action(Act.mix.rawValue, 0, [Hym.clamp(v)], touchTime: 0)
        case .thread, .ring: break
        }
      },
      text: { [unowned self] k in
        let r = row(k)
        switch r.kind {
        case .cloud: return r.idx == Hym.Cloud.freeze ? (mirror.cloud[r.idx] >= 0.5 ? "frozen" : "flowing") : pct(mirror.cloud[r.idx])
        case .mix: return pct(mirror.mix)
        case .thread, .ring: return ""
        }
      },
      lfo: SheetLfo(obj: -1, keys: Set(rows.filter { $0.kind == .cloud && $0.idx != Hym.Cloud.freeze }.map(\.key)), shape: 1),
      reset: nil,
      arc: { [unowned self] in mirror.cloud[Hym.Cloud.echo] },
      tap: { [unowned self] k in
        guard k == "freeze" else { return }
        core.action(Act.cloud.rawValue, 0, [Double(Hym.Cloud.freeze), mirror.cloud[Hym.Cloud.freeze] >= 0.5 ? 0 : 1], touchTime: 0)
      })
  }

  // ---- кольцо (удержание на ободе) ----
  func ringSpec(_ i: Int) -> SheetSpec {
    let rows = HymenaeusView.ringRows
    func row(_ k: String) -> HymRow { rows.first { $0.key == k } ?? rows[0] }
    return SheetSpec(
      title: Hym.ringNames[i], hint: "ring — tap outside to close",
      rows: rows.map { r in SheetRow(key: r.key, label: r.label, code: r.kind == .ring && r.idx < Hym.Ring.poly ? i * HymenaeusView.ringNK + r.idx : -1) },
      get: { [unowned self] k in
        let q = mirror.rings[i], r = row(k)
        if r.kind == .thread { return Hym.clamp(Double(mirror.len[i] - HymenaeusView.lenMin) / Double(HymenaeusView.lenMax - HymenaeusView.lenMin)) }
        switch r.idx {
        case Hym.Ring.structure: return q.structure
        case Hym.Ring.bright: return q.bright
        case Hym.Ring.damp: return q.damp
        case Hym.Ring.poly: return q.poly
        default: return q.lvl
        }
      },
      set: { [unowned self] k, v in
        let r = row(k)
        if r.kind == .thread { core.action(Act.thread.rawValue, i, [Double(Hym.Thread.len), Double(HymenaeusView.lenFrom(v))], touchTime: 0); return }
        let val = r.idx == Hym.Ring.poly ? [0.0, 0.5, 1.0][min(2, Int(Hym.clamp(v) * 3))] : Hym.clamp(v)
        core.action(Act.ring.rawValue, i, [Double(r.idx), val], touchTime: 0)
      },
      text: { [unowned self] k in
        let q = mirror.rings[i], r = row(k)
        if r.kind == .thread { return "\(mirror.len[i]) pearls" }
        switch r.idx {
        case Hym.Ring.poly: return "\(q.voices)"
        case Hym.Ring.structure: return pct(q.structure)
        case Hym.Ring.bright: return pct(q.bright)
        case Hym.Ring.damp: return pct(q.damp)
        default: return pct(q.lvl)
        }
      },
      lfo: SheetLfo(obj: -1, keys: ["structure", "bright", "damp", "lvl"], shape: 1),
      reset: nil, arc: nil,
      tap: { [unowned self] k in
        let r = row(k), q = mirror.rings[i]
        if r.kind == .ring && r.idx == Hym.Ring.poly { core.action(Act.ring.rawValue, i, [Double(r.idx), [0.0, 0.5, 1.0][(q.voices == 1 ? 1 : q.voices == 2 ? 2 : 0)]], touchTime: 0) }
      })
  }
}
