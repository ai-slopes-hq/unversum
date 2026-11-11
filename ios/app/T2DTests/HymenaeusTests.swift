import XCTest
@testable import T2D
private final class HymRec: Core {
  var acts: [(kind: Int, i: Int, a: [Double])] = []
  func action(_ kind: Int, _ i: Int, _ a: [Double], touchTime: Double) -> Bool { acts.append((kind, i, a)); return true }
}

// Hymenaeus: разбор зеркала, раскладка зон, номера действий, тап-удар
final class HymenaeusTests: XCTestCase {
  // 99 чисел по порядку world.hpp: голова 14, жемчужины 2×32, кольца 2×6, облака 8, энергия
  private func vector() -> [Double] {
    var m = [Double](repeating: 0, count: Hym.mirrorN)
    m[0] = 1; m[1] = 96; m[2] = 2; m[3] = 0.62
    m[4] = 3; m[5] = 6                                             // cursor
    m[6] = 11; m[7] = 8                                            // len
    m[8] = 0.7; m[9] = 0.4                                         // dejavu
    m[10] = 36; m[11] = 55; m[12] = 72; m[13] = 84                 // lo, hi
    for t in 0..<Hym.nr { for i in 0..<Hym.pearlMax { m[14 + t * 32 + 2 * i] = Double(i) / 16 + Double(t) / 100; m[15 + t * 32 + 2 * i] = (t == 0 && i == 4) || (t == 1 && i == 1) ? 1 : 0 } }
    let o = 14 + 64
    for r in 0..<Hym.nr { for k in 0..<Hym.ringN { m[o + r * Hym.ringN + k] = Double(r * 10 + k + 1) / 100 } }
    let c = o + Hym.nr * Hym.ringN
    for k in 0..<Hym.nck { m[c + k] = Double(k + 1) / 10 }
    m[c + Hym.nck] = 0.5
    return m
  }

  func testMirrorLayout() {
    XCTAssertEqual(Hym.mirrorN, 99); XCTAssertEqual(Hym.nr, 2)
    let m = HymMirror(vector())
    XCTAssertTrue(m.run); XCTAssertEqual(m.bpm, 96); XCTAssertEqual(m.loc, 2); XCTAssertEqual(m.mix, 0.62, accuracy: 1e-9)
    XCTAssertEqual(m.cursor, [3, 6]); XCTAssertEqual(m.len, [11, 8])
    XCTAssertEqual(m.dejavu[0], 0.7, accuracy: 1e-9); XCTAssertEqual(m.dejavu[1], 0.4, accuracy: 1e-9)
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
    var bad = vector(); bad[1] = .nan; bad[6] = .infinity; bad[14 + 2] = .nan
    let b = HymMirror(bad)
    XCTAssertEqual(b.bpm, 0); XCTAssertEqual(b.len[0], 1); XCTAssertEqual(b.pearls[0][1].pitch, 0)
  }

  func testGeoNoOverlap() {
    for (W, H) in [(CGFloat(390), CGFloat(844)), (320, 568)] {
      let g = HymGeo(W: W, H: H, topY: 59)
      var zones: [(String, CGRect)] = [("clouds", g.cloudsRect), ("mix", g.mixTrack), ("stave0", g.staveRect(0)), ("stave1", g.staveRect(1)), ("rings", g.ringsRect)]
      for l in 0..<Hym.locN { zones.append(("seal\(l)", g.sealRect(l))) }
      for i in 0..<zones.count { for j in (i + 1)..<zones.count { XCTAssertFalse(zones[i].1.intersects(zones[j].1), "\(zones[i].0) x \(zones[j].0) \(W)") } }
      XCTAssertTrue(g.cloudsRect.contains(g.lensRect), "линза внутри облаков \(W)")
      XCTAssertLessThan(g.staveY(0), g.staveY(1))
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

  func testClaspDragDejaVu() {
    let (v, rec) = make(), k = NSObject(), p = v.geo.claspPoint(0, lastPitch: v.mirror.pearls[0][v.mirror.len[0] - 1].pitch)
    v.down(fin(k, p.x, p.y, 0, 0))
    v.move(fin(k, p.x + 39 * v.geo.S, p.y, 0, 0))
    XCTAssertEqual(acts(rec, .thread).last?.i, 0); XCTAssertEqual(acts(rec, .thread).last?.a[0], 0)
    XCTAssertEqual(acts(rec, .thread).last?.a[1] ?? 0, 0.7 + 0.3, accuracy: 0.01)           // 0.7 + 39/130 = 1.0
    XCTAssertEqual(v.activeLabels.first?.text, "déjà vu")
  }

  func testPearlTapTogglesRest() {
    let (v, rec) = make(), k = NSObject()
    for (t, i, want) in [(0, 0, 1.0), (0, 4, 0.0), (1, 1, 0.0)] {                          // пауза в векторе: нить 0 жемчужина 4, нить 1 жемчужина 1
      let n = v.mirror.len[t], p = v.geo.pearlPoint(t, i, n: n, pitch: v.mirror.pearls[t][i].pitch)
      v.down(fin(k, p.x, p.y, 0, 0)); v.up(fin(k, p.x, p.y, 0, 0))
      XCTAssertEqual(acts(rec, .pearl).last?.i, i)
      XCTAssertEqual(acts(rec, .pearl).last?.a, [Double(t), 1, want], "\(t) \(i)")
    }
  }

  func testPearlVerticalPitch() {
    let (v, rec) = make(), k = NSObject(), n = v.mirror.len[1], p0 = v.mirror.pearls[1][2].pitch, p = v.geo.pearlPoint(1, 2, n: n, pitch: p0)
    v.down(fin(k, p.x, p.y, 0, 0))
    v.move(fin(k, p.x, p.y - 22 * v.geo.S, 0, 0))                                          // вверх — выше
    XCTAssertEqual(acts(rec, .pearl).last?.i, 2); XCTAssertEqual(acts(rec, .pearl).last?.a[0], 1); XCTAssertEqual(acts(rec, .pearl).last?.a[1], 0)
    XCTAssertEqual(acts(rec, .pearl).last?.a[2] ?? 0, p0 + 0.2, accuracy: 0.01)
    v.up(fin(k, p.x, p.y - 22 * v.geo.S, 0, 0))
    XCTAssertTrue(acts(rec, .pearl).allSatisfy { $0.a[1] == 0 })                            // перетащил — пауза не переключилась
  }

  func testDragOutsideZonesIgnored() {
    let (v, rec) = make(), k = NSObject(), g = v.geo
    let p = g.pt(20, 560)                                                                  // между станами и кольцами
    XCTAssertNil(HymTouch.zone(at: p, geo: g, mirror: v.mirror))
    v.down(fin(k, p.x, p.y, 0, 0)); v.move(fin(k, p.x + 80, p.y - 80, 0, 0)); v.up(fin(k, p.x, p.y, 0, 0))
    XCTAssertTrue(rec.acts.isEmpty)
  }

  func testNaNPointIgnored() {
    let (v, rec) = make(), k = NSObject(), cr = v.geo.cloudsRect
    XCTAssertNil(HymTouch.zone(at: CGPoint(x: CGFloat.nan, y: 100), geo: v.geo, mirror: v.mirror))
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
    XCTAssertEqual(acts(rec, .thread).map { $0.a }, [[1, 16], [1, 4]]); XCTAssertEqual(acts(rec, .thread).map { $0.i }, [1, 1])
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
}
