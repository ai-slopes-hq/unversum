import Foundation

// зеркало Sidera (MIRROR_N = 227) — core/worlds/sidera/world.hpp:
// [0] энергия выхода [1] step (луна) [2] send; по голосу v с 3 + 72·v: NOTE N WD WA LVL PAN DIST энергия (8), затем 16 × (d, a, hd, ha); с 219: drift, space, tone, cutoff, reso; с 224: oct голосов
enum Sid {
  static let nv = 3, nptMin = 3, nptMax = 16
  static let head = 3, voiceN = 8 + nptMax * 4, fxN = 5, mirrorN = head + nv * voiceN + fxN + nv   // 72, 227
  static let dMin = 0.02
  // VKey ядра (ключи VOICE и значения зеркала)
  enum Key { static let note = 0, n = 1, wd = 2, wa = 3, lvl = 4, pan = 5, dist = 6, vstep = 7, oct = 8 }
  enum Dist { static let cauchy = 0, logistic = 1, uniform = 2 }
  static let distNames = ["cauchy", "logistic", "uniform"]
  // ACT_LFO: 0 — шаг луны; 1 + 4·голос + {0 lvl, 1 pan, 2 wd, 3 wa}
  enum Lfo { static let step = 0, perVoice = 4
    static func code(_ voice: Int, _ k: Int) -> Int { 1 + perVoice * voice + k } }
  // FX мира: 0 drift (вторичное блуждание рамок), 1 space (посыл в ревербер), 2 tone (яркость), 3 cutoff и 4 reso (мастер-фильтр); LFO-ключи 13 + k
  enum Fx { static let drift = 0, space = 1, tone = 2, cutoff = 3, reso = 4, lfoBase = 13 }
  static func clamp(_ v: Double, _ lo: Double = 0, _ hi: Double = 1) -> Double { v.isFinite ? min(hi, max(lo, v)) : lo }
}

// действия ядра — те же номера, что enum Act в world.hpp (сверяет SideraTests)
enum SidAct: Int { case voice, star, dice, clear, moon, run, kick, fx }

struct SidStar { var d = 0.5, a = 0.0, hd = 0.5, ha = 0.0 }

struct SidVoice {
  var oct = 0, note = 0.0, n = 5, wd = 0.25, wa = 0.3, lvl = 0.5, pan = 0.0, dist = 0, energy = 0.0
  var stars = [SidStar](repeating: SidStar(), count: Sid.nptMax)

  // доля 0…1 позиции звезды i вдоль периода: накопленные длительности до неё / сумма; home — по центрам рамок (hd)
  func frac(_ i: Int, home: Bool = false) -> Double {
    let cnt = min(max(n, 1), Sid.nptMax)
    var sum = 0.0, pre = 0.0
    for j in 0..<cnt { let x = home ? stars[j].hd : stars[j].d; sum += x; if j < i { pre += x } }
    return sum > 0 ? pre / sum : Double(max(0, i)) / Double(cnt)
  }
}

struct SidMirror {
  var level = 0.0, step = 0.15, send = 0.3
  var fx = [0.25, 0.3, 0.35, 1.0, 0.1]                    // drift, space, tone, cutoff, reso
  var voices = [SidVoice](repeating: SidVoice(), count: Sid.nv)

  init() {}
  static let zero: SidMirror = {
    var z = SidMirror(); z.step = 0; z.send = 0; z.fx = [0, 0, 0, 1, 0]
    z.voices = z.voices.map { _ in var v = SidVoice(); v.n = Sid.nptMin; v.wd = 0; v.wa = 0; v.lvl = 0; v.stars = v.stars.map { _ in SidStar(d: 0.5, a: 0, hd: 0.5, ha: 0) }; return v }
    return z
  }()
  init?(_ m: [Double]) {
    guard m.count == Sid.mirrorN else { return nil }
    load(m, n: m.count)
  }

  // короткий или битый буфер — всё в нулях; нечисла зажимаются
  @discardableResult mutating func load(_ m: [Double], n: Int) -> Bool {
    guard n >= Sid.mirrorN, m.count >= n else { self = SidMirror.zero; return false }
    level = Sid.clamp(m[0]); step = Sid.clamp(m[1]); send = Sid.clamp(m[2])
    for k in 0..<Sid.fxN { fx[k] = Sid.clamp(m[Sid.head + Sid.nv * Sid.voiceN + k]) }
    for v in 0..<Sid.nv { voices[v].oct = Int(Sid.clamp(m[Sid.head + Sid.nv * Sid.voiceN + Sid.fxN + v].rounded(), -2, 2)) }
    for v in 0..<Sid.nv {
      var o = Sid.head + v * Sid.voiceN
      voices[v].note = Sid.clamp(m[o].rounded(), 0, 60)
      voices[v].n = Int(Sid.clamp(m[o + 1].rounded(), Double(Sid.nptMin), Double(Sid.nptMax)))
      voices[v].wd = Sid.clamp(m[o + 2]); voices[v].wa = Sid.clamp(m[o + 3]); voices[v].lvl = Sid.clamp(m[o + 4])
      voices[v].pan = Sid.clamp(m[o + 5], -1, 1)
      voices[v].dist = Int(Sid.clamp(m[o + 6].rounded(), 0, 2))
      voices[v].energy = Sid.clamp(m[o + 7])
      o += 8
      for i in 0..<Sid.nptMax {
        voices[v].stars[i] = SidStar(d: Sid.clamp(m[o], Sid.dMin, 1), a: Sid.clamp(m[o + 1], -1, 1), hd: Sid.clamp(m[o + 2], Sid.dMin, 1), ha: Sid.clamp(m[o + 3], -1, 1))
        o += 4
      }
    }
    return true
  }
}
