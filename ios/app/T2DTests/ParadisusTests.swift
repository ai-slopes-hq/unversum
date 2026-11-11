import XCTest
@testable import T2D

final class ParadisusTests: XCTestCase {
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
}
