import Foundation

// зеркало Paradisus (MIRROR_N = 188) — core/worlds/paradisus/world.hpp:
// [0] n ангелов [1] закреплено [2] orbitT [3..10] fx в порядке FxKey [11] уровень сухого звука (солнце);
// дальше по 11 чисел на ангела j<n в порядке создания: id, k, v, vEff, pinned, lvl, singers, detune, vib, breath, reg
enum Par {
  static let maxAngel = 16, head = 12, per = 11, nSph = 9
  static let mirrorN = head + maxAngel * per   // 188
  static let fxKeys = ["shimmer", "decay", "tone", "mix", "cathedral", "gloria", "vibrato", "orbits"]   // FxKey ядра 0…7
  static func clamp(_ v: Double, _ lo: Double = 0, _ hi: Double = 1) -> Double { v.isFinite ? min(hi, max(lo, v)) : lo }

  // ангел j из буфера зеркала; за пределами буфера — пустой
  static func angel(_ m: [Double], _ j: Int) -> ParAngel {
    let o = head + j * per
    guard j >= 0, j < maxAngel, m.count >= o + per else { return ParAngel() }
    return ParAngel(id: Int(clamp(m[o], -1e9, 1e9)), k: Int(clamp(m[o + 1].rounded(), 0, 8)), v: clamp(m[o + 2], 0, 5), vEff: clamp(m[o + 3], 0, 5),
                    pinned: m[o + 4] != 0, lvl: clamp(m[o + 5]), singers: Int(clamp(m[o + 6].rounded(), 1, 5)), detune: clamp(m[o + 7]),
                    vib: clamp(m[o + 8]), breath: clamp(m[o + 9]), reg: Int(clamp(m[o + 10].rounded(), 0, 2)))
  }
}

// действия ядра — те же номера, что enum Act в world.hpp (сверяет ParadisusTests); новые — только в конец
enum ParAct: Int { case touch, release, pin, vowel, rose, fx, angel, dice, clear }

struct ParAngel: Equatable {
  var id = 0, k = 0
  var v = 0.0, vEff = 0.0                    // гласная как задана и где ангел поёт (у закреплённого дрейфует)
  var pinned = false
  var lvl = 0.0                              // 0…1, растёт, пока строится голос
  var singers = 1                            // 1…5
  var detune = 0.0, vib = 0.0, breath = 0.0
  var reg = 1                                // 0 / 1 / 2 → ×0.5 / ×1 / ×2
}

struct ParMirror {
  var angels: [ParAngel] = []
  var pinnedCount = 0
  var orbitT = 0.0
  var fx = [Double](repeating: 0, count: 8)  // по порядку Par.fxKeys
  var level = 0.0                            // сухой выход 0…1 — «солнце»

  init() {}
  init(_ m: [Double]) { load(m) }

  // короткий или битый буфер — пусто; нечисла зажимаются
  @discardableResult mutating func load(_ m: [Double]) -> Bool {
    guard m.count >= Par.mirrorN else { self = ParMirror(); return false }
    let n = Int(Par.clamp(m[0].rounded(), 0, Double(Par.maxAngel)))
    pinnedCount = Int(Par.clamp(m[1].rounded(), 0, Double(Par.maxAngel)))
    orbitT = m[2].isFinite ? m[2] : 0
    for i in 0..<8 { fx[i] = Par.clamp(m[3 + i]) }
    level = Par.clamp(m[11])
    angels = (0..<n).map { Par.angel(m, $0) }
    return true
  }

  // фаза планеты сферы k на орбите (0…1, по часовой от верха) — как в ядре
  func planet(_ k: Int) -> Double {
    let x = orbitT * Double(9 - k) + 0.13 * Double(k)
    return x - x.rounded(.down)
  }
}
