import Foundation

// плашка голоса (удержание на полосе или звезде): нота, число звёзд, уровень, панорама, ширина и высота рамок, закон шага.
// Значения — из зеркала, запись — те же VOICE-действия, что у жестов
extension SideraView {
  static let rowKeys = ["note", "oct", "n", "lvl", "pan", "wd", "wa", "dist"]
  private static let labels = ["note": "nota", "oct": "octava", "n": "stellae", "lvl": "level", "pan": "panorama", "wd": "latitudo", "wa": "altitudo", "dist": "lex"]
  private static let keyOf = ["note": Sid.Key.note, "oct": Sid.Key.oct, "n": Sid.Key.n, "lvl": Sid.Key.lvl, "pan": Sid.Key.pan, "wd": Sid.Key.wd, "wa": Sid.Key.wa, "dist": Sid.Key.dist]
  private static let lfoIndex = ["lvl": 0, "pan": 1, "wd": 2, "wa": 3]

  func voiceSpec(_ v: Int) -> SheetSpec {
    let v = min(max(v, 0), Sid.nv - 1)
    func pct(_ x: Double) -> String { "\(Int((Sid.clamp(x) * 100).rounded()))%" }
    func send(_ key: String, _ val: Double) { core.action(SidAct.voice.rawValue, v, [Double(SideraView.keyOf[key] ?? 0), val], touchTime: 0) }
    return SheetSpec(
      title: "stella \(v + 1)", hint: "voice — tap outside to close",
      rows: SideraView.rowKeys.map { SheetRow(key: $0, label: SideraView.labels[$0] ?? $0, code: SideraView.lfoIndex[$0].map { Sid.Lfo.code(v, $0) } ?? -1) },
      get: { [unowned self] k in
        let m = mirror.voices[v]
        switch k {
        case "note": return Sid.clamp(m.note / 60)
        case "oct": return Double(m.oct + 2) / 4
        case "n": return Sid.clamp(Double(m.n - Sid.nptMin) / Double(Sid.nptMax - Sid.nptMin))
        case "lvl": return m.lvl
        case "pan": return Sid.clamp((m.pan + 1) / 2)
        case "wd": return m.wd
        case "wa": return m.wa
        default: return Sid.clamp(Double(m.dist) / 2)
        }
      },
      set: { k, x in
        let x = Sid.clamp(x)
        switch k {
        case "note": send(k, (x * 60).rounded())
        case "oct": send(k, (x * 4).rounded() - 2)
        case "n": send(k, (Double(Sid.nptMin) + x * Double(Sid.nptMax - Sid.nptMin)).rounded())
        case "pan": send(k, 2 * x - 1)
        case "dist": send(k, Double(min(2, Int(x * 3))))
        default: send(k, x)
        }
      },
      text: { [unowned self] k in
        let m = mirror.voices[v]
        switch k {
        case "note": return sidNoteName(m.note)
        case "oct": return m.oct == 0 ? "0" : (m.oct > 0 ? "+\(m.oct)" : "\(m.oct)")
        case "n": return "\(m.n) stellae"
        case "lvl": return pct(m.lvl)
        case "pan": return abs(m.pan) < 0.05 ? "centrum" : (m.pan < 0 ? "sinistra \(pct(-m.pan))" : "dextra \(pct(m.pan))")
        case "wd": return pct(m.wd)
        case "wa": return pct(m.wa)
        default: return Sid.distNames[min(max(m.dist, 0), 2)]
        }
      },
      lfo: SheetLfo(obj: -1, keys: ["lvl", "pan", "wd", "wa"], shape: 1),
      reset: nil, arc: nil,
      tap: { [unowned self] k in
        guard k == "dist" else { return }
        send(k, Double((mirror.voices[v].dist + 1) % 3))
      })
  }

  // веер FX (кнопка fx): drift — вторичное блуждание рамок, space — посыл в ревербер, tone — яркость
  static let fxRows = ["drift", "space", "tone", "cutoff", "reso"]
  private static let fxLabels = ["drift": "motus", "space": "spatium", "tone": "tonus", "cutoff": "filtrum", "reso": "resonantia"]

  var fxSpec: SheetSpec {
    func idx(_ k: String) -> Int { SideraView.fxRows.firstIndex(of: k) ?? 0 }
    func pct(_ x: Double) -> String { "\(Int((Sid.clamp(x) * 100).rounded()))%" }
    return SheetSpec(
      title: "astra", hint: "drift, space, tone, master filter — tap outside to close",
      rows: SideraView.fxRows.map { SheetRow(key: $0, label: SideraView.fxLabels[$0] ?? $0, code: Sid.Fx.lfoBase + idx($0)) },
      get: { [unowned self] k in mirror.fx[idx(k)] },
      set: { [unowned self] k, x in core.action(SidAct.fx.rawValue, 0, [Double(idx(k)), Sid.clamp(x)], touchTime: 0) },
      text: { [unowned self] k in
        let x = mirror.fx[idx(k)]
        if k == "drift" && x < 0.02 { return "still" }
        if k == "cutoff" { return "\(Int((30 * pow(600.0, x)).rounded())) Hz" }
        return pct(x)
      },
      lfo: SheetLfo(obj: -1, keys: Set(SideraView.fxRows), shape: 1),
      reset: nil, arc: nil, tap: nil)
  }
}
