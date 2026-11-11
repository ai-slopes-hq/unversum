import Foundation

// плашки Paradisus: веер fx (шиммер, собор, Gloria, вибрато, планеты) и плашка закреплённого ангела (удержание); значения — из зеркала,
// запись — те же действия ядра, что у жестов (ACT FX a = [FxKey, v], ACT ANGEL i = id, a = [AngelKey, v])
extension ParadisusView {
  struct FxRow { let key, label: String; let idx: Int }
  // порядок и номера — FxKey ядра (Par.fxKeys): shimmer decay tone mix cathedral gloria vibrato orbits
  static let fxRows: [FxRow] = [
    FxRow(key: "shimmer", label: "shimmer", idx: 0), FxRow(key: "decay", label: "decay", idx: 1), FxRow(key: "tone", label: "tone", idx: 2),
    FxRow(key: "mix", label: "mix", idx: 3), FxRow(key: "cathedral", label: "aether", idx: 4), FxRow(key: "gloria", label: "gloria", idx: 5),
    FxRow(key: "vibrato", label: "vibrato", idx: 6), FxRow(key: "orbits", label: "orbits", idx: 7),
  ]
  static let cathedralIdx = 4
  // AngelKey ядра: singers detune vib breath reg
  static let angelRows: [(key: String, label: String, idx: Int)] = [
    ("singers", "choir", 0), ("detune", "detune", 1), ("vib", "vibrato", 2), ("breath", "breath", 3), ("reg", "register", 4),
  ]

  private func pct(_ v: Double) -> String { "\(v.isFinite ? Int((Par.clamp(v) * 100).rounded()) : 0)%" }

  var fxSpec: SheetSpec {
    let rows = ParadisusView.fxRows
    func idx(_ k: String) -> Int { rows.first { $0.key == k }?.idx ?? 0 }
    return SheetSpec(
      title: "paradisus", hint: "FX — tap outside to close",
      rows: rows.map { SheetRow(key: $0.key, label: $0.label, code: $0.idx) },
      get: { [unowned self] k in mirror.fx[idx(k)] },
      set: { [unowned self] k, v in core.action(Act.fx.rawValue, 0, [Double(idx(k)), Par.clamp(v)], touchTime: 0) },
      text: { [unowned self] k in idx(k) == 7 && mirror.fx[7] <= 0.01 ? "off" : pct(mirror.fx[idx(k)]) },
      lfo: SheetLfo(obj: -1, keys: Set(rows.filter { $0.idx != ParadisusView.cathedralIdx }.map(\.key)), shape: 0),   // собор — без LFO, как в вебе
      reset: nil,
      arc: { [unowned self] in mirror.fx[ParadisusView.cathedralIdx] })
  }

  // ---- закреплённый ангел (удержание, не двигая) ----
  func angelSpec(_ id: Int) -> SheetSpec {
    let rows = ParadisusView.angelRows
    func row(_ k: String) -> Int { rows.first { $0.key == k }?.idx ?? 0 }
    func angel() -> ParAngel? { mirror.angels.first { $0.id == id } }
    let name = angel().map { "\(ParLook.name($0.k)) · \(ParLook.harm($0.k))" } ?? "angelus"
    return SheetSpec(
      title: name, hint: "angel — tap outside to close",
      rows: rows.map { SheetRow(key: $0.key, label: $0.label, code: -1) },
      get: { k in
        guard let a = angel() else { return 0 }
        switch row(k) {
        case 0: return (Double(a.singers) - 0.5) / 5
        case 1: return a.detune
        case 2: return a.vib
        case 3: return a.breath
        default: return (Double(a.reg) + 0.5) / 3
        }
      },
      set: { [self] k, v in core.action(Act.angel.rawValue, id, [Double(row(k)), Par.clamp(v)], touchTime: 0) },
      text: { [self] k in
        guard let a = angel() else { return "" }
        switch row(k) {
        case 0: return "\(a.singers) \(a.singers == 1 ? "voice" : "voices")"
        case 1: return pct(a.detune)
        case 2: return pct(a.vib)
        case 3: return pct(a.breath)
        default: return ["octave down", "natural", "octave up"][min(2, max(0, a.reg))]
        }
      },
      lfo: nil, reset: nil, arc: nil, obj: id)
  }
}
