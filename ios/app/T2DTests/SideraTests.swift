import XCTest
import T2DCore
@testable import T2D

// Sidera: разбор зеркала, раскладка полос, номера действий, попадание в луну
final class SideraTests: XCTestCase {
  // 219 чисел по порядку world.hpp: голова 3, по голосу 8 + 16 × (d, a, hd, ha)
  fileprivate func vector() -> [Double] {
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

  // ---------- окно линии и рисование ----------
  func testWindowCount() {
    let w = SidWindow.segments(head: 4.3, n: 8, count: 3)
    XCTAssertEqual(w.map { $0.from }, [2, 3, 4]); XCTAssertEqual(w.map { $0.to }, [3, 4, 5])
    XCTAssertTrue(zip(w, w.dropFirst()).allSatisfy { $0.alpha < $1.alpha }, "alpha растёт к голове")
    XCTAssertEqual(w.last?.alpha, 1)
  }

  func testWindowWraps() {
    let w = SidWindow.segments(head: 7.5, n: 8, count: 2)
    XCTAssertEqual(w.map { $0.from }, [6, 7]); XCTAssertEqual(w.map { $0.to }, [7, 0])
    let w2 = SidWindow.segments(head: 0.2, n: 8, count: 3)
    XCTAssertEqual(w2.map { $0.from }, [6, 7, 0]); XCTAssertEqual(w2.map { $0.to }, [7, 0, 1])
  }

  func testWindowSmallN() {
    let w = SidWindow.segments(head: 1.1, n: 3, count: 3)
    XCTAssertEqual(Set(w.map { $0.from }).count, 3, "три отрезка без повторов")
    let w2 = SidWindow.segments(head: 1.1, n: 3, count: 5)
    XCTAssertLessThanOrEqual(w2.count, 3, "отрезков не больше, чем звёзд")
    for s in w + w2 { XCTAssertTrue((0..<3).contains(s.from) && (0..<3).contains(s.to)) }
    for bad in [Double.nan, -3.5, .infinity, 1e12] {
      for s in SidWindow.segments(head: bad, n: 5, count: 3) { XCTAssertTrue((0..<5).contains(s.from) && (0..<5).contains(s.to), "head \(bad)") }
    }
    XCTAssertTrue(SidWindow.segments(head: 0, n: 0, count: 3).isEmpty)
  }

  func testHeadRateRange() {
    XCTAssertEqual(sidHeadRate(step: 0), 1); XCTAssertEqual(sidHeadRate(step: 1), 2); XCTAssertEqual(sidHeadRate(step: 0.5), 1.5, accuracy: 1e-9)
    XCTAssertEqual(sidHeadRate(step: .nan), 1); XCTAssertEqual(sidHeadRate(step: 9), 2); XCTAssertEqual(sidHeadRate(step: -9), 1)
  }

  func testNoteName() {
    XCTAssertEqual(sidNoteName(0), "A1"); XCTAssertEqual(sidNoteName(12), "A2"); XCTAssertEqual(sidNoteName(3), "C2"); XCTAssertEqual(sidNoteName(7), "E2")
    XCTAssertEqual(sidNoteName(.nan), "A1")
  }

  private func canvas(_ w: Int, _ h: Int) -> (Draw, CGContext) {
    let g = CGContext(data: nil, width: w, height: h, bitsPerComponent: 8, bytesPerRow: 0, space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue)!
    g.translateBy(x: 0, y: CGFloat(h)); g.scaleBy(x: 1, y: -1)
    g.setFillColor(UIColor.black.cgColor); g.fill(CGRect(x: 0, y: 0, width: w, height: h))
    let d = Draw(g: g, W: CGFloat(w), H: CGFloat(h)); d.setTheme(.base); return (d, g)
  }
  private func lit(_ g: CGContext, _ r: CGRect) -> Int {          // сколько пикселей прямоугольника светлее фона
    guard let data = g.data?.assumingMemoryBound(to: UInt8.self) else { return 0 }
    let bpr = g.bytesPerRow, h = g.height
    var n = 0
    for y in Int(r.minY)..<Int(r.maxY) { for x in Int(r.minX)..<Int(r.maxX) { let o = y * bpr + x * 4; if data[o + 1] > 20 { n += 1 } } }
    return n
  }

  func testDrawBandsAndMoon() {
    let v = SideraView(core: Rec2(), readMirror: { b in let m = self.vector(); for i in 0..<m.count { b[i] = m[i] }; return m.count })
    v.layout(W: 390, H: 844, topY: 59, safeBot: 34); v.tick(now: 1, dt: 0.016)
    let (d, g) = canvas(390, 844)
    v.draw(d, now: 1)
    for k in 0..<Sid.nv { XCTAssertGreaterThan(lit(g, v.geo.bandRect(k)), 150, "полоса \(k) нарисована") }
    let m = v.geo.moon
    let corner = CGRect(x: 0, y: m.c.y, width: m.r, height: m.r)
    XCTAssertGreaterThan(lit(g, corner), 100, "луна в углу")
    let (d2, g2) = canvas(390, 844)
    v.drawShot(d2, now: 1)
    XCTAssertLessThan(lit(g2, CGRect(x: 0, y: m.c.y + 4, width: m.r * 0.8, height: m.r * 0.8)), 20, "в снимке для карты луны нет (остаются фоновые звёзды)")
    XCTAssertGreaterThan(lit(g2, v.geo.bandRect(1)), 150)
  }

  func testDrawSurvivesGarbageMirror() {
    let v = SideraView(core: Rec2(), readMirror: { _ in 0 })            // зеркало не пришло — нули
    v.layout(W: 320, H: 568, topY: 20, safeBot: 0); v.tick(now: 0, dt: 0.016)
    let (d, _) = canvas(320, 568)
    v.draw(d, now: 3); v.drawShot(d, now: 3)
  }
}

private final class Rec2: Core { func action(_ k: Int, _ i: Int, _ a: [Double], touchTime: Double) -> Bool { true } }
