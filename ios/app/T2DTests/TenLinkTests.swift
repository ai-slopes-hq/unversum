import XCTest
@testable import T2D

private final class TLRec: Core {
  var acts: [(kind: Int, i: Int, a: [Double], t: Double)] = []
  func action(_ kind: Int, _ i: Int, _ a: [Double], touchTime: Double) -> Bool { acts.append((kind, i, a, touchTime)); return true }
}

// мост двух пальцев и плашка связи Tenebrae (app.js:325-349, 885-982)
final class TenLinkTests: XCTestCase {
  private let rec = TLRec()
  private var mbuf: [Double] = []
  private lazy var view = TenebraeView(core: rec, readMirror: { [unowned self] b in
    let n = min(self.mbuf.count, b.count); b.replaceSubrange(0..<n, with: self.mbuf[0..<n]); return n
  }, readHeard: { _ in 0 }, readLevel: { 0 }, rng: { 0.5 })
  private let a = NSObject(), b = NSObject(), c = NSObject()

  override func setUp() {
    mbuf = mirror()
    view.layout(W: 390, H: 844, topY: 103, safeBot: 34)
    view.tick(now: 0.5, dt: 0.016)
    rec.acts.removeAll()
  }

  private func mirror(_ edit: (inout [Double]) -> Void = { _ in }) -> [Double] {
    var m = [Double](repeating: 0, count: Ten.mirrorN)
    m[0] = 6; m[3] = 0.3; m[4] = 3.5; m[5] = 0.3; m[6] = 0; m[7] = 0.2
    for i in 0..<6 { for k in 0..<8 { m[Ten.head + Ten.voiceN * i + 38 + 3 * k + 2] = -1 } }
    edit(&m)
    return m
  }
  private func put(_ m: inout [Double], _ i: Int, held: Bool = false, x: Double = 0.4, y: Double = 0.5, links: [(src: Double, type: Double, depth: Double, auto: Double)] = []) {
    let o = Ten.head + Ten.voiceN * i
    m[o] = 1; m[o + 1] = 1; m[o + 2] = 0; m[o + 3] = held ? 1 : 0
    m[o + 4] = x; m[o + 5] = y; m[o + 6] = 0.35; m[o + 15] = 1; m[o + 16] = 1
    m[o + 17] = Double(links.count)
    for (k, l) in links.enumerated() { let q = o + 18 + 4 * k; m[q] = l.src; m[q + 1] = l.type; m[q + 2] = l.depth; m[q + 3] = l.auto }
  }
  private func load(_ e: @escaping (inout [Double]) -> Void) { mbuf = mirror(e) }
  // планеты 0 (x .25, y .3) и 3 (x .6, y .5), связь 3 → 0 (tzfm, 0.5); 5 — в стороне
  private func linked(type: Double = 1, auto: Double = 0) {
    load { self.put(&$0, 0, x: 0.25, y: 0.3, links: [(3, type, 0.5, auto)]); self.put(&$0, 3, x: 0.6, y: 0.5); self.put(&$0, 5, x: 0.9, y: 0.8) }
  }
  private func tick(_ t: Double) { view.tick(now: t, dt: 0.016) }
  private func fing(_ id: NSObject, _ x: Double, _ y: Double, t0: Double, tt: Double = 0) -> Finger {
    Finger(id: ObjectIdentifier(id), x: x, y: y, x0: x, y0: y, t0: t0, moved: false, touchTime: tt == 0 ? t0 : tt)
  }
  private var log: [(kind: Int, i: Int, a: [Double], t: Double)] { rec.acts.filter { $0.kind != Act.sun } }
  private func has(_ kind: Int) -> Bool { rec.acts.contains { $0.kind == kind } }
  private func pos(_ i: Int) -> (x: Double, y: Double) { view.screenPos(i) }
  private func dir(_ deg: Double, _ d: Double, from p: (x: Double, y: Double)) -> (x: Double, y: Double) {
    (p.x + d * cos(deg * .pi / 180), p.y + d * sin(deg * .pi / 180))
  }
  private var mid: (x: Double, y: Double) { view.linkMid(dst: 0, src: 3) }

  // A держит палец a, второй палец b на планете 3 — мост
  private func bridgeUp() -> (pa: (x: Double, y: Double), pb: (x: Double, y: Double)) {
    load { self.put(&$0, 0, x: 0.25, y: 0.3); self.put(&$0, 3, x: 0.6, y: 0.5); self.put(&$0, 5, x: 0.9, y: 0.8) }
    tick(1)
    let pa = pos(0), pb = pos(3)
    view.down(fing(a, pa.x, pa.y, t0: 1))
    rec.acts.removeAll()
    view.down(fing(b, pb.x, pb.y, t0: 1.1))
    return (pa, pb)
  }
  private func openRing() -> (x: Double, y: Double) {      // палец c на ручке; точка касания — возвращается
    linked()
    tick(1)
    let m = mid
    rec.acts.removeAll()
    view.down(fing(c, m.x, m.y, t0: 1))
    return m
  }

  func testBridgeDrawnPredicate() {
    // мост без второго пальца кадр не рисует — значит, и планеты моста кадр не должен пропускать
    _ = bridgeUp()
    XCTAssertNotNil(view.bridgeDrawn)
    view.fingers[ObjectIdentifier(b)] = nil
    XCTAssertNotNil(view.bridge)
    XCTAssertNil(view.bridgeDrawn)
  }

  func testBridge() {
    let (pa, pb) = bridgeUp()
    XCTAssertEqual(log.map(\.kind), [Act.attack, Act.grab, Act.link])
    XCTAssertEqual(log.map(\.i), [3, 3, 0]); XCTAssertEqual(log[2].a, [3])
    XCTAssertNotNil(view.bridge)
    rec.acts.removeAll()
    view.move(fing(b, pa.x + (pb.x - pa.x) * 1.3, pa.y + (pb.y - pa.y) * 1.3, t0: 1.1))
    XCTAssertEqual(log.map(\.kind), [Act.linkDepth]); XCTAssertEqual(log[0].i, 0); XCTAssertEqual(log[0].a[0], 3)
    XCTAssertEqual(log[0].a[1], 0.5 + 1.3 - 1, accuracy: 1e-9)
    rec.acts.removeAll()
    view.move(fing(b, pa.x + (pb.x - pa.x) * 2, pa.y + (pb.y - pa.y) * 2, t0: 1.1))
    XCTAssertEqual(log[0].a[1], 1, "0.5 + 2 − 1 = 1.5 → 1")
    rec.acts.removeAll()
    view.move(fing(b, pa.x + (pb.x - pa.x) * 0.54, pa.y + (pb.y - pa.y) * 0.54, t0: 1.1))
    XCTAssertEqual(log[0].a[1], 0.04, accuracy: 1e-9)
    rec.acts.removeAll()
    view.up(fing(b, pa.x, pa.y, t0: 1.1))
    XCTAssertEqual(log.map(\.kind), [Act.unlink, Act.letGo]); XCTAssertEqual(log[0].i, 0); XCTAssertEqual(log[0].a, [3])
    XCTAssertEqual(log[1].i, 3); XCTAssertNil(view.bridge)
    rec.acts.removeAll()
    view.move(fing(a, pa.x + 20, pa.y, t0: 1))
    XCTAssertEqual(log.map(\.kind), [Act.move], "оставшийся палец ведёт A дальше"); XCTAssertEqual(log[0].i, 0)
    let q = view.geo.unplace(pa.x + 20, pa.y, zoom: view.zoom)
    XCTAssertEqual(log[0].a[0], q.p, accuracy: 1e-6, "без прыжка: смещение пересчитано")
  }

  func testBridgeKeepsThickLink() {
    let (pa, pb) = bridgeUp()
    rec.acts.removeAll()
    view.move(fing(b, pa.x + (pb.x - pa.x) * 1.1, pa.y + (pb.y - pa.y) * 1.1, t0: 1.1))
    XCTAssertEqual(log[0].a[1], 0.6, accuracy: 1e-9)
    rec.acts.removeAll()
    view.up(fing(b, pa.x, pa.y, t0: 1.1))
    XCTAssertFalse(has(Act.unlink)); XCTAssertEqual(log.map(\.kind), [Act.letGo])
  }

  func testBridgeNotWhenTwoHeld() {
    _ = bridgeUp()
    rec.acts.removeAll()
    let p5 = pos(5)
    view.down(fing(c, p5.x, p5.y, t0: 1.2))
    XCTAssertEqual(log.map(\.kind), [Act.attack, Act.grab], "третий палец — не мост"); XCTAssertEqual(log[0].i, 5)
    XCTAssertFalse(has(Act.link))
  }

  func testBridgeHuge() {
    let (pa, pb) = bridgeUp()
    rec.acts.removeAll()
    view.move(fing(b, 1e300, -1e300, t0: 1.1))
    XCTAssertEqual(log[0].a[1], 1)
    view.move(fing(b, .nan, 5, t0: 1.1)); view.move(fing(b, .infinity, 5, t0: 1.1))
    XCTAssertEqual(log.count, 1, "не конечное — событие брошено")
    view.up(fing(b, pb.x, pb.y, t0: 1.1)); view.up(fing(a, pa.x, pa.y, t0: 1))
  }

  private var plate: TenLinkGeo { view.linkGeo }
  private func tap(_ x: Double, _ y: Double, t0: Double = 2) {
    view.down(fing(a, x, y, t0: t0)); view.up(fing(a, x, y, t0: t0))
  }

  func testHandleOpensPlate() {
    _ = openRing()
    XCTAssertEqual(log.map(\.kind), [Act.link]); XCTAssertEqual(log[0].i, 0); XCTAssertEqual(log[0].a, [3])
    let r = try! XCTUnwrap(view.ring)
    XCTAssertEqual(r.dst, 0); XCTAssertEqual(r.src, 3); XCTAssertNil(r.finger, "ручка — тап, палец плашку не ведёт")
  }

  func testAutoLinkHasNoHandle() {
    linked(auto: 1)
    tick(1)
    let m = mid
    rec.acts.removeAll()
    view.down(fing(c, m.x, m.y, t0: 1))
    XCTAssertNil(view.ring, "хаос-связь (auto) ручки не имеет")
  }

  func testPlateType() {
    _ = openRing(); rec.acts.removeAll()
    let c2 = plate.chips[2]                                   // am
    tap(c2.midX, c2.midY)
    XCTAssertEqual(log.map(\.kind), [Act.linkType]); XCTAssertEqual(log[0].i, 0); XCTAssertEqual(log[0].a, [3, 2])
    XCTAssertNotNil(view.ring, "плашка остаётся")
    rec.acts.removeAll()
    tap(c2.midX, c2.midY, t0: 3)                              // am уже выбран (своё значение сразу)
    XCTAssertTrue(log.isEmpty)
  }

  func testPlateDepth() {
    _ = openRing(); rec.acts.removeAll()
    let g = plate, x = g.tx0 + 0.25 * (g.tx1 - g.tx0)
    view.down(fing(a, x, g.sliderY, t0: 2))
    XCTAssertEqual(log.map(\.kind), [Act.linkDepth]); XCTAssertEqual(log[0].a[0], 3); XCTAssertEqual(log[0].a[1], 0.25, accuracy: 1e-9)
    rec.acts.removeAll()
    view.move(fing(a, g.tx1 + 500, g.sliderY - 60, t0: 2))
    XCTAssertEqual(log[0].a[1], 1, "за краем — предел, палец ведёт и выше полосы")
    view.up(fing(a, g.tx1, g.sliderY, t0: 2))
    rec.acts.removeAll()
    view.move(fing(a, g.tx0, g.sliderY, t0: 2))
    XCTAssertTrue(log.isEmpty, "палец поднят — ползунок не ведёт")
    XCTAssertNotNil(view.ring)
  }

  func testPlateCut() {
    _ = openRing(); rec.acts.removeAll()
    let c4 = plate.chips[4]
    tap(c4.midX, c4.midY)
    XCTAssertEqual(log.map(\.kind), [Act.unlink]); XCTAssertEqual(log[0].i, 0); XCTAssertEqual(log[0].a, [3])
    XCTAssertNil(view.ring)
  }

  func testPlateClose() {
    _ = openRing(); rec.acts.removeAll()
    let c = plate.close
    tap(c.midX, c.midY)
    XCTAssertNil(view.ring); XCTAssertTrue(log.isEmpty)
    _ = openRing(); rec.acts.removeAll()
    view.down(fing(a, 195, plate.top - 30, t0: 3))            // мимо плашки — закрыть, в поле не идёт
    XCTAssertNil(view.ring); XCTAssertTrue(log.isEmpty); XCTAssertTrue(view.fingers.isEmpty)
  }

  func testPlateHuge() {
    _ = openRing()
    let g = plate
    view.down(fing(a, g.tx0, g.sliderY, t0: 2))
    rec.acts.removeAll()
    view.move(fing(a, 1e300, 1e300, t0: 2))
    XCTAssertEqual(log.map(\.kind), [Act.linkDepth]); XCTAssertEqual(log[0].a[1], 1)
    view.move(fing(a, .nan, 0, t0: 2))
    XCTAssertEqual(log.count, 1, "NaN — событие брошено")
    view.up(fing(a, 1e300, 1e300, t0: 2))
  }

  func testHugeMirrorLinkType() {
    load { self.put(&$0, 0, x: 0.25, y: 0.3, links: [(3, 1e300, 0.5, 0)]); self.put(&$0, 3, x: 0.6, y: 0.5) }
    tick(1)
    XCTAssertTrue(view.voice(0).mods.isEmpty, "тип 1e300 — связь отброшена без падения")
    let p = pos(0)
    view.down(fing(c, p.x, p.y, t0: 1))
    XCTAssertNil(view.ring)
  }

  func testPlateClosesWhenLinkGone() {
    _ = openRing()
    XCTAssertNotNil(view.ring)
    load { self.put(&$0, 0, x: 0.25, y: 0.3); self.put(&$0, 3, x: 0.6, y: 0.5) }   // связи нет
    tick(2)
    XCTAssertNil(view.ring)
  }

  func testBridgeEndsWhenVoiceGone() {
    let (pa, _) = bridgeUp()
    load { self.put(&$0, 0, x: 0.25, y: 0.3) }               // B пропала
    tick(2)
    XCTAssertNil(view.bridge)
    rec.acts.removeAll()
    view.move(fing(b, 5, 5, t0: 1.1)); view.up(fing(b, 5, 5, t0: 1.1))
    XCTAssertFalse(has(Act.linkDepth)); XCTAssertFalse(has(Act.unlink))
    view.up(fing(a, pa.x, pa.y, t0: 1))
  }

  func testRestoredMidPlateAndBridge() {
    _ = openRing()
    let g = plate
    view.down(fing(a, g.tx0 + 40, g.sliderY, t0: 2))
    rec.acts.removeAll()
    view.restored()
    XCTAssertNil(view.ring)
    view.move(fing(a, g.tx0 + 80, g.sliderY, t0: 2)); view.up(fing(a, g.tx0 + 80, g.sliderY, t0: 2))
    XCTAssertTrue(rec.acts.isEmpty)
    let (pa, pb) = bridgeUp()
    rec.acts.removeAll()
    view.restored()
    XCTAssertNil(view.bridge)
    view.move(fing(b, pb.x + 30, pb.y, t0: 1.1)); view.up(fing(b, pb.x, pb.y, t0: 1.1)); view.up(fing(a, pa.x, pa.y, t0: 1))
    XCTAssertTrue(rec.acts.isEmpty)
  }

  func testLinkCurve() {
    linked(); tick(1)
    let c = view.linkCurve(dst: 0, src: 3), acc = 1e-6
    XCTAssertEqual(c.x0, 207.06251178146383, accuracy: acc); XCTAssertEqual(c.y0, 303.5156772388611, accuracy: acc)
    XCTAssertEqual(c.cx, 200.28071192018612, accuracy: acc); XCTAssertEqual(c.cy, 486.64568116971293, accuracy: acc)
    XCTAssertEqual(c.x1, 287.0143622391961, accuracy: acc); XCTAssertEqual(c.y1, 648.0763630471428, accuracy: acc)
    let m = view.linkMid(dst: 0, src: 3), b = tenBez(c, 0.5)
    XCTAssertEqual(m.x, b.x, accuracy: 1e-9); XCTAssertEqual(m.y, b.y, accuracy: 1e-9)
    XCTAssertEqual(b.x, 223.65957446525803, accuracy: acc); XCTAssertEqual(b.y, 481.22085065635747, accuracy: acc)
    let t = tenBez(c, 0.3)
    XCTAssertEqual(t.x, 211.40982238092306, accuracy: acc); XCTAssertEqual(t.y, 411.4407406125642, accuracy: acc)
    let chord = (x: 247.03843701032997, y: 475.7960201430019)       // середина отрезка; контрольная точка — ближе к солнцу (0, 806)
    XCTAssertLessThan(hypot(c.cx, c.cy - 806), hypot(chord.x, chord.y - 806))
    let t0 = tenBez(c, 0), t1 = tenBez(c, 1)
    XCTAssertEqual(t0.x, c.x0, accuracy: 1e-9); XCTAssertEqual(t1.y, c.y1, accuracy: 1e-9)
  }

  func testLinkCurveHuge() {
    load { self.put(&$0, 0, x: 0.25, y: 0.3, links: [(3, 1, 1e300, 0)]); self.put(&$0, 3, x: 0.6, y: 0.5) }
    tick(1)
    let c = view.linkCurve(dst: 0, src: 3)
    XCTAssertTrue(c.cx.isFinite && c.cy.isFinite)
  }
}
