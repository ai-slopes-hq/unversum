import XCTest
import T2DCore
@testable import T2D
private final class HymRec: Core {
  var acts: [(kind: Int, i: Int, a: [Double])] = []
  func action(_ kind: Int, _ i: Int, _ a: [Double], touchTime: Double) -> Bool { acts.append((kind, i, a)); return true }
}

// Hymenaeus: разбор зеркала, раскладка зон, номера действий, тап-удар
final class HymenaeusTests: XCTestCase {
  // 97 число по порядку world.hpp: голова 12, жемчужины 2×32, кольца 2×6, облака 8, энергия
  private func vector() -> [Double] {
    var m = [Double](repeating: 0, count: Hym.mirrorN)
    m[0] = 1; m[1] = 96; m[2] = 2; m[3] = 0.62
    m[4] = 3; m[5] = 6                                             // cursor
    m[6] = 11; m[7] = 8                                            // len
    m[8] = 36; m[9] = 55; m[10] = 72; m[11] = 84                   // lo, hi
    for t in 0..<Hym.nr { for i in 0..<Hym.pearlMax { m[12 + t * 32 + 2 * i] = Double(i) / 16 + Double(t) / 100; m[13 + t * 32 + 2 * i] = (t == 0 && i == 4) || (t == 1 && i == 1) ? 1 : 0 } }
    let o = 12 + 64
    for r in 0..<Hym.nr { for k in 0..<Hym.ringN { m[o + r * Hym.ringN + k] = Double(r * 10 + k + 1) / 100 } }
    let c = o + Hym.nr * Hym.ringN
    for k in 0..<Hym.nck { m[c + k] = Double(k + 1) / 10 }
    m[c + Hym.nck] = 0.5
    return m
  }

  func testMirrorLayout() {
    XCTAssertEqual(Hym.mirrorN, 97); XCTAssertEqual(Hym.nr, 2)
    let m = HymMirror(vector())
    XCTAssertTrue(m.run); XCTAssertEqual(m.bpm, 96); XCTAssertEqual(m.loc, 2); XCTAssertEqual(m.mix, 0.62, accuracy: 1e-9)
    XCTAssertEqual(m.cursor, [3, 6]); XCTAssertEqual(m.len, [11, 8])
    XCTAssertEqual(m.lo, [36, 55]); XCTAssertEqual(m.hi, [72, 84])
    XCTAssertEqual(m.playedIndex(0), 2); XCTAssertEqual(m.playedIndex(1), 5)
    XCTAssertEqual(m.pearls[0][5].pitch, 5.0 / 16, accuracy: 1e-9); XCTAssertTrue(m.pearls[0][4].rest); XCTAssertFalse(m.pearls[0][5].rest)
    XCTAssertEqual(m.pearls[1][3].pitch, 3.0 / 16 + 0.01, accuracy: 1e-9); XCTAssertTrue(m.pearls[1][1].rest); XCTAssertFalse(m.pearls[1][4].rest)
    XCTAssertEqual(m.rings[0].structure, 0.01, accuracy: 1e-9)
    XCTAssertEqual(m.rings[1].structure, 0.11, accuracy: 1e-9); XCTAssertEqual(m.rings[1].poly, 0.15, accuracy: 1e-9); XCTAssertEqual(m.rings[1].energy, 0.16, accuracy: 1e-9)
    XCTAssertEqual(m.rings[1].voices, 1)
    XCTAssertEqual(m.cloud[Hym.Cloud.echo], 0.6, accuracy: 1e-9); XCTAssertEqual(m.cloudEnergy, 0.5)
    let z = HymMirror(Array(vector().prefix(30)))                 // короче — не падает, нули
    XCTAssertFalse(z.run); XCTAssertEqual(z.len, [0, 0]); XCTAssertEqual(z.cloud, [Double](repeating: 0, count: Hym.nck))
    var bad = vector(); bad[1] = .nan; bad[6] = .infinity; bad[12 + 2] = .nan
    let b = HymMirror(bad)
    XCTAssertEqual(b.bpm, 0); XCTAssertEqual(b.len[0], 1); XCTAssertEqual(b.pearls[0][1].pitch, 0)
  }

  // кольца — над нижним баром (safeBot + barH) на iPhone 16, на SE и на 320×568
  func testRingsClearOfBottomBar() {
    for (W, H, safeBot) in [(CGFloat(393), CGFloat(852), CGFloat(34)), (375, 667, 0), (320, 568, 0)] {
      let g = HymGeo(W: W, H: H, topY: 59), barTop = H - safeBot - TopBar.barH
      XCTAssertLessThanOrEqual(g.ringsRect.maxY, barTop, "rings \(W)x\(H)")
    }
  }

  func testGeoNoOverlap() {
    for (W, H) in [(CGFloat(390), CGFloat(844)), (320, 568)] {
      let g = HymGeo(W: W, H: H, topY: 59)
      var zones: [(String, CGRect)] = [("clouds", g.cloudsRect), ("mix", g.mixTrack), ("rings", g.ringsRect)]
      for i in 0..<zones.count { for j in (i + 1)..<zones.count { XCTAssertFalse(zones[i].1.intersects(zones[j].1), "\(zones[i].0) x \(zones[j].0) \(W)") } }
      XCTAssertTrue(g.cloudsRect.contains(g.lensRect), "линза внутри облаков \(W)")
      for (n, r) in zones where n != "clouds" { XCTAssertFalse(g.lensRect.intersects(r), "lens x \(n) \(W)") }
      for z in zones { XCTAssertTrue(CGRect(x: 0, y: 0, width: W, height: H).contains(z.1), "\(z.0) на экране \(W)") }
      for i in 0..<Hym.nr { XCTAssertTrue(g.ringsRect.contains(CGRect(x: g.ringCenter(i).x - g.ringR(i), y: g.ringCenter(i).y - g.ringR(i), width: 2 * g.ringR(i), height: 2 * g.ringR(i)))) }
      XCTAssertLessThan(g.ringCenter(0).x, g.ringCenter(1).x)
      XCTAssertGreaterThan(2 * g.ringR(0), g.ringCenter(1).x - g.ringCenter(0).x, "кольца переплетены \(W)")
    }
  }

  func testLfoKeys() throws {
    XCTAssertEqual(HymenaeusView.ringNK, 5); XCTAssertEqual(HymenaeusView.cloudLfoBase, 10)
    let (v, _) = make(), fx = try XCTUnwrap(v.fx)
    for i in 0..<Hym.nr {
      let spec = v.ringSpec(i)
      for (k, key) in ["structure", "bright", "damp", "lvl"].enumerated() { XCTAssertEqual(spec.rows.first { $0.key == key }?.code, i * 5 + k) }
    }
    for (k, key) in ["pos", "size", "dens", "pitch"].enumerated() { XCTAssertEqual(fx.rows.first { $0.key == key }?.code, 10 + k) }
    XCTAssertEqual(fx.rows.first { $0.key == "send" }?.code, 10 + Hym.Cloud.send)
  }

  func testActNumbersMatchCore() throws {
    let header = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
      .appendingPathComponent("core/worlds/hymenaeus/world.hpp")
    let text = try String(contentsOf: header, encoding: .utf8)
    let line = try XCTUnwrap(text.split(separator: "\n").first { $0.contains("enum Act {") })
    let names = line.split(separator: "{")[1].split(separator: "}")[0].split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces).lowercased() }
    XCTAssertEqual(names, ["ring", "cloud", "pearl", "strike", "thread", "dice", "clear", "run", "bpm", "timbre", "loc", "mix"])
    XCTAssertEqual(names.count, 12)
    for (i, n) in names.enumerated() { XCTAssertEqual(String(describing: HymAct(rawValue: i)!), n); XCTAssertEqual(HymAct(rawValue: i)?.rawValue, i) }
    XCTAssertNil(HymAct(rawValue: 12))
    // MIRROR_N — из комментария-описания зеркала в шапке world.hpp: «(MIRROR_N = 97 чисел)»
    let m = try XCTUnwrap(text.range(of: "MIRROR_N = ")), digits = text[m.upperBound...].prefix { $0.isNumber }
    XCTAssertEqual(Int(digits), Hym.mirrorN)
  }

  func testRingTapStrikes() {
    let rec = HymRec()
    let v = HymenaeusView(core: rec, readMirror: { _ in 0 }, readHeard: { _ in 0 })
    v.layout(W: 390, H: 844, topY: 59, safeBot: 34)
    let g = v.geo, c = g.ringCenter(1), R = g.ringR(1)
    func tap(_ x: CGFloat, _ y: CGFloat) {
      let f = Finger(id: ObjectIdentifier(rec), x: x, y: y, x0: x, y0: y, t0: 0, moved: false, touchTime: 1)
      v.down(f); v.up(f)
    }
    tap(c.x + R, c.y)                                            // правая точка обода кольца II: место удара 0.25
    XCTAssertEqual(rec.acts.count, 1)
    XCTAssertEqual(rec.acts[0].kind, HymAct.strike.rawValue); XCTAssertEqual(rec.acts[0].i, 1)
    XCTAssertEqual(rec.acts[0].a[0], 0.25, accuracy: 0.02)
    tap(g.pt(20, 500).x, g.pt(20, 500).y)                        // мимо колец — ничего
    XCTAssertEqual(rec.acts.count, 1)
    XCTAssertTrue(v.cornerHit(10, g.sun.y + 10))
    XCTAssertFalse(v.cornerHit(200, 400))
  }

  func testHeardFlashesRing() {
    var q: [[Double]] = [[2, 0, 4, 1, -1]]
    let v = HymenaeusView(core: HymRec(), readMirror: { _ in 0 }, readHeard: { b in
      guard !q.isEmpty else { return 0 }
      let r = q.removeFirst(); b.replaceSubrange(0..<r.count, with: r); return r.count
    })
    v.tick(now: 5, dt: 0.016)
    XCTAssertEqual(v.ringHit[0], 5); XCTAssertEqual(v.ringHit[1], 5)
    XCTAssertEqual(v.notes.filter { $0.born == 5 }.count, 2)
    XCTAssertEqual(v.notes.first { $0.born == 5 && $0.ring == 0 }?.pearl, 4, "нота помнит свою жемчужину")
    XCTAssertEqual(v.notes.first { $0.born == 5 && $0.ring == 1 }?.pearl, -1, "ручной удар — без жемчужины")
  }

  // нота стартует над своим кольцом: pitch — высота из зеркала, точка старта зависит от высоты; ручной удар без полёта
  func testStrikeLaunchesAboveRing() {
    let mir = vector()
    var q: [[Double]] = [[2, 0, 4, 1, 0]]
    let v = HymenaeusView(core: HymRec(), readMirror: { b in b.replaceSubrange(0..<mir.count, with: mir); return mir.count }, readHeard: { b in
      guard !q.isEmpty else { return 0 }
      let r = q.removeFirst(); b.replaceSubrange(0..<r.count, with: r); return r.count
    })
    v.tick(now: 3, dt: 0.016)
    let a = v.notes.first { $0.born == 3 && $0.ring == 0 }
    XCTAssertEqual(a?.pitch ?? -1, v.mirror.pearls[0][4].pitch, accuracy: 1e-9)
    let g = v.geo, lo = g.launchPoint(0, pitch: 0.1), hi = g.launchPoint(0, pitch: 0.9)
    XCTAssertEqual(lo.y, hi.y, accuracy: 1e-9); XCTAssertGreaterThan(hi.x, lo.x, "выше нота — правее")
    XCTAssertLessThan(g.launchPoint(0, pitch: 0.5).y, g.ringCenter(0).y - g.ringR(0), "старт над кольцом")
    XCTAssertEqual(g.launchPoint(1, pitch: 0.5).x, g.ringCenter(1).x, accuracy: 1e-9)
  }

  // ---------- жесты по осям ----------
  private func make(_ mv: [Double]? = nil) -> (HymenaeusView, HymRec) {
    let rec = HymRec(); let mir = mv ?? vector()
    let v = HymenaeusView(core: rec, readMirror: { b in b.replaceSubrange(0..<mir.count, with: mir); return mir.count }, readHeard: { _ in 0 })
    v.layout(W: 390, H: 844, topY: 59, safeBot: 34); v.tick(now: 1, dt: 0.016)
    return (v, rec)
  }
  private func fin(_ id: AnyObject, _ x: CGFloat, _ y: CGFloat, _ x0: CGFloat, _ y0: CGFloat) -> Finger {
    Finger(id: ObjectIdentifier(id), x: x, y: y, x0: x0, y0: y0, t0: 0, moved: false, touchTime: 1)
  }
  private func acts(_ rec: HymRec, _ kind: HymAct) -> [(i: Int, a: [Double])] { rec.acts.filter { $0.kind == kind.rawValue }.map { ($0.i, $0.a) } }

  func testCloudDragMapsAxes() {
    let (v, rec) = make(), cr = v.geo.cloudsRect, k = NSObject()
    v.down(fin(k, cr.minX + 20, cr.midY, 0, 0))
    v.move(fin(k, cr.minX + cr.width * 0.25, cr.minY + cr.height * 0.45 * 0.9, 0, 0))     // четверть по X, середина по Y (шкала без полосы echo)
    let a = acts(rec, .cloud)
    XCTAssertEqual(a[0].a[0], Double(Hym.Cloud.pos)); XCTAssertEqual(a[0].a[1], 0.25, accuracy: 0.01)
    XCTAssertEqual(a[1].a[0], Double(Hym.Cloud.size)); XCTAssertEqual(a[1].a[1], 0.55, accuracy: 0.01)
    v.move(fin(k, -500, -500, 0, 0))                                                      // за краем — зажато
    let b = Array(acts(rec, .cloud).suffix(2))
    XCTAssertEqual(b[0].a[1], 0); XCTAssertEqual(b[1].a[1], 1)
    XCTAssertEqual(v.activeLabels.first?.text, "positio · granum")
    v.up(fin(k, 0, 0, 0, 0)); XCTAssertTrue(v.activeLabels.isEmpty)
  }

  func testHoldFreezes() {
    let (v, rec) = make(), cr = v.geo.cloudsRect, k = NSObject()
    v.down(fin(k, cr.midX, cr.midY, 0, 0))
    v.tick(now: 1.1, dt: 0.016)
    XCTAssertTrue(acts(rec, .cloud).isEmpty)                                              // ещё рано
    v.tick(now: 1.6, dt: 0.016)
    XCTAssertEqual(acts(rec, .cloud).last?.a, [Double(Hym.Cloud.freeze), 1])
    XCTAssertEqual(v.activeLabels.first?.text, "glacies · freeze")
    v.up(fin(k, cr.midX, cr.midY, 0, 0))
    XCTAssertEqual(acts(rec, .cloud).last?.a, [Double(Hym.Cloud.freeze), 0])              // отпустил — оттаяло
  }

  func testEchoStrip() {
    let (v, rec) = make(), cr = v.geo.cloudsRect, k = NSObject()
    v.down(fin(k, cr.minX + 10, cr.maxY - 3, 0, 0))
    v.move(fin(k, cr.minX + cr.width * 0.7, cr.maxY - 3, 0, 0))
    XCTAssertEqual(acts(rec, .cloud).last?.a[0], Double(Hym.Cloud.echo))
    XCTAssertEqual(acts(rec, .cloud).last?.a[1] ?? 0, 0.7, accuracy: 0.01)
    XCTAssertEqual(v.activeLabels.first?.text, "echo")
  }

  func testRingAxes() {
    let (v, rec) = make(), k = NSObject(), c = v.geo.ringCenter(0), R = v.geo.ringR(0)
    let b0 = v.mirror.rings[0].bright, d0 = v.mirror.rings[0].damp
    v.down(fin(k, c.x + R, c.y, 0, 0))
    v.move(fin(k, c.x + R, c.y - 40 * v.geo.S, 0, 0))                                     // вверх — ярче
    var a = acts(rec, .ring)
    XCTAssertEqual(a[0].a[0], Double(Hym.Ring.bright)); XCTAssertGreaterThan(a[0].a[1], b0)
    XCTAssertEqual(a[1].a[0], Double(Hym.Ring.damp)); XCTAssertEqual(a[1].a[1], d0, accuracy: 1e-9)          // по горизонтали не сдвинулся
    v.move(fin(k, c.x + R + 40 * v.geo.S, c.y, 0, 0))                                     // вправо — затухание
    a = acts(rec, .ring)
    XCTAssertGreaterThan(a[a.count - 1].a[1], d0)
    XCTAssertEqual(a[a.count - 2].a[1], b0, accuracy: 1e-9)
    v.up(fin(k, 0, 0, 0, 0))
    XCTAssertTrue(acts(rec, .strike).isEmpty)                                              // перетаскивание не бьёт
  }

  func testPinchStructure() {
    let (v, rec) = make(), a = NSObject(), b = NSObject(), c = v.geo.ringCenter(0), R = v.geo.ringR(0), S = v.geo.S
    let s0 = v.mirror.rings[0].structure
    v.down(fin(a, c.x, c.y - R, 0, 0)); v.down(fin(b, c.x, c.y + R, 0, 0))
    v.move(fin(a, c.x, c.y - R - 30 * S, 0, 0))                                     // разводим — structure растёт
    let p = acts(rec, .ring)
    XCTAssertEqual(p.last?.a[0], Double(Hym.Ring.structure))
    XCTAssertGreaterThan(p.last!.a[1], s0)
    v.up(fin(a, 0, 0, 0, 0)); v.up(fin(b, 0, 0, 0, 0))
    XCTAssertTrue(acts(rec, .strike).isEmpty)
  }

  // нотоносцев, застёжки и печатей нет: тап, перетаскивание и удержание там, где они были, ничего не шлют
  func testFormerStaveSpotsDoNothing() {
    let (v, rec) = make(), k = NSObject(), g = v.geo
    let spots = [g.pt(42, 384), g.pt(200, 384), g.pt(356, 384), g.pt(42, 478), g.pt(356, 478), g.pt(105, 722), g.pt(291, 722)]    // линии, застёжки, печати прежней раскладки
    for p in spots {
      v.down(fin(k, p.x, p.y, 0, 0)); v.up(fin(k, p.x, p.y, 0, 0))                           // тап
      v.down(fin(k, p.x, p.y, 0, 0)); v.move(fin(k, p.x + 40 * g.S, p.y - 30 * g.S, 0, 0)); v.up(fin(k, p.x, p.y, 0, 0))   // перетаскивание
      v.down(fin(k, p.x, p.y, 0, 0)); v.tick(now: 1.6, dt: 0.016); v.up(fin(k, p.x, p.y, 0, 0))                              // удержание
      XCTAssertNil(HymTouch.zone(at: p, geo: g))
    }
    XCTAssertTrue(rec.acts.isEmpty); XCTAssertTrue(v.activeLabels.isEmpty)
  }

  func testDragOutsideZonesIgnored() {
    let (v, rec) = make(), k = NSObject(), g = v.geo
    let p = g.pt(20, 560)                                                                  // между облаками и кольцами
    XCTAssertNil(HymTouch.zone(at: p, geo: g))
    v.down(fin(k, p.x, p.y, 0, 0)); v.move(fin(k, p.x + 80, p.y - 80, 0, 0)); v.up(fin(k, p.x, p.y, 0, 0))
    XCTAssertTrue(rec.acts.isEmpty)
  }

  func testNaNPointIgnored() {
    let (v, rec) = make(), k = NSObject(), cr = v.geo.cloudsRect
    XCTAssertNil(HymTouch.zone(at: CGPoint(x: CGFloat.nan, y: 100), geo: v.geo))
    v.down(fin(k, CGFloat.nan, CGFloat.nan, 0, 0)); v.up(fin(k, CGFloat.nan, CGFloat.nan, 0, 0))
    v.down(fin(k, cr.midX, cr.midY, 0, 0)); v.move(fin(k, CGFloat.nan, 50, 0, 0)); v.move(fin(k, 50, CGFloat.infinity, 0, 0))
    XCTAssertTrue(rec.acts.isEmpty)
    XCTAssertNil(HymTouch.pinch(ring: 0, d0: CGFloat.nan, d: 10, geo: v.geo, base: HymBase()))
  }

  // ---------- плашки ----------
  func testSpecKeysMatchCore() throws {
    let (v, _) = make(), fx = try XCTUnwrap(v.fx)
    let text = try String(contentsOf: URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
      .appendingPathComponent("core/worlds/hymenaeus/model.hpp"), encoding: .utf8)
    XCTAssertTrue(text.contains("enum CloudKey { POS, SIZE, DENS, PITCH, FREEZE, ECHO, CLEVEL, SEND, NCK }"))
    XCTAssertTrue(text.contains("enum RingKey"))
    XCTAssertEqual(Set(fx.rows.map(\.key)), ["pos", "size", "dens", "pitch", "freeze", "echo", "level", "send", "mix"])
    XCTAssertEqual(HymenaeusView.cloudRows.filter { $0.kind == .cloud }.map(\.idx).sorted(), Array(0..<Hym.nck))
    let ring = v.ringSpec(1)
    XCTAssertEqual(ring.rows.map(\.key), ["structure", "bright", "damp", "lvl", "poly", "len"])
    XCTAssertEqual(ring.rows.map(\.code), [5, 6, 7, 8, -1, -1])                              // LFO кольца 1: ring·NRK + ключ
    XCTAssertEqual(fx.rows.first { $0.key == "echo" }?.code, 10 + Hym.Cloud.echo)            // облака — после NR·NRK колец
    XCTAssertEqual(fx.rows.first { $0.key == "freeze" }?.code, -1)
    for k in ring.rows.map(\.key) { XCTAssertFalse(ring.text(k).isEmpty); XCTAssertTrue((0.0...1.0).contains(ring.get(k))) }
  }

  func testSliderWritesSameAction() throws {
    let (v, rec) = make(), fx = try XCTUnwrap(v.fx), cr = v.geo.cloudsRect, k = NSObject()
    fx.set("pos", 0.25)
    v.down(fin(k, cr.minX + 5, cr.midY, 0, 0)); v.move(fin(k, cr.minX + cr.width * 0.25, cr.midY, 0, 0))
    let a = acts(rec, .cloud)
    XCTAssertEqual(a[0].a, a[1].a)                                                            // ползунок и жест — одно действие
    let ring = v.ringSpec(0), n = rec.acts.count
    ring.set("poly", 0.9)
    XCTAssertEqual(Array(rec.acts[n...]).map { $0.a }, [[Double(Hym.Ring.poly), 1]])
    ring.tap?("poly")                                                                         // 1 голос в векторе → 2 (0.5)
    XCTAssertEqual(rec.acts.last?.a, [Double(Hym.Ring.poly), 0.5])
  }

  func testStepsClamp() throws {
    XCTAssertEqual(HymenaeusView.lenFrom(-3), 4); XCTAssertEqual(HymenaeusView.lenFrom(9), 16)
    XCTAssertEqual(HymenaeusView.lenFrom(.nan), 4); XCTAssertEqual(HymenaeusView.lenFrom(0.5), 10)
    let (v, rec) = make(), ring = v.ringSpec(1)
    ring.set("len", 5); ring.set("len", -5)
    XCTAssertEqual(acts(rec, .thread).map { $0.a }, [[0, 16], [0, 4]]); XCTAssertEqual(acts(rec, .thread).map { $0.i }, [1, 1])
    XCTAssertEqual(ring.text("len"), "8 pearls")
  }

  func testRingHoldOpensSheet() {
    let (v, rec) = make(), k = NSObject(), c = v.geo.ringCenter(1), R = v.geo.ringR(1)
    var opened: [String] = []
    v.openSheet = { opened.append($0.title) }
    v.down(fin(k, c.x + R, c.y, 0, 0)); v.tick(now: 1.6, dt: 0.016); v.up(fin(k, c.x + R, c.y, 0, 0))
    XCTAssertEqual(opened, ["chorda"]); XCTAssertTrue(acts(rec, .strike).isEmpty)
  }

  // ---------- ревью: правки ----------
  func testPinchUsesCurrentPositionsOfBothFingers() {
    let (v, rec) = make(), a = NSObject(), b = NSObject(), c = v.geo.ringCenter(0), R = v.geo.ringR(0), S = v.geo.S
    let s0 = v.mirror.rings[0].structure
    v.down(fin(a, c.x, c.y - R, 0, 0)); v.down(fin(b, c.x, c.y + R, 0, 0))
    v.move(fin(a, c.x, c.y - R - 15 * S, 0, 0)); v.move(fin(b, c.x, c.y + R + 15 * S, 0, 0))     // оба разошлись на 15 — вместе на 30
    let want = s0 + Double(30 * S / S / HymTouch.pinchSpan)
    XCTAssertEqual(acts(rec, .ring).last?.a[1] ?? 0, want, accuracy: 1e-6)
  }

  func testEnterStartsOnlyOnce() {
    let (v, rec) = make()
    v.enter(); v.enter()
    XCTAssertEqual(acts(rec, .run).count, 1)                                                  // остановили — повторный вход не будит
    let (w, rec2) = make(); w.restored(); w.enter()
    XCTAssertTrue(acts(rec2, .run).isEmpty)                                                   // после restore run решает снимок
  }

  func testLenRowClampedBelowMin() throws {
    var m = vector(); m[7] = 2
    let (v, _) = make(m), ring = v.ringSpec(1)
    XCTAssertEqual(ring.get("len"), 0)
  }

  // ---------- Task 6: вид C, зоны, кнопки ----------
  func testHitZones() {
    let (v, _) = make(), g = v.geo
    func hit(_ p: CGPoint) -> HymHit { HymTouch.hit(p, geo: g) }
    XCTAssertEqual(hit(g.pt(200, 384)), HymHit.none, "на месте бывшей нити пусто")
    for i in 0..<Hym.nr { let c = g.ringCenter(i); if case .ring(let r, _) = hit(CGPoint(x: c.x, y: c.y - g.ringR(i))) { XCTAssertEqual(r, i) } else { XCTFail("ring \(i)") } }
    XCTAssertEqual(hit(CGPoint(x: g.mixTrack.midX, y: g.mixTrack.midY)), .mix)
    XCTAssertEqual(hit(CGPoint(x: g.cloudsRect.midX, y: g.cloudsRect.minY + 20)), .cloud)
    XCTAssertEqual(hit(CGPoint(x: g.cloudsRect.midX, y: g.cloudsRect.maxY - 3)), .echo)
    XCTAssertEqual(hit(g.pt(20, 560)), HymHit.none)
    XCTAssertEqual(hit(CGPoint(x: CGFloat.nan, y: 1)), HymHit.none)
  }

  func testNoRangeNoPearlUi() {
    let (v, rec) = make(), ring = v.ringSpec(1)
    XCTAssertEqual(HymenaeusView.ringRows.map(\.key), ["structure", "bright", "damp", "lvl", "poly", "len"], "ни ima, ни summa")
    ring.set("len", 0.5)
    XCTAssertEqual(acts(rec, .thread).map { $0.a }, [[0, 10]])
    XCTAssertEqual(rec.acts.filter { $0.kind == HymAct.pearl.rawValue }.count, 0)
    XCTAssertEqual(HymAct.pearl.rawValue, 2)                                                // номер в ядре остался, интерфейс его не шлёт
  }

  func testMixDragAndTap() {
    let (v, rec) = make(), k = NSObject(), t = v.geo.mixTrack, x0 = v.geo.pt(HymGeo.mixX0, 0).x, x1 = v.geo.pt(HymGeo.mixX1, 0).x
    v.down(fin(k, x0 + (x1 - x0) * 0.5, t.midY, 0, 0)); v.move(fin(k, x0 + (x1 - x0) * 0.8, t.midY, 0, 0))
    XCTAssertEqual(acts(rec, .mix).last?.a[0] ?? 0, 0.8, accuracy: 1e-6)
    v.up(fin(k, x0 + (x1 - x0) * 0.8, t.midY, 0, 0))
    v.down(fin(k, x0 + (x1 - x0) * 0.25, t.midY, 0, 0)); v.up(fin(k, x0 + (x1 - x0) * 0.25, t.midY, 0, 0))
    XCTAssertEqual(acts(rec, .mix).last?.a[0] ?? 0, 0.25, accuracy: 1e-6)
  }

  func testDiceButtons() {
    let (v, rec) = make()
    XCTAssertTrue(v.hasTimbre)
    v.dice(); v.timbre()
    XCTAssertEqual(rec.acts.map { $0.kind }, [HymAct.dice.rawValue, HymAct.timbre.rawValue])
    XCTAssertEqual(rec.acts[0].a, [])
    XCTAssertEqual(HymAct.timbre.rawValue, 9); XCTAssertEqual(HymAct.dice.rawValue, 5)
  }

  func testClearButtonCallsClear() {
    let (v, rec) = make()
    v.clear()
    XCTAssertEqual(rec.acts.map { $0.kind }, [HymAct.clear.rawValue])
  }

  // CLEAR и отмена: снимок до очистки возвращает ноты (живое ядро, как делает хост: snap → CLEAR → restore)
  func testClearThenUndoRestoresState() throws {
    t2d_app_init(48000, 1, 1e9, 1 << 14)                           // watch — маска слотов: следим за слотом 14
    let core = AppCore(slot: HymenaeusView.slotN), host = AppHostCore()
    var l = [Float](repeating: 0, count: 256), r = l, buf = [Double](repeating: 0, count: 128)
    func render() { for _ in 0..<4 { t2d_app_render(&l, &r, 256, 0) } }
    func mirror() -> HymMirror { render(); let n = Int(t2d_app_mirror(Int32(HymenaeusView.slotN), &buf, 128)); return HymMirror(Array(buf[0..<n])) }
    core.action(HymAct.dice.rawValue, 0, [], touchTime: 0)
    core.action(HymAct.pearl.rawValue, 2, [0, 0, 0.9], touchTime: 0)
    let before = mirror()
    XCTAssertTrue(host.snap(slot: HymenaeusView.slotN, id: 1))
    var snap: (id: Int, slot: Int, w: String)?
    for _ in 0..<20 where snap == nil { render(); snap = host.takeSnap() }
    let s = try XCTUnwrap(snap)
    core.action(HymAct.clear.rawValue, 0, [], touchTime: 0)
    let cleared = mirror()
    XCTAssertTrue((0..<Hym.nr).allSatisfy { t in cleared.pearls[t].allSatisfy { $0.rest } })          // после CLEAR тишина
    XCTAssertFalse(before.pearls[0].prefix(before.len[0]).allSatisfy { $0.rest })
    XCTAssertTrue(host.restore(slot: HymenaeusView.slotN, s.w))
    let after = mirror()
    XCTAssertEqual(after.pearls[0].map { $0.pitch }, before.pearls[0].map { $0.pitch })
    XCTAssertEqual(after.pearls[1].map { $0.pitch }, before.pearls[1].map { $0.pitch })
    XCTAssertEqual(after.pearls[0].map { $0.rest }, before.pearls[0].map { $0.rest })
    XCTAssertEqual(after.len, before.len)
  }
}
