import XCTest
import T2DCore
@testable import T2D

// Sidera: разбор зеркала, раскладка полос, номера действий, попадание в луну
final class SideraTests: XCTestCase {
  // 219 чисел по порядку world.hpp: голова 3, по голосу 8 + 16 × (d, a, hd, ha)
  private func vector() -> [Double] {
    var m = [Double](repeating: 0, count: Sid.mirrorN)
    m[0] = 0.4; m[1] = 0.6; m[2] = 0.3
    for v in 0..<Sid.nv {
      let o = Sid.head + v * Sid.voiceN
      m[o] = Double(7 * v); m[o + 1] = Double(5 + 3 * v); m[o + 2] = 0.25; m[o + 3] = 0.3; m[o + 4] = 0.5 - 0.1 * Double(v)
      m[o + 5] = Double(v - 1) / 2; m[o + 6] = Double(v); m[o + 7] = 0.2 + 0.1 * Double(v)
      for i in 0..<Sid.nptMax {
        let p = o + 8 + 4 * i
        m[p] = 0.5 + 0.01 * Double(i); m[p + 1] = 0.05 * Double(i) - 0.4; m[p + 2] = 0.5; m[p + 3] = 0.05 * Double(i) - 0.4
      }
    }
    return m
  }

  func testMirrorDecode() {
    XCTAssertEqual(Sid.mirrorN, 219)
    let m = SidMirror(vector())!
    XCTAssertEqual(m.level, 0.4, accuracy: 1e-9); XCTAssertEqual(m.step, 0.6, accuracy: 1e-9); XCTAssertEqual(m.send, 0.3, accuracy: 1e-9)
    XCTAssertEqual(m.voices.map { $0.n }, [5, 8, 11])
    XCTAssertEqual(m.voices[1].note, 7); XCTAssertEqual(m.voices[2].note, 14)
    XCTAssertEqual(m.voices[0].pan, -0.5, accuracy: 1e-9); XCTAssertEqual(m.voices[2].pan, 0.5, accuracy: 1e-9)
    XCTAssertEqual(m.voices.map { $0.dist }, [0, 1, 2])
    XCTAssertEqual(m.voices[2].energy, 0.4, accuracy: 1e-9)
    XCTAssertEqual(m.voices[1].stars.count, 16)
    XCTAssertEqual(m.voices[1].stars[3].d, 0.53, accuracy: 1e-9); XCTAssertEqual(m.voices[1].stars[3].a, -0.25, accuracy: 1e-9)
    XCTAssertEqual(m.voices[1].stars[3].hd, 0.5, accuracy: 1e-9)
  }

  func testMirrorRejectsWrongLength() {
    XCTAssertNil(SidMirror(Array(vector().prefix(Sid.mirrorN - 1))))
    XCTAssertNil(SidMirror(vector() + [0]))
    var m = SidMirror(); XCTAssertFalse(m.load(Array(vector().prefix(30)), n: 30))      // короткий буфер — нули, не падает
    XCTAssertEqual(m.voices[0].lvl, 0); XCTAssertEqual(m.step, 0)
    var bad = vector(); bad[1] = .nan; bad[3 + 1] = .infinity; bad[3 + 8] = .nan; bad[3 + 8 + 1] = 99
    let b = SidMirror(bad)!
    XCTAssertEqual(b.step, 0); XCTAssertEqual(b.voices[0].n, Sid.nptMin == 3 ? 3 : 0)
    XCTAssertEqual(b.voices[0].stars[0].d, Sid.dMin); XCTAssertEqual(b.voices[0].stars[0].a, 1)
  }

  func testActNumbers() {
    XCTAssertEqual(SidAct.voice.rawValue, 0); XCTAssertEqual(SidAct.star.rawValue, 1); XCTAssertEqual(SidAct.dice.rawValue, 2)
    XCTAssertEqual(SidAct.clear.rawValue, 3); XCTAssertEqual(SidAct.moon.rawValue, 4); XCTAssertEqual(SidAct.run.rawValue, 5)
    XCTAssertEqual(Sid.Lfo.code(2, 1), 10); XCTAssertEqual(Sid.Lfo.code(0, 0), 1)
  }

  func testBandsStackedInsideField() {
    for (W, H, safeBot) in [(CGFloat(393), CGFloat(852), CGFloat(34)), (375, 667, 0), (320, 568, 0)] {
      let g = SidGeo(W: W, H: H, topY: 59, safeBot: safeBot), barTop = H - safeBot - TopBar.barH
      for v in 0..<Sid.nv {
        let r = g.bandRect(v)
        XCTAssertGreaterThan(r.height, 40, "\(W)x\(H) band \(v)")
        XCTAssertGreaterThanOrEqual(r.minY, g.topY + 50, "под луной \(W)")
        XCTAssertLessThanOrEqual(r.maxY, barTop, "над нижним баром \(W)x\(H)")
        XCTAssertGreaterThanOrEqual(r.minX, 0); XCTAssertLessThanOrEqual(r.maxX, W)
        if v > 0 { XCTAssertFalse(r.intersects(g.bandRect(v - 1)), "bands \(v) \(W)") }
      }
    }
  }

  func testStarX() {
    let m = SidMirror(vector())!, g = SidGeo(W: 390, H: 844, topY: 59, safeBot: 34)
    for v in 0..<Sid.nv {
      let voice = m.voices[v]
      var prev = -CGFloat.infinity
      for i in 0..<voice.n {
        let p = g.starPoint(v, voice, i)
        XCTAssertGreaterThan(p.x, prev, "x монотонен v\(v) i\(i)"); prev = p.x
        XCTAssertTrue(g.bandRect(v).contains(p), "звезда внутри полосы v\(v) i\(i)")
      }
      XCTAssertEqual(voice.frac(0), 0)
      XCTAssertLessThan(voice.frac(voice.n - 1), 1)
    }
    var degenerate = SidVoice(); degenerate.n = 4
    for i in 0..<4 { degenerate.stars[i].d = 0; degenerate.stars[i].hd = 0 }
    XCTAssertEqual(degenerate.frac(2), 0.5, accuracy: 1e-9, "сумма 0 — поровну, без деления на ноль")
  }

  func testBoxAroundHome() {
    let m = SidMirror(vector())!, g = SidGeo(W: 390, H: 844, topY: 59, safeBot: 34)
    let r = g.boxRect(0, m.voices[0], 2), c = g.homePoint(0, m.voices[0], 2)
    XCTAssertEqual(r.midX, c.x, accuracy: 1e-6); XCTAssertEqual(r.midY, c.y, accuracy: 1e-6)
    XCTAssertGreaterThan(r.width, 0); XCTAssertGreaterThan(r.height, 0)
  }

  func testBandAt() {
    let g = SidGeo(W: 390, H: 844, topY: 59, safeBot: 34)
    for v in 0..<Sid.nv { let r = g.bandRect(v); XCTAssertEqual(g.bandAt(CGPoint(x: r.midX, y: r.midY)), v) }
    XCTAssertNil(g.bandAt(CGPoint(x: 195, y: 5)))
  }

  func testCornerHitOnMoon() {
    let g = SidGeo(W: 390, H: 844, topY: 59, safeBot: 34)
    XCTAssertTrue(g.cornerHit(g.moon.c.x + 10, g.moon.c.y + 10))
    let r = g.bandRect(1)
    XCTAssertFalse(g.cornerHit(r.midX, r.midY))
    XCTAssertFalse(g.cornerHit(g.moon.c.x + 10, g.moon.c.y - 20), "выше верхнего края луны — нет")
  }

  func testRegistrationAndEnter() {
    XCTAssertEqual(Worlds.info(14)?.title, "Sidera")
    final class Rec: Core { var acts: [(Int, Int, [Double])] = []; func action(_ k: Int, _ i: Int, _ a: [Double], touchTime: Double) -> Bool { acts.append((k, i, a)); return true } }
    let rec = Rec(), v = SideraView(core: rec, readMirror: { b in for i in 0..<Sid.mirrorN { b[i] = 0.5 }; return Sid.mirrorN })
    v.enter(); v.enter()
    XCTAssertEqual(rec.acts.count, 1, "enter будит ядро один раз")
    XCTAssertEqual(rec.acts[0].0, SidAct.run.rawValue); XCTAssertEqual(rec.acts[0].2, [1])
    v.tick(now: 1, dt: 0.016)
    XCTAssertEqual(v.mirror.step, 0.5, accuracy: 1e-9)
    v.dice(); v.clear()
    XCTAssertEqual(rec.acts.map { $0.0 }.suffix(2), [SidAct.dice.rawValue, SidAct.clear.rawValue])
    XCTAssertFalse(v.cornerHit(5, 61), "cornerHit — через goMap самой луной")
  }
}
