import XCTest
import T2DCore
import ObjectiveC
@testable import T2D

// Sidera: разбор зеркала, раскладка полос, номера действий, попадание в луну
final class SideraTests: XCTestCase {
  // 227 чисел по порядку world.hpp: голова 3, по голосу 8 + 16 × (d, a, hd, ha), потом drift / space / tone / cutoff / reso, потом oct голосов
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
    XCTAssertEqual(Sid.mirrorN, 227)
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
    XCTAssertEqual(SidAct.kick.rawValue, 6); XCTAssertEqual(SidAct.fx.rawValue, 7)
    XCTAssertEqual(Sid.Lfo.code(2, 1), 10); XCTAssertEqual(Sid.Lfo.code(0, 0), 1)
  }

  func testBandsStackedInsideField() {
    for (W, H, safeBot) in [(CGFloat(393), CGFloat(852), CGFloat(34)), (375, 667, 0), (320, 568, 0)] {
      let g = SidGeo(W: W, H: H, topY: 59, safeBot: safeBot), barTop = H - safeBot - TopBar.barH
      for v in 0..<Sid.nv {
        let r = g.bandRect(v)
        XCTAssertGreaterThan(r.height, 40, "\(W)x\(H) band \(v)")
        XCTAssertGreaterThanOrEqual(r.minY, g.moon.c.y + g.moon.r, "под луной \(W)")
        XCTAssertLessThanOrEqual(r.maxY, barTop, "над нижним баром \(W)x\(H)")
        XCTAssertGreaterThanOrEqual(r.minX, 0); XCTAssertLessThanOrEqual(r.maxX, W)
        if v > 0 { XCTAssertFalse(r.intersects(g.bandRect(v - 1)), "bands \(v) \(W)") }
      }
    }
  }

  func testOutlineEncloses() {                                    // контур группы: вокруг всех звёзд, а не прямоугольник; вырожденные случаи не падают
    XCTAssertTrue(sidOutline([], pad: 10).isEmpty)
    XCTAssertEqual(sidOutline([CGPoint(x: 5, y: 5)], pad: 10).count, 12)
    XCTAssertEqual(sidOutline([CGPoint(x: 0, y: 0), CGPoint(x: CGFloat.nan, y: 1)], pad: 10).count, 12)
    let tri = [CGPoint(x: 0, y: 0), CGPoint(x: 100, y: 0), CGPoint(x: 50, y: 80), CGPoint(x: 50, y: 20)]
    let o = sidOutline(tri, pad: 10)
    XCTAssertEqual(o.count, 3)                                    // внутренняя точка в оболочку не входит
    XCTAssertTrue(o.allSatisfy { p in tri.contains { hypot($0.x - p.x, $0.y - p.y) > 9.9 } })
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

  func testMoonHit() {
    let g = SidGeo(W: 390, H: 844, topY: 59, safeBot: 34)
    XCTAssertEqual(g.moon.c.x, 195, "луна по центру")
    XCTAssertTrue(g.moonHit(g.moon.c.x, g.moon.c.y))
    XCTAssertTrue(g.moonHit(g.moon.c.x + g.moonSpan, g.moon.c.y), "золотой круг на краю хода")
    let r = g.bandRect(1)
    XCTAssertFalse(g.moonHit(r.midX, r.midY))
    XCTAssertFalse(g.moonHit(g.moon.c.x, g.moon.c.y - g.moon.r - 20), "выше луны — нет")
    XCTAssertGreaterThan(g.moon.c.y - g.moon.r, 59, "под строкой заголовка")
  }

  func testRegistrationAndEnter() {
    XCTAssertEqual(Worlds.info(14)?.title, "Sidera")
    final class Rec: Core { var acts: [(Int, Int, [Double])] = []; func action(_ k: Int, _ i: Int, _ a: [Double], touchTime: Double) -> Bool { acts.append((k, i, a)); return true } }
    let rec = Rec(), v = SideraView(core: rec, readMirror: { b in for i in 0..<Sid.mirrorN { b[i] = 0.5 }; return Sid.mirrorN })
    v.enter()
    XCTAssertEqual(rec.acts.count, 1, "enter будит ядро: RUN 1")
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
    XCTAssertEqual(sidHeadRate(phase: 0.5), 1); XCTAssertEqual(sidHeadRate(phase: 1), 2); XCTAssertEqual(sidHeadRate(phase: 0), -2); XCTAssertEqual(sidHeadRate(phase: 0.75), 1.5, accuracy: 1e-9); XCTAssertEqual(sidHeadRate(phase: 0.25), -1.5, accuracy: 1e-9)
    XCTAssertEqual(sidHeadRate(phase: .nan), 1); XCTAssertEqual(sidHeadRate(phase: 9), 2); XCTAssertEqual(sidHeadRate(phase: -9), -2)
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
    let disc = CGRect(x: m.c.x - m.r, y: m.c.y - m.r, width: 2 * m.r, height: 2 * m.r)
    XCTAssertGreaterThan(lit(g, disc.insetBy(dx: -m.r * 2.5, dy: 0)), 100, "луна по центру")
    let (d2, g2) = canvas(390, 844)
    v.drawShot(d2, now: 1)
    XCTAssertLessThan(lit(g2, disc.insetBy(dx: m.r * 0.3, dy: m.r * 0.3)), 20, "в снимке для карты луны нет (остаются фоновые звёзды)")
    XCTAssertGreaterThan(lit(g2, v.geo.bandRect(1)), 150)
  }

  func testDrawSurvivesGarbageMirror() {
    let v = SideraView(core: Rec2(), readMirror: { _ in 0 })            // зеркало не пришло — нули
    v.layout(W: 320, H: 568, topY: 20, safeBot: 0); v.tick(now: 0, dt: 0.016)
    let (d, _) = canvas(320, 568)
    v.draw(d, now: 3); v.drawShot(d, now: 3)
  }
  // ---------- жесты ----------
  private func geoAndMirror() -> (SidGeo, SidMirror) { (SidGeo(W: 390, H: 844, topY: 59, safeBot: 34), SidMirror(vector())!) }

  func testPickStarWins() {
    let (g, m) = geoAndMirror()
    let p = g.starPoint(1, m.voices[1], 3)
    XCTAssertEqual(sidPick(g, m, CGPoint(x: p.x + 4, y: p.y - 3)), .star(voice: 1, pt: 3))
  }

  func testPickBandThenNone() {
    let (g, m) = geoAndMirror()
    var pt = CGPoint(x: g.bandRect(2).midX, y: g.bandRect(2).maxY - 3)
    for v in 0..<Sid.nv { for i in 0..<m.voices[v].n { let s = g.starPoint(v, m.voices[v], i); if hypot(s.x - pt.x, s.y - pt.y) < 30 { pt.x += 40 } } }
    let got = sidPick(g, m, pt)
    if case .box(let v) = got { XCTAssertEqual(v, 2) } else if case .star = got { /* звезда оказалась рядом — тоже верно */ } else { XCTFail("\(got)") }
    XCTAssertEqual(sidPick(g, m, CGPoint(x: 195, y: 3)), .none)
  }

  func testPickMoon() {
    let (g, m) = geoAndMirror()
    XCTAssertEqual(sidPick(g, m, CGPoint(x: g.moon.c.x + 12, y: g.moon.c.y + 4)), .moon)
  }

  func testHomeFromPointRoundTrip() {
    let (g, m) = geoAndMirror()
    for v in 0..<Sid.nv { for i in 1..<m.voices[v].n {
      let h = sidHomeFromPoint(g, voice: v, m.voices[v], pt: i, g.homePoint(v, m.voices[v], i))
      XCTAssertEqual(h.ha, m.voices[v].stars[i].ha, accuracy: 0.01, "ha v\(v) i\(i)")
      XCTAssertEqual(h.prev?.pt, i - 1); XCTAssertEqual(h.prev?.hd ?? -1, m.voices[v].stars[i - 1].hd, accuracy: 0.01, "hd v\(v) i\(i)")
    } }
    let h0 = sidHomeFromPoint(g, voice: 0, m.voices[0], pt: 0, g.homePoint(0, m.voices[0], 0))
    XCTAssertNil(h0.prev, "первая звезда стоит на левом краю")
  }

  func testHomeClamped() {
    let (g, m) = geoAndMirror()
    let far = sidHomeFromPoint(g, voice: 0, m.voices[0], pt: 2, CGPoint(x: 9999, y: -9999))
    XCTAssertEqual(far.ha, 1); XCTAssertTrue((Sid.dMin...1).contains(far.prev?.hd ?? 0))
    let low = sidHomeFromPoint(g, voice: 0, m.voices[0], pt: 2, CGPoint(x: -9999, y: 9999))
    XCTAssertEqual(low.ha, -1); XCTAssertTrue((Sid.dMin...1).contains(low.prev?.hd ?? 0))
    let nan = sidHomeFromPoint(g, voice: 0, m.voices[0], pt: 2, CGPoint(x: CGFloat.nan, y: CGFloat.nan))
    XCTAssertTrue(nan.ha.isFinite); XCTAssertNil(nan.prev, "не число — звезду не трогаем")
  }

  func testMoonPhase() {
    let (g, _) = geoAndMirror(), c = g.moon.c
    XCTAssertEqual(sidMoonPhase(g, c), 0.5, accuracy: 1e-9)
    XCTAssertEqual(sidMoonPhase(g, CGPoint(x: c.x + g.moonSpan, y: c.y)), 1, accuracy: 1e-9)
    XCTAssertEqual(sidMoonPhase(g, CGPoint(x: c.x - g.moonSpan, y: c.y)), 0, accuracy: 1e-9)
    XCTAssertEqual(sidMoonPhase(g, CGPoint(x: c.x + 900, y: c.y)), 1); XCTAssertEqual(sidMoonPhase(g, CGPoint(x: c.x - 900, y: 0)), 0)
    XCTAssertEqual(sidMoonPhase(g, CGPoint(x: CGFloat.nan, y: CGFloat.nan)), 0.5)
    XCTAssertEqual(sidMoonName(0.5), "eclipsis"); XCTAssertEqual(sidMoonName(1), "luna plena"); XCTAssertEqual(sidMoonName(0), "luna plena")
    XCTAssertEqual(sidMoonName(0.8), "ascendens"); XCTAssertEqual(sidMoonName(0.2), "descendens")
  }

  func testPinchWidensBox() {
    let (g, _) = geoAndMirror()
    let r = sidPinch(g, voice: 0, base: (wd: 0.25, wa: 0.3), d0: (dx: 40, dy: 40), d: (dx: 40 + g.inner(0).width / 2, dy: 40))
    XCTAssertEqual(r.wd, 0.75, accuracy: 1e-6); XCTAssertEqual(r.wa, 0.3, accuracy: 1e-6)
    let c = sidPinch(g, voice: 0, base: (0.25, 0.3), d0: (40, 40), d: (-9999, 9999))
    XCTAssertEqual(c.wd, 0); XCTAssertEqual(c.wa, 1)
  }

  private func rig() -> (SideraView, SidRec, [Finger]) {
    let rec = SidRec(), v = SideraView(core: rec, readMirror: { b in let m = self.vector(); for i in 0..<m.count { b[i] = m[i] }; return m.count })
    v.layout(W: 390, H: 844, topY: 59, safeBot: 34); v.tick(now: 1, dt: 0.016)
    let ids = [NSObject(), NSObject()]
    let fs = ids.map { Finger(id: ObjectIdentifier($0), x: 0, y: 0, x0: 0, y0: 0, t0: 0, moved: false, touchTime: 0) }
    objc_setAssociatedObject(v, "ids", ids, .OBJC_ASSOCIATION_RETAIN)
    return (v, rec, fs)
  }
  private func at(_ f: Finger, _ p: CGPoint, moved: Bool = true) -> Finger { var q = f; q.x = p.x; q.y = p.y; q.moved = moved; return q }

  func testGestureSendsActions() {
    let (v, rec, fs) = rig(); let g = v.geo, m = v.mirror
    // тянешь звезду → STAR (ha и hd предыдущей), подпись видна, по отпусканию уходит
    let s = g.starPoint(1, m.voices[1], 3)
    var f = at(fs[0], s, moved: false); v.down(f)
    f = at(f, CGPoint(x: s.x + 30, y: s.y - 25)); v.move(f)
    let stars = rec.acts.filter { $0.kind == SidAct.star.rawValue }
    XCTAssertEqual(stars.count, 2); XCTAssertEqual(stars[0].i, 1); XCTAssertEqual(stars[0].a[0], 3); XCTAssertEqual(stars[1].a[0], 2)
    for a in stars { XCTAssertTrue(a.a.allSatisfy { $0.isFinite }); XCTAssertTrue((Sid.dMin...1).contains(a.a[1])); XCTAssertTrue((-1.0...1).contains(a.a[2])) }
    XCTAssertTrue(v.labelOn[1]); v.up(f); XCTAssertFalse(v.labelOn[1])
    // луна: тянешь → MOON, тап → goMap
    rec.acts.removeAll()
    var mapped = 0; v.goMap = { mapped += 1 }
    let c = g.moon.c
    var mf = at(fs[0], CGPoint(x: c.x + 12, y: c.y + 4), moved: false); v.down(mf)
    mf = at(mf, CGPoint(x: c.x + 60, y: c.y + 4)); v.move(mf); v.up(mf)
    XCTAssertEqual(rec.acts.last?.kind, SidAct.moon.rawValue); XCTAssertEqual(mapped, 0)
    let tf = at(fs[0], CGPoint(x: c.x + 12, y: c.y + 4), moved: false); v.down(tf); v.up(tf)
    XCTAssertEqual(mapped, 1, "тап по луне — карта")
  }

  func testTapKicksAndFxSheetWrites() {
    let (v, rec, fs) = rig(); let g = v.geo, m = v.mirror
    let s = g.starPoint(2, m.voices[2], 4)
    let f = at(fs[0], s, moved: false); v.down(f); v.up(f)                              // тап по звезде → KICK одной звезды, вспышка
    XCTAssertEqual(rec.acts.last?.kind, SidAct.kick.rawValue); XCTAssertEqual(rec.acts.last?.i, 2); XCTAssertEqual(rec.acts.last?.a, [4])
    XCTAssertEqual(v.pulses.count, 1)
    let r = g.bandRect(0)
    let b = at(fs[0], CGPoint(x: r.midX, y: r.minY + 2), moved: false); v.down(b); v.up(b)   // пустое место полосы → встряска всего голоса
    if let last = rec.acts.last, last.kind == SidAct.kick.rawValue { XCTAssertEqual(last.a.count, 1) } else { XCTFail("kick") }
    rec.acts.removeAll()
    let spec = v.fx!
    XCTAssertEqual(spec.rows.map(\.key), ["drift", "space", "tone", "cutoff", "reso"]); XCTAssertEqual(spec.rows.map(\.code), [13, 14, 15, 16, 17])
    spec.set("space", 0.7)
    XCTAssertEqual(rec.acts.last?.kind, SidAct.fx.rawValue); XCTAssertEqual(rec.acts.last?.a, [1, 0.7])
  }

  func testOctaveRowAndMirror() {
    var vec = vector(); let o = Sid.head + Sid.nv * Sid.voiceN + Sid.fxN
    vec[o] = -2; vec[o + 1] = 1; vec[o + 2] = 99
    let m = SidMirror(vec)!
    XCTAssertEqual(m.voices.map { $0.oct }, [-2, 1, 2]); XCTAssertEqual(m.fx.count, 5)
    let (v, rec, _) = rig()
    let spec = v.voiceSpec(1)
    XCTAssertTrue(spec.rows.contains { $0.key == "oct" })
    spec.set("oct", 1)                                                                // край шкалы → +2
    XCTAssertEqual(rec.acts.last?.kind, SidAct.voice.rawValue); XCTAssertEqual(rec.acts.last?.a, [Double(Sid.Key.oct), 2])
    spec.set("oct", 0.5); XCTAssertEqual(rec.acts.last?.a, [Double(Sid.Key.oct), 0])
  }

  func testTwoFingersWidenAndHoldOpensSheet() {
    let (v, rec, fs) = rig(); let g = v.geo
    let r = g.bandRect(0)
    var a = at(fs[0], CGPoint(x: r.minX + 80, y: r.midY), moved: false), b = at(fs[1], CGPoint(x: r.minX + 120, y: r.midY), moved: false)
    v.down(a); v.down(b)
    b = at(b, CGPoint(x: r.minX + 220, y: r.midY - 30)); v.move(b)
    let w = rec.acts.filter { $0.kind == SidAct.voice.rawValue }
    XCTAssertEqual(Set(w.map { Int($0.a[0]) }), [Sid.Key.wd, Sid.Key.wa]); XCTAssertTrue(w.allSatisfy { $0.i == 0 && (0.0...1).contains($0.a[1]) })
    v.up(a); v.up(b)
    // удержание на полосе → плашка
    var opened: SheetSpec?; v.openSheet = { opened = $0 }
    a = at(fs[0], CGPoint(x: r.minX + 30, y: r.maxY - 5), moved: false); v.down(a)
    v.tick(now: 1.2, dt: 0.016); XCTAssertNil(opened)
    v.tick(now: 1.2 + Touch.holdMs / 1000 + 0.05, dt: 0.016); XCTAssertNotNil(opened)
    v.up(a)
  }

  func testVoiceSheet() {
    let (v, rec, _) = rig()
    let spec = v.voiceSpec(2)
    XCTAssertEqual(spec.rows.map { $0.key }, ["note", "oct", "n", "lvl", "pan", "wd", "wa", "dist"])
    XCTAssertEqual(spec.rows.first { $0.key == "lvl" }?.code, Sid.Lfo.code(2, 0)); XCTAssertEqual(spec.rows.first { $0.key == "wa" }?.code, Sid.Lfo.code(2, 3))
    XCTAssertEqual(spec.text("note"), sidNoteName(14)); XCTAssertEqual(spec.text("n"), "11 stellae")
    for r in spec.rows { XCTAssertTrue((0.0...1).contains(spec.get(r.key)), r.key) }
    spec.set("n", 1); spec.set("pan", 0); spec.set("note", 0.5); spec.set("dist", 1)
    let vs = rec.acts.filter { $0.kind == SidAct.voice.rawValue }
    XCTAssertEqual(vs.map { $0.i }, [2, 2, 2, 2])
    XCTAssertEqual(vs[0].a, [Double(Sid.Key.n), 16]); XCTAssertEqual(vs[1].a[0], Double(Sid.Key.pan)); XCTAssertEqual(vs[1].a[1], -1)
    XCTAssertEqual(vs[2].a, [Double(Sid.Key.note), 30]); XCTAssertEqual(vs[3].a, [Double(Sid.Key.dist), 2])
    rec.acts.removeAll(); spec.tap?("dist")
    XCTAssertEqual(rec.acts.last?.a, [Double(Sid.Key.dist), 0], "тап по строке закона шага — следующий")
    XCTAssertNotNil(spec.lfo); XCTAssertEqual(spec.lfo?.keys, ["lvl", "pan", "wd", "wa"])
  }

  func testLegendHasNoRussianAndCoversGestures() {
    let v = SideraView(core: Rec2())
    XCTAssertGreaterThanOrEqual(v.legend.count, 5)
    for r in v.legend { XCTAssertNil((r.gesture + r.does).range(of: "[А-Яа-я]", options: .regularExpression)) }
  }

  // CLEAR и отмена в живом ядре: снимок до очистки возвращает голоса (как делает хост: snap → CLEAR → restore)
  func testClearThenUndoRestoresState() throws {
    t2d_app_init(48000, 1, 1e9, 1 << 14)
    let core = AppCore(slot: SideraView.slotN), host = AppHostCore()
    var l = [Float](repeating: 0, count: 256), r = l, buf = [Double](repeating: 0, count: 256)
    func render() { for _ in 0..<4 { t2d_app_render(&l, &r, 256, 0) } }
    func mirror() -> SidMirror { render(); let n = Int(t2d_app_mirror(Int32(SideraView.slotN), &buf, 256)); return SidMirror(Array(buf[0..<n])) ?? .zero }
    core.action(SidAct.run.rawValue, 0, [1], touchTime: 0)
    core.action(SidAct.voice.rawValue, 1, [Double(Sid.Key.lvl), 0.8], touchTime: 0)
    core.action(SidAct.voice.rawValue, 1, [Double(Sid.Key.note), 17], touchTime: 0)
    core.action(SidAct.fx.rawValue, 0, [Double(Sid.Fx.drift), 0], touchTime: 0)       // дрейф рамок выключен: иначе центры уходят, пока идёт рендер
    let before = mirror()
    XCTAssertEqual(before.voices[1].lvl, 0.8, accuracy: 1e-9)
    XCTAssertTrue(host.snap(slot: SideraView.slotN, id: 1))
    var snap: (id: Int, slot: Int, w: String)?
    for _ in 0..<20 where snap == nil { render(); snap = host.takeSnap() }
    let s = try XCTUnwrap(snap)
    core.action(SidAct.clear.rawValue, 0, [], touchTime: 0)
    XCTAssertTrue(mirror().voices.allSatisfy { $0.lvl == 0 }, "после CLEAR тишина")
    XCTAssertTrue(host.restore(slot: SideraView.slotN, s.w))
    let after = mirror()
    XCTAssertEqual(after.voices.map { $0.lvl }, before.voices.map { $0.lvl })
    XCTAssertEqual(after.voices.map { $0.note }, before.voices.map { $0.note })
    XCTAssertEqual(after.voices.map { $0.n }, before.voices.map { $0.n })
    XCTAssertEqual(after.voices[1].stars.map { $0.hd }, before.voices[1].stars.map { $0.hd }, "центры рамок не блуждают")
  }

  // ревью: reinit ядра сбрасывает run — каждый вход в мир должен снова слать RUN 1 (в том числе после restored)
  func testEnterAlwaysSendsRun() {
    let rec = SidRec(), v = SideraView(core: rec)
    v.enter(); v.enter(); v.restored(); v.enter()
    XCTAssertEqual(rec.acts.filter { $0.kind == SidAct.run.rawValue && $0.a == [1] }.count, 3)
  }

}

private final class Rec2: Core { func action(_ k: Int, _ i: Int, _ a: [Double], touchTime: Double) -> Bool { true } }
private final class SidRec: Core {
  var acts: [(kind: Int, i: Int, a: [Double])] = []
  func action(_ k: Int, _ i: Int, _ a: [Double], touchTime: Double) -> Bool { acts.append((k, i, a)); return true }
}
