import CoreGraphics
import Foundation

// жесты Paradisus как чистые функции (порт down/move/up из worlds/paradisus/view.js): что под пальцем, как тянуть, двойной тап.
// Вид только вызывает их и шлёт действия ядру.
// Закреплённый ангел: касание (TOUCH) принимает только id >= 0, поэтому его тянут лишь вдоль — гласная (VOWEL); поперёк — через ядро нельзя.
enum ParHit: Equatable {
  case angel(Int)       // ангел, который ещё не закреплён (поёт, пока его держат), по id из зеркала
  case sphere(Int)
  case rose
  case pinned(Int)      // закреплённый ангел, по id из зеркала
  case none
}

enum ParGesture: Equatable {
  case touch(k: Int, v: Double)   // поперёк: на ближайшую сферу (гармонику), гласная — по её эллипсу
  case vowel(Double)              // вдоль: только гласная
}

// прошлый тап: двойной тап — ещё раз в тот же ангел (id) или рядом с первым тапом в пустое место (id = nil)
struct ParTap: Equatable {
  var id: Int?
  var t: Double
  var p: CGPoint
}

enum ParTouch {
  static let dragStart: CGFloat = 6, dtapNear: CGFloat = 34   // view.js: сдвиг меньше 6 — не ведёшь; два тапа ближе 34 — двойной

  static func ok(_ p: CGPoint) -> Bool { p.x.isFinite && p.y.isFinite }

  // роза → закреплённый ангел → прочий ангел → сфера; ангелы — по зоне ParGeo.angelRect, ближайший к пальцу
  static func hit(_ p: CGPoint, geo: ParGeo, m: [Double]) -> ParHit {
    guard ok(p) else { return .none }
    if geo.inRose(p) { return .rose }
    let angels = ParMirror(m).angels
    func nearest(_ pinned: Bool) -> ParAngel? {
      var best: ParAngel?, bd = CGFloat.infinity
      for a in angels where a.pinned == pinned && geo.angelRect(a).contains(p) {
        let c = geo.angelPos(a.k, a.v), d = hypot(p.x - c.x, p.y - c.y)
        if d < bd { bd = d; best = a }
      }
      return best
    }
    if let a = nearest(true) { return .pinned(a.id) }
    if let a = nearest(false) { return .angel(a.id) }
    if let k = geo.sphereAt(p) { return .sphere(k) }
    return .none
  }

  // жест протяжки: палец p, ангел сейчас на сфере k; nil — точка не число
  static func drag(to p: CGPoint, k: Int, geo: ParGeo) -> ParGesture? {
    guard ok(p) else { return nil }
    let k1 = geo.sphereAt(p) ?? k
    let v = geo.vowel(k1, p)
    return k1 == k ? .vowel(v) : .touch(k: k1, v: v)
  }

  // закреплённого тянут только вдоль его сферы
  static func dragPinned(to p: CGPoint, k: Int, geo: ParGeo) -> ParGesture? {
    guard ok(p) else { return nil }
    return .vowel(geo.vowel(k, p))
  }

  static func moved(from p0: CGPoint, to p: CGPoint) -> Bool { ok(p) && hypot(p.x - p0.x, p.y - p0.y) >= dragStart }

  // двойной тап: за dtapMs; по ангелу — тот же id, в пустое место — первый тап был без ангела и рядом
  static func isDouble(last: ParTap?, id: Int?, at p: CGPoint, now: Double, dtapMs: Double = Touch.dtapMs) -> Bool {
    guard let l = last, ok(p), now - l.t < dtapMs / 1000 else { return false }
    if let id { return l.id == id }
    return l.id == nil && hypot(p.x - l.p.x, p.y - l.p.y) < dtapNear
  }

  // тихая развилка пятого закрепления: ядро отказало (ангел так и остался временным) — его надо отпустить
  static func stuckTemp(_ m: ParMirror, id: Int) -> Bool { m.angels.contains { $0.id == id && !$0.pinned } }
}
