import Foundation

// зеркало Hymenaeus (MIRROR_N = 61) — core/worlds/hymenaeus/world.hpp:
// run, bpm, dejavu, len, cursor; 16 × (pitch, rest); 3 × (struct, bright, damp, lvl, energy); 8 значений облаков; энергия облаков
enum Hym {
  static let nr = 3, pearlMax = 16, nck = 8, head = 5, ringN = 5
  static let mirrorN = head + 2 * pearlMax + nr * ringN + nck + 1   // 61
  static let ink: RGB = (44, 38, 30), gold: RGB = (168, 124, 48), ivory: RGB = (242, 237, 226)
  static let roman = ["I", "II", "III"]
  enum Cloud { static let pos = 0, size = 1, dens = 2, pitch = 3, freeze = 4, echo = 5, level = 6, send = 7 }   // CloudKey ядра
  enum Ring { static let structure = 0, bright = 1, damp = 2, lvl = 3 }                                           // RingKey ядра
  static func clamp(_ v: Double, _ lo: Double = 0, _ hi: Double = 1) -> Double { v.isFinite ? min(hi, max(lo, v)) : lo }
}

// действия ядра — те же номера, что enum Act в world.hpp (сверяет HymenaeusTests)
enum HymAct: Int { case ring, cloud, pearl, strike, thread, dice, clear, run, bpm }

struct HymRing { var structure = 0.5, bright = 0.5, damp = 0.5, lvl = 0.8, energy = 0.0 }

struct HymMirror {
  var run = false
  var bpm = 0.0, dejavu = 0.0
  var len = 0, cursor = 0
  var pearls = [(pitch: Double, rest: Bool)](repeating: (0.5, false), count: Hym.pearlMax)
  var rings = [HymRing](repeating: HymRing(), count: Hym.nr)
  var cloud = [Double](repeating: 0, count: Hym.nck)
  var cloudEnergy = 0.0

  init() {}
  static let zero: HymMirror = {
    var z = HymMirror()
    z.pearls = z.pearls.map { _ in (0, false) }
    z.rings = z.rings.map { _ in HymRing(structure: 0, bright: 0, damp: 0, lvl: 0, energy: 0) }
    return z
  }()
  init(_ m: [Double]) { load(m, n: m.count) }

  // короткий или битый буфер — всё в нулях; нечисла зажимаются
  @discardableResult mutating func load(_ m: [Double], n: Int) -> Bool {
    guard n >= Hym.mirrorN, m.count >= n else { self = HymMirror.zero; return false }
    run = m[0] != 0
    bpm = Hym.clamp(m[1], 0, 1000); dejavu = Hym.clamp(m[2])
    len = Int(Hym.clamp(m[3].rounded(), 1, Double(Hym.pearlMax)))
    cursor = Int(Hym.clamp(m[4].rounded(), 0, Double(Hym.pearlMax - 1)))
    var o = Hym.head
    for i in 0..<Hym.pearlMax { pearls[i] = (Hym.clamp(m[o]), m[o + 1] != 0); o += 2 }
    for r in 0..<Hym.nr {
      rings[r] = HymRing(structure: Hym.clamp(m[o]), bright: Hym.clamp(m[o + 1]), damp: Hym.clamp(m[o + 2]), lvl: Hym.clamp(m[o + 3]), energy: Hym.clamp(m[o + 4], 0, 4))
      o += Hym.ringN
    }
    for k in 0..<Hym.nck { cloud[k] = Hym.clamp(m[o + k]) }
    cloudEnergy = Hym.clamp(m[o + Hym.nck], 0, 4)
    return true
  }

  // жемчужина, которая сыграла последней (курсор в ядре — следующая)
  var playedIndex: Int { len > 0 ? (cursor + len - 1) % len : 0 }
  // кольцо по высоте — те же полосы ⅓ / ⅔, что в model.cpp ringFor
  static func ringFor(_ pitch: Double) -> Int { pitch < 1.0 / 3 ? 0 : pitch < 2.0 / 3 ? 1 : 2 }
}
