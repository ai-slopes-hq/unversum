import XCTest
@testable import T2D
private final class HymRec: Core {
  var acts: [(kind: Int, i: Int, a: [Double])] = []
  func action(_ kind: Int, _ i: Int, _ a: [Double], touchTime: Double) -> Bool { acts.append((kind, i, a)); return true }
}

// Hymenaeus: разбор зеркала, раскладка зон, номера действий, тап-удар
final class HymenaeusTests: XCTestCase {
  private func vector() -> [Double] {
    var m = [Double](repeating: 0, count: Hym.mirrorN)
    m[0] = 1; m[1] = 96; m[2] = 0.7; m[3] = 11; m[4] = 3
    for i in 0..<Hym.pearlMax { m[5 + 2 * i] = Double(i) / 16; m[6 + 2 * i] = i == 4 ? 1 : 0 }
    let o = 5 + 32
    for r in 0..<Hym.nr { for k in 0..<Hym.ringN { m[o + r * Hym.ringN + k] = Double(r * 10 + k + 1) / 100 } }
    let c = o + Hym.nr * Hym.ringN
    for k in 0..<Hym.nck { m[c + k] = Double(k + 1) / 10 }
    m[c + Hym.nck] = 0.5
    return m
  }

  func testMirrorParse() {
    XCTAssertEqual(Hym.mirrorN, 67)
    let m = HymMirror(vector())
    XCTAssertTrue(m.run); XCTAssertEqual(m.bpm, 96); XCTAssertEqual(m.dejavu, 0.7, accuracy: 1e-9)
    XCTAssertEqual(m.len, 11); XCTAssertEqual(m.cursor, 3); XCTAssertEqual(m.playedIndex, 2)
    XCTAssertEqual(m.pearls[5].pitch, 5.0 / 16, accuracy: 1e-9); XCTAssertTrue(m.pearls[4].rest); XCTAssertFalse(m.pearls[5].rest)
    XCTAssertEqual(m.rings[1].structure, 0.11, accuracy: 1e-9); XCTAssertEqual(m.rings[1].energy, 0.17, accuracy: 1e-9)
    XCTAssertEqual(m.rings[1].mode, 0.15, accuracy: 1e-9); XCTAssertEqual(m.rings[1].poly, 0.16, accuracy: 1e-9)
    XCTAssertFalse(m.rings[1].strings); XCTAssertEqual(m.rings[1].voices, 1)
    XCTAssertEqual(m.rings[2].lvl, 0.24, accuracy: 1e-9)
    XCTAssertEqual(m.cloud[Hym.Cloud.echo], 0.6, accuracy: 1e-9); XCTAssertEqual(m.cloudEnergy, 0.5)
    let z = HymMirror(Array(vector().prefix(30)))                 // короче — не падает, нули
    XCTAssertFalse(z.run); XCTAssertEqual(z.len, 0); XCTAssertEqual(z.cloud, [Double](repeating: 0, count: Hym.nck))
    var bad = vector(); bad[1] = .nan; bad[3] = .infinity; bad[7] = .nan
    let b = HymMirror(bad)
    XCTAssertEqual(b.bpm, 0); XCTAssertEqual(b.len, 1); XCTAssertEqual(b.pearls[1].pitch, 0)
  }

  func testGeoZonesDisjoint() {
    for (W, H) in [(CGFloat(390), CGFloat(844)), (430, 932)] {
      let g = HymGeo(W: W, H: H, topY: 59)
      XCTAssertFalse(g.cloudsRect.intersects(g.neckRect), "\(W)")
      XCTAssertFalse(g.cloudsRect.intersects(g.ringsRect), "\(W)")
      XCTAssertTrue(g.neckRect.contains(g.ringsRect), "кольца лежат внутри нити \(W)")
      let cx = g.neckRect.midX, cy = g.neckRect.midY, rx = HymGeo.neckRx * g.S, ry = HymGeo.neckRy * g.S
      for i in 0..<Hym.nr {                                       // обод каждого кольца целиком внутри эллипса нити
        let c = g.ringCenter(i), R = g.ringRadius(i) + HymGeo.ringW * g.S / 2
        for k in 0..<16 {
          let a = CGFloat(k) / 16 * 2 * .pi, u = (c.x + cos(a) * R - cx) / rx, v = (c.y + sin(a) * R - cy) / ry
          XCTAssertLessThan(u * u + v * v, 1, "кольцо \(i) внутри нити \(W)")
        }
      }
      XCTAssertLessThanOrEqual(g.neckRect.maxY, H); XCTAssertGreaterThanOrEqual(g.neckRect.minX, 0); XCTAssertLessThanOrEqual(g.neckRect.maxX, W)
    }
  }

  func testActNumbersMatchCore() throws {
    let header = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
      .appendingPathComponent("core/worlds/hymenaeus/world.hpp")
    let text = try String(contentsOf: header, encoding: .utf8)
    let line = try XCTUnwrap(text.split(separator: "\n").first { $0.contains("enum Act {") })
    let names = line.split(separator: "{")[1].split(separator: "}")[0].split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces).lowercased() }
    XCTAssertEqual(names.count, 9)
    for (i, n) in names.enumerated() { XCTAssertEqual(String(describing: HymAct(rawValue: i)!), n) }
  }

  func testRingTapStrikes() {
    let rec = HymRec()
    let v = HymenaeusView(core: rec, readMirror: { _ in 0 }, readHeard: { _ in 0 })
    v.layout(W: 390, H: 844, topY: 59, safeBot: 34)
    let g = v.geo, c = g.ringCenter(1), R = g.ringRadius(1)
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
    var q: [[Double]] = [[2, 0, 4, 2, -1]]
    let v = HymenaeusView(core: HymRec(), readMirror: { _ in 0 }, readHeard: { b in
      guard !q.isEmpty else { return 0 }
      let r = q.removeFirst(); b.replaceSubrange(0..<r.count, with: r); return r.count
    })
    v.tick(now: 5, dt: 0.016)
    XCTAssertEqual(v.ringHit[0], 5); XCTAssertEqual(v.ringHit[2], 5); XCTAssertEqual(v.ringHit[1], -1e9)
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
    let (v, rec) = make(), k = NSObject(), c = v.geo.ringCenter(0), R = v.geo.ringRadius(0)
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
    let (v, rec) = make(), a = NSObject(), b = NSObject(), c = v.geo.ringCenter(0), R = v.geo.ringRadius(0), S = v.geo.S
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
    let (v, rec) = make(), k = NSObject(), p = v.geo.claspPoints(dejavu: v.mirror.dejavu)[0]
    v.down(fin(k, p.x, p.y, 0, 0))
    v.move(fin(k, p.x + 39 * v.geo.S, p.y, 0, 0))
    XCTAssertEqual(acts(rec, .thread).last?.a[0], 0)
    XCTAssertEqual(acts(rec, .thread).last?.a[1] ?? 0, 0.7 + 0.3, accuracy: 0.01)           // 0.7 + 39/130 = 1.0
    XCTAssertEqual(v.activeLabels.first?.text, "déjà vu")
  }

  func testPearlTapTogglesRest() {
    let (v, rec) = make(), k = NSObject(), n = v.mirror.len
    for (i, rest) in [(0, true), (4, false)] {                                             // жемчужина 4 в векторе — пауза
      let p = v.geo.pearlPoint(i == 0 ? 0 : i, n: n, dejavu: v.mirror.dejavu)
      v.down(fin(k, p.x, p.y, 0, 0)); v.up(fin(k, p.x, p.y, 0, 0))
      XCTAssertEqual(acts(rec, .pearl).last?.i, i)
      XCTAssertEqual(acts(rec, .pearl).last?.a, [1, v.mirror.pearls[i].rest ? 0 : 1], "\(i) \(rest)")
    }
    XCTAssertEqual(acts(rec, .pearl).last?.a, [1, 0])                                      // пауза снимается
  }

  func testPearlVerticalPitch() {
    let (v, rec) = make(), k = NSObject(), n = v.mirror.len, p = v.geo.pearlPoint(2, n: n, dejavu: v.mirror.dejavu)
    let p0 = v.mirror.pearls[2].pitch
    v.down(fin(k, p.x, p.y, 0, 0))
    v.move(fin(k, p.x, p.y - 22 * v.geo.S, 0, 0))                                          // вверх — выше
    XCTAssertEqual(acts(rec, .pearl).last?.i, 2); XCTAssertEqual(acts(rec, .pearl).last?.a[0], 0)
    XCTAssertEqual(acts(rec, .pearl).last?.a[1] ?? 0, p0 + 0.2, accuracy: 0.01)
    v.up(fin(k, p.x, p.y - 22 * v.geo.S, 0, 0))
    XCTAssertTrue(acts(rec, .pearl).dropFirst().allSatisfy { $0.a[0] == 0 })                // перетащил — пауза не переключилась
  }

  func testDragOutsideZonesIgnored() {
    let (v, rec) = make(), k = NSObject(), g = v.geo
    let p = g.pt(20, 560)                                                                  // между облаками и нитью
    XCTAssertNil(HymTouch.zone(at: p, geo: g, len: 11, dejavu: 0.7))
    v.down(fin(k, p.x, p.y, 0, 0)); v.move(fin(k, p.x + 80, p.y - 80, 0, 0)); v.up(fin(k, p.x, p.y, 0, 0))
    XCTAssertTrue(rec.acts.isEmpty)
  }

  func testNaNPointIgnored() {
    let (v, rec) = make(), k = NSObject(), cr = v.geo.cloudsRect
    XCTAssertNil(HymTouch.zone(at: CGPoint(x: CGFloat.nan, y: 100), geo: v.geo, len: 11, dejavu: 0.7))
    v.down(fin(k, CGFloat.nan, CGFloat.nan, 0, 0)); v.up(fin(k, CGFloat.nan, CGFloat.nan, 0, 0))
    v.down(fin(k, cr.midX, cr.midY, 0, 0)); v.move(fin(k, CGFloat.nan, 50, 0, 0)); v.move(fin(k, 50, CGFloat.infinity, 0, 0))
    XCTAssertTrue(rec.acts.isEmpty)
    XCTAssertNil(HymTouch.pinch(ring: 0, d0: CGFloat.nan, d: 10, geo: v.geo, base: HymBase()))
  }
}
