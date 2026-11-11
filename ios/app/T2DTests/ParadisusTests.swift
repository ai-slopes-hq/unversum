import XCTest
@testable import T2D

private final class ParRec: Core {
  var acts: [(kind: Int, i: Int, a: [Double])] = []
  func action(_ kind: Int, _ i: Int, _ a: [Double], touchTime: Double) -> Bool { acts.append((kind, i, a)); return true }
  func kinds() -> [Int] { acts.map(\.kind) }
}

final class ParadisusTests: XCTestCase {
  override class func setUp() {   // шрифты бандла тестов: подписи рисуют IM Fell
    let urls = Bundle(for: ParadisusTests.self).urls(forResourcesWithExtension: "ttf", subdirectory: nil) ?? []
    for u in urls { CTFontManagerRegisterFontsForURL(u as CFURL, .process, nil) }
  }
  private var header: String {
    get throws {
      let url = URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent()
        .appendingPathComponent("core/worlds/paradisus/world.hpp")
      return try String(contentsOf: url, encoding: .utf8)
    }
  }

  func testMirrorLayout() throws {
    let text = try header
    let m = try XCTUnwrap(text.range(of: "MIRROR_N = ")), digits = text[m.upperBound...].prefix { $0.isNumber }
    XCTAssertEqual(Int(digits), Par.mirrorN)
    XCTAssertEqual(Par.mirrorN, 188)
    var b = [Double](repeating: 0, count: Par.mirrorN)
    b[0] = 2; b[1] = 1; b[2] = 0.25
    for i in 0..<8 { b[3 + i] = Double(i) / 10 }
    b[11] = 0.4
    let o = Par.head + Par.per
    b[o...o + 10] = [-3, 4, 2.5, 2.7, 1, 0.8, 3, 0.2, 0.3, 0.4, 2]
    let mir = ParMirror(b)
    XCTAssertEqual(mir.angels.count, 2); XCTAssertEqual(mir.pinnedCount, 1)
    XCTAssertEqual(mir.fx[7], 0.7, accuracy: 1e-9); XCTAssertEqual(mir.level, 0.4, accuracy: 1e-9)
    XCTAssertEqual(mir.angels[1], ParAngel(id: -3, k: 4, v: 2.5, vEff: 2.7, pinned: true, lvl: 0.8, singers: 3, detune: 0.2, vib: 0.3, breath: 0.4, reg: 2))
    XCTAssertEqual(mir.planet(8), 0.25 + 0.13 * 8 - (0.25 + 0.13 * 8).rounded(.down), accuracy: 1e-9)
    var short = mir
    XCTAssertFalse(short.load([Double](repeating: 1, count: 10)))
    XCTAssertTrue(short.angels.isEmpty)
    b[0] = .nan; b[o + 2] = .infinity
    XCTAssertEqual(ParMirror(b).angels.count, 0)
  }

  func testActNumbersMatchCore() throws {
    let text = try header
    let line = try XCTUnwrap(text.split(separator: "\n").first { $0.contains("enum Act {") })
    let names = line.split(separator: "{")[1].split(separator: "}")[0].split(separator: ",").map { $0.trimmingCharacters(in: .whitespaces).lowercased() }
    XCTAssertEqual(names, ["touch", "release", "pin", "vowel", "rose", "fx", "angel", "dice", "clear"])
    for (i, n) in names.enumerated() { XCTAssertEqual(String(describing: ParAct(rawValue: i)!), n) }
    XCTAssertNil(ParAct(rawValue: names.count))
  }

  func testSphereAtNearest() {
    let g = ParGeo(size: CGSize(width: 393, height: 852), safeBot: 34, topY: 60)
    for k in 0..<Par.nSph {
      XCTAssertEqual(g.sphereAt(CGPoint(x: g.cx, y: g.cy - g.sph[k].ry)), k)       // на самом эллипсе
      XCTAssertEqual(g.sphereAt(CGPoint(x: g.cx + g.sph[k].rx, y: g.cy)), k)
    }
    XCTAssertNil(g.sphereAt(CGPoint(x: 0, y: g.bot + 100)))
    XCTAssertTrue(g.inRose(g.center)); XCTAssertFalse(g.inRose(CGPoint(x: g.cx + g.rose + 2, y: g.cy)))
    let p = g.angelPos(3, 1.25)
    XCTAssertEqual(g.vowel(3, p), 1.25, accuracy: 1e-6)
    XCTAssertTrue(g.angelRect(k: 3, v: 1.25).contains(p))
  }

  func testGeoClearOfBottomBar() {
    for (w, h, sb) in [(320.0, 568.0, 0.0), (375, 667, 0), (393, 852, 34)] {
      let g = ParGeo(size: CGSize(width: w, height: h), safeBot: sb, topY: 56)
      let bar = CGRect(x: 0, y: h - sb, width: w, height: sb + 1)          // нижняя панель / корзина начинаются от H - safeBot
      for k in 0..<Par.nSph {
        let r = g.sphereRect(k)
        XCTAssertFalse(r.intersects(bar), "sphere \(k) \(w)x\(h)")
        XCTAssertGreaterThanOrEqual(r.minX, 0); XCTAssertLessThanOrEqual(r.maxX, w); XCTAssertGreaterThanOrEqual(r.minY, 56)
      }
      XCTAssertFalse(CGRect(x: g.cx - g.rose, y: g.cy - g.rose, width: g.rose * 2, height: g.rose * 2).intersects(bar))
      XCTAssertLessThan(g.rose, g.sph[0].rx)
    }
  }

  // ---------- вид и жесты ----------
  private func vec(_ angels: [ParAngel], fx: [Double] = []) -> [Double] {
    var b = [Double](repeating: 0, count: Par.mirrorN)
    b[0] = Double(angels.count); b[1] = Double(angels.filter(\.pinned).count)
    for (i, f) in fx.enumerated() { b[3 + i] = f }
    for (j, a) in angels.enumerated() {
      let o = Par.head + j * Par.per
      b[o...o + 10] = [Double(a.id), Double(a.k), a.v, a.vEff, a.pinned ? 1 : 0, a.lvl, Double(a.singers), a.detune, a.vib, a.breath, Double(a.reg)]
    }
    return b
  }
  private final class Box { var m: [Double]; var heard: [Double] = []; init(_ m: [Double]) { self.m = m } }
  private func make(_ box: Box = Box([])) -> (ParadisusView, ParRec, Box) {
    let rec = ParRec()
    if box.m.isEmpty { box.m = vec([]) }
    let v = ParadisusView(core: rec, readMirror: { b in for i in 0..<min(b.count, box.m.count) { b[i] = box.m[i] }; return box.m.count },
                          readHeard: { b in let h = box.heard; box.heard = []; for i in 0..<min(b.count, h.count) { b[i] = h[i] }; return h.count })
    v.layout(W: 393, H: 852, topY: 103, safeBot: 86)
    v.tick(now: 1, dt: 0.016)
    return (v, rec, box)
  }
  private let fid = NSObject()
  private func finger(_ p: CGPoint, from q: CGPoint? = nil, id: NSObject? = nil) -> Finger {
    let q = q ?? p
    return Finger(id: ObjectIdentifier(id ?? fid), x: p.x, y: p.y, x0: q.x, y0: q.y, t0: 0, moved: false, touchTime: 1)
  }

  func testHitZones() {
    let g = ParGeo(size: CGSize(width: 393, height: 852), safeBot: 86, topY: 103)
    let m = vec([ParAngel(id: -1, k: 4, v: 2, vEff: 2, pinned: true), ParAngel(id: 5, k: 7, v: 4, vEff: 4)])
    XCTAssertEqual(ParTouch.hit(g.center, geo: g, m: m), .rose)
    XCTAssertEqual(ParTouch.hit(g.angelPos(4, 2), geo: g, m: m), .pinned(-1))
    XCTAssertEqual(ParTouch.hit(CGPoint(x: g.angelPos(4, 2).x + 20, y: g.angelPos(4, 2).y - 20), geo: g, m: m), .pinned(-1))   // в пределах зоны
    XCTAssertEqual(ParTouch.hit(g.angelPos(7, 4), geo: g, m: m), .angel(5))
    XCTAssertEqual(ParTouch.hit(g.angelPos(2, 0.5), geo: g, m: m), .sphere(2))
    XCTAssertEqual(ParTouch.hit(g.angelPos(2, 0.5), geo: g, m: []), .sphere(2))                // пустое зеркало — без ангелов, не падает
    XCTAssertEqual(ParTouch.hit(CGPoint(x: 1, y: g.bot + 60), geo: g, m: m), ParHit.none)
    XCTAssertEqual(ParTouch.hit(CGPoint(x: CGFloat.nan, y: 5), geo: g, m: m), ParHit.none)
    if case .touch(let k, let v)? = ParTouch.drag(to: g.angelPos(5, 1), k: 3, geo: g) { XCTAssertEqual(k, 5); XCTAssertEqual(v, 1, accuracy: 1e-6) } else { XCTFail("across") }
    if case .vowel(let v)? = ParTouch.drag(to: g.angelPos(3, 2), k: 3, geo: g) { XCTAssertEqual(v, 2, accuracy: 1e-6) } else { XCTFail("along") }
  }

  func testHoldSendsTouchRelease() {
    let (v, rec, _) = make()
    let p = v.geo.angelPos(3, 1.25)
    v.down(finger(p))
    XCTAssertEqual(rec.acts.count, 1)
    XCTAssertEqual(rec.acts[0].kind, ParAct.touch.rawValue); XCTAssertGreaterThanOrEqual(rec.acts[0].i, 0)
    XCTAssertEqual(rec.acts[0].a[0], 3); XCTAssertEqual(rec.acts[0].a[1], 1.25, accuracy: 1e-6)
    v.move(finger(CGPoint(x: p.x + 2, y: p.y + 1), from: p))                               // дрожь пальца меньше порога — ничего
    XCTAssertEqual(rec.acts.count, 1)
    v.up(finger(p))
    XCTAssertEqual(rec.acts.count, 2)
    XCTAssertEqual(rec.acts[1].kind, ParAct.release.rawValue); XCTAssertEqual(rec.acts[1].i, rec.acts[0].i)
    v.up(finger(p)); XCTAssertEqual(rec.acts.count, 2)                                      // второй подъём — ничего
  }

  func testAlongDragChangesVowel() {
    let (v, rec, _) = make()
    let p = v.geo.angelPos(3, 1)
    v.down(finger(p))
    let q = v.geo.angelPos(3, 2.5)
    v.move(finger(q, from: p))
    XCTAssertEqual(rec.acts.count, 2)
    XCTAssertEqual(rec.acts[1].kind, ParAct.vowel.rawValue); XCTAssertEqual(rec.acts[1].i, rec.acts[0].i)
    XCTAssertEqual(rec.acts[1].a[0], 2.5, accuracy: 1e-6)
    XCTAssertEqual(v.activeLabels.count, 1)                                                 // подпись есть, пока палец на ангеле
    v.up(finger(q)); XCTAssertTrue(v.activeLabels.isEmpty)
  }

  func testAcrossDragChangesSphere() {
    let (v, rec, _) = make()
    let p = v.geo.angelPos(3, 1)
    v.down(finger(p))
    let q = v.geo.angelPos(5, 1)
    v.move(finger(q, from: p))
    XCTAssertEqual(rec.acts[1].kind, ParAct.touch.rawValue); XCTAssertEqual(rec.acts[1].i, rec.acts[0].i)
    XCTAssertEqual(rec.acts[1].a[0], 5); XCTAssertEqual(rec.acts[1].a[1], 1, accuracy: 1e-6)
    let r = v.geo.angelPos(5, 3)                                                            // дальше вдоль новой сферы — гласная, сфера сохранена
    v.move(finger(r, from: p))
    XCTAssertEqual(rec.acts[2].kind, ParAct.vowel.rawValue); XCTAssertEqual(rec.acts[2].a[0], 3, accuracy: 1e-6)
  }

  // закреплённый: тянут вдоль (VOWEL) и поперёк (TOUCH с его отрицательным id); RELEASE при подъёме не шлём; под плашкой подписи нет
  func testPinnedDragAcrossAndAlong() {
    let (v, rec, _) = make(Box(vec([ParAngel(id: -3, k: 4, v: 2, vEff: 2, pinned: true)])))
    let p = v.geo.angelPos(4, 2)
    v.down(finger(p)); XCTAssertTrue(rec.acts.isEmpty); XCTAssertTrue(v.activeLabels.isEmpty)
    v.move(finger(v.geo.angelPos(4, 3), from: p))
    XCTAssertEqual(rec.acts[0].kind, ParAct.vowel.rawValue); XCTAssertEqual(rec.acts[0].i, -3)
    v.move(finger(v.geo.angelPos(6, 3), from: p))
    XCTAssertEqual(rec.acts[1].kind, ParAct.touch.rawValue); XCTAssertEqual(rec.acts[1].i, -3); XCTAssertEqual(rec.acts[1].a[0], 6)
    XCTAssertEqual(v.activeLabels.count, 1)
    v.up(finger(p)); XCTAssertEqual(rec.acts.count, 2)
    // удержание открывает плашку — подпись прячется
    let (w, _, _) = make(Box(vec([ParAngel(id: -3, k: 4, v: 2, vEff: 2, pinned: true)])))
    w.openSheet = { _ in }
    w.down(finger(p)); w.tick(now: 1.6, dt: 0.1)
    XCTAssertTrue(w.activeLabels.isEmpty)
  }

  func testDoubleTapPins() {
    let (v, rec, _) = make()
    let p = v.geo.angelPos(4, 2)
    v.down(finger(p)); v.up(finger(p))
    v.tick(now: 1.1, dt: 0.1)
    v.down(finger(CGPoint(x: p.x + 5, y: p.y)))
    let k = rec.kinds()
    XCTAssertEqual(k, [ParAct.touch, .release, .touch, .pin].map(\.rawValue))
    XCTAssertEqual(rec.acts[3].i, rec.acts[2].i)
    v.up(finger(p))
    XCTAssertEqual(rec.acts.count, 4)                                                       // id пальца после закрепления мёртв: ни RELEASE, ни TOUCH
    v.move(finger(CGPoint(x: p.x + 30, y: p.y), from: p)); XCTAssertEqual(rec.acts.count, 4)
    // двойной тап по закреплённому — PIN (снять)
    let box = Box(vec([ParAngel(id: -1, k: 4, v: 2, vEff: 2, pinned: true)]))
    let (w, rw, _) = make(box)
    let pp = w.geo.angelPos(4, 2)
    w.down(finger(pp)); w.up(finger(pp)); w.tick(now: 1.1, dt: 0.1); w.down(finger(pp))
    XCTAssertEqual(rw.kinds(), [ParAct.pin.rawValue]); XCTAssertEqual(rw.acts[0].i, -1)
    w.up(finger(pp)); XCTAssertEqual(rw.acts.count, 1)                                      // RELEASE закреплённому не шлём
  }

  func testFifthPinRejectedShowsNoCrash() {
    let four = (1...4).map { ParAngel(id: -$0, k: $0, v: Double($0) * 0.7, vEff: Double($0) * 0.7, pinned: true) }
    let box = Box(vec(four))
    let (v, rec, _) = make(box)
    let p = v.geo.angelPos(7, 3)
    v.down(finger(p)); v.up(finger(p)); v.tick(now: 1.1, dt: 0.1); v.down(finger(p)); v.up(finger(p))
    XCTAssertEqual(rec.kinds(), [ParAct.touch, .release, .touch, .pin].map(\.rawValue))   // PIN отправлен, пока вид не знает ответа
    let id = rec.acts[3].i
    box.m = vec(four + [ParAngel(id: id, k: 7, v: 3, vEff: 3)])                              // ядро отказало: ангел так и остался временным
    v.tick(now: 1.2, dt: 0.1); XCTAssertEqual(rec.acts.count, 4)                             // ответ ещё мог не дойти
    v.tick(now: 1.6, dt: 0.4)
    XCTAssertEqual(rec.acts.count, 5); XCTAssertEqual(rec.acts[4].kind, ParAct.release.rawValue); XCTAssertEqual(rec.acts[4].i, id)
    v.tick(now: 2, dt: 0.4); XCTAssertEqual(rec.acts.count, 5)                               // ровно один раз
    XCTAssertEqual(v.mirror.pinnedCount, 4)
    // удавшееся закрепление: id в зеркале исчез — ничего лишнего
    let box2 = Box(vec([]))
    let (w, rw, _) = make(box2)
    w.down(finger(p)); w.up(finger(p)); w.tick(now: 1.1, dt: 0.1); w.down(finger(p)); w.up(finger(p))
    box2.m = vec([ParAngel(id: -1, k: 7, v: 3, vEff: 3, pinned: true)])
    w.tick(now: 1.6, dt: 0.5); XCTAssertEqual(rw.acts.count, 4)
  }

  func testRoseTapSendsRose() {
    let (v, rec, _) = make()
    v.down(finger(v.geo.center)); v.up(finger(v.geo.center))
    XCTAssertEqual(rec.kinds(), [ParAct.rose.rawValue])
    XCTAssertTrue(v.activeLabels.isEmpty)
  }

  func testInterruptedGestureReleases() {
    let (v, rec, _) = make()
    let p = v.geo.angelPos(6, 0.5)
    v.down(finger(p)); v.cancel(finger(p))
    XCTAssertEqual(rec.kinds(), [ParAct.touch.rawValue, ParAct.release.rawValue]); XCTAssertEqual(rec.acts[1].i, rec.acts[0].i)
    v.up(finger(p)); XCTAssertEqual(rec.acts.count, 2)                                      // палец уже снят
    // отмена закреплённого — ничего (RELEASE ему не нужен), отмена по розе — тоже
    let (w, rw, _) = make(Box(vec([ParAngel(id: -1, k: 4, v: 2, vEff: 2, pinned: true)])))
    w.down(finger(w.geo.angelPos(4, 2))); w.cancel(finger(w.geo.angelPos(4, 2)))
    XCTAssertTrue(rw.acts.isEmpty)
  }

  func testSheetsAndHeard() {
    let a = ParAngel(id: -2, k: 5, v: 1, vEff: 1, pinned: true, lvl: 1, singers: 3, detune: 0.2, vib: 0.3, breath: 0.4, reg: 2)
    let box = Box(vec([a], fx: [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.0]))
    let (v, rec, _) = make(box)
    let fx = v.fx!
    XCTAssertEqual(fx.rows.map(\.key), Par.fxKeys)
    XCTAssertEqual(fx.get("tone"), 0.3, accuracy: 1e-9); XCTAssertEqual(fx.text("orbits"), "off")
    fx.set("cathedral", 0.8)
    XCTAssertEqual(rec.acts.last?.kind, ParAct.fx.rawValue); XCTAssertEqual(rec.acts.last?.a, [4, 0.8])
    XCTAssertFalse(fx.lfo!.keys.contains("cathedral"))
    let s = v.angelSpec(-2)
    XCTAssertEqual(s.rows.count, 5); XCTAssertEqual(s.text("singers"), "3 voices"); XCTAssertEqual(s.text("reg"), "octave up")
    s.set("breath", 0.25)
    XCTAssertEqual(rec.acts.last?.kind, ParAct.angel.rawValue); XCTAssertEqual(rec.acts.last?.i, -2); XCTAssertEqual(rec.acts.last?.a, [3, 0.25])
    // heard: звон над ангелом (id) и зерно
    box.heard = [2, 0, -2, 1, 0.4]
    v.tick(now: 2, dt: 0.016)
    XCTAssertEqual(v.dr.flashes.count, 1); XCTAssertEqual(v.dr.glitter.count, 1)
    // удержание закреплённого, не двигая, открывает плашку
    var opened: SheetSpec?
    v.openSheet = { opened = $0 }
    v.down(finger(v.geo.angelPos(5, 1)))
    v.tick(now: 2.6, dt: 0.1)
    XCTAssertEqual(opened?.obj, -2)
    // ангел снят — хост должен закрыть плашку
    var gone: [Int] = []
    v.objGone = { gone.append($0) }
    box.m = vec([]); v.tick(now: 2.7, dt: 0.1)
    XCTAssertEqual(gone, [-2])
  }

  // кадр рисуется без падений в обеих темах и с ангелами, планетами и подписью
  func testDrawSmoke() {
    let a = ParAngel(id: -1, k: 3, v: 2.5, vEff: 2.7, pinned: true, lvl: 0.8)
    let (v, _, _) = make(Box(vec([a], fx: [0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5])))
    let g = CGContext(data: nil, width: 393, height: 852, bitsPerComponent: 8, bytesPerRow: 0, space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue)!
    let d = Draw(g: g, W: 393, H: 852); d.setTheme(v.theme)
    v.down(finger(v.geo.angelPos(3, 2.5)))
    v.tick(now: 1.2, dt: 0.1)
    v.draw(d, now: 1.2); v.drawShot(d, now: 1.2)
    XCTAssertEqual(v.wallBuilds, 1)                                                          // стена строится раз
    v.layout(W: 393, H: 852, topY: 103, safeBot: 86); v.draw(d, now: 1.3)
    XCTAssertEqual(v.wallBuilds, 1)
  }
}
