import Foundation

// зеркало Hymenaeus (MIRROR_N = 96) — core/worlds/hymenaeus/world.hpp:
// [0] run [1] bpm [2] mix; [3..4] cursor, [5..6] len, [7..8] lo, [9..10] hi (по полю на нить; MIRROR_HEAD = 11);
// 2 нити × 16 × (высота, пауза); 2 кольца × (struct, bright, damp, lvl, poly, энергия); 8 значений облаков; энергия облаков
enum Hym {
  static let nr = 2, pearlMax = 16, nck = 8, head = 11, ringN = 6
  static let mirrorN = head + nr * 2 * pearlMax + nr * ringN + nck + 1   // 96
  static let ink: RGB = (44, 38, 30), gold: RGB = (168, 124, 48), ivory: RGB = (242, 237, 226)
  static let roman = ["I", "II"]
  static let ringNames = ["resonans", "chorda"]                          // нить t играет кольцо t
  enum Cloud { static let pos = 0, size = 1, dens = 2, pitch = 3, freeze = 4, echo = 5, level = 6, send = 7 }   // CloudKey ядра
  enum Ring { static let structure = 0, bright = 1, damp = 2, lvl = 3, poly = 4 }                                // RingKey ядра (NRK = 5)
  enum Thread { static let len = 0 }                                                                             // ключи THREAD (1 lo, 2 hi интерфейс не шлёт)
  static func clamp(_ v: Double, _ lo: Double = 0, _ hi: Double = 1) -> Double { v.isFinite ? min(hi, max(lo, v)) : lo }
}

// действия ядра — те же номера, что enum Act в world.hpp (сверяет HymenaeusTests)
enum HymAct: Int { case ring, cloud, pearl, strike, thread, dice, clear, run, bpm, timbre, mix }

struct HymRing {
  var structure = 0.5, bright = 0.5, damp = 0.5, lvl = 0.8, poly = 0.0, energy = 0.0   // poly 0…1 → 1/2/4 голоса
  var voices: Int { poly < 1.0 / 3 ? 1 : poly < 2.0 / 3 ? 2 : 4 }
}

struct HymMirror {
  var run = false
  var bpm = 0.0, mix = 0.5
  var cursor = [Int](repeating: 0, count: Hym.nr), len = [Int](repeating: 0, count: Hym.nr)
  var lo = [Int](repeating: 0, count: Hym.nr), hi = [Int](repeating: 0, count: Hym.nr)   // lo / hi — ноты MIDI
  var pearls = [[(pitch: Double, rest: Bool)]](repeating: [(pitch: Double, rest: Bool)](repeating: (0.5, false), count: Hym.pearlMax), count: Hym.nr)
  var rings = [HymRing](repeating: HymRing(), count: Hym.nr)
  var cloud = [Double](repeating: 0, count: Hym.nck)
  var cloudEnergy = 0.0

  init() {}
  static let zero: HymMirror = {
    var z = HymMirror()
    z.mix = 0
    z.pearls = z.pearls.map { $0.map { _ in (0, false) } }
    z.rings = z.rings.map { _ in HymRing(structure: 0, bright: 0, damp: 0, lvl: 0, poly: 0, energy: 0) }
    return z
  }()
  init(_ m: [Double]) { load(m, n: m.count) }

  // короткий или битый буфер — всё в нулях; нечисла зажимаются
  @discardableResult mutating func load(_ m: [Double], n: Int) -> Bool {
    guard n >= Hym.mirrorN, m.count >= n else { self = HymMirror.zero; return false }
    run = m[0] != 0
    bpm = Hym.clamp(m[1], 0, 1000)
    mix = Hym.clamp(m[2])
    for t in 0..<Hym.nr {
      cursor[t] = Int(Hym.clamp(m[3 + t].rounded(), 0, Double(Hym.pearlMax - 1)))
      len[t] = Int(Hym.clamp(m[5 + t].rounded(), 1, Double(Hym.pearlMax)))
      lo[t] = Int(Hym.clamp(m[7 + t].rounded(), 0, 127)); hi[t] = Int(Hym.clamp(m[9 + t].rounded(), 0, 127))
    }
    var o = Hym.head
    for t in 0..<Hym.nr { for i in 0..<Hym.pearlMax { pearls[t][i] = (Hym.clamp(m[o]), m[o + 1] != 0); o += 2 } }
    for r in 0..<Hym.nr {
      rings[r] = HymRing(structure: Hym.clamp(m[o]), bright: Hym.clamp(m[o + 1]), damp: Hym.clamp(m[o + 2]), lvl: Hym.clamp(m[o + 3]), poly: Hym.clamp(m[o + 4]), energy: Hym.clamp(m[o + 5], 0, 4))
      o += Hym.ringN
    }
    for k in 0..<Hym.nck { cloud[k] = Hym.clamp(m[o + k]) }
    cloudEnergy = Hym.clamp(m[o + Hym.nck], 0, 4)
    return true
  }

  // шаг нити t, который сыграл последним (курсор в ядре — следующая)
  func playedIndex(_ t: Int) -> Int { len[t] > 0 ? (cursor[t] + len[t] - 1) % len[t] : 0 }
}
