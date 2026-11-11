// Тесты мира Paradisus (VI): модель. Сборка и запуск — core/build.sh → core/build/paradisus-test.
#include <cmath>
#include <cstdio>
#include <string>

#include "../host/scale.hpp"
#include "../worlds/paradisus/model.hpp"

static int checks = 0, fails = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

using namespace paradisus;

static void harmMapping() {
  Scale sc; Model m(sc);
  CHECK(m.harm(0) == 9); CHECK(m.harm(8) == 1);
  for (int k = 0; k < NSPH; k++) CHECK(m.harm(k) == 9 - k);
  m.touch(0, 8, 0); CHECK(m.hzOf(m.angel(0)) == 110);
  m.touch(0, 0, 0); CHECK(m.hzOf(m.angel(0)) == 990);
}

static void formantsInterpolate() {
  Scale sc; Model m(sc);
  double f[4], bw[4], g[4];
  m.formants(0, f, bw, g);
  CHECK(f[0] == 800 && f[1] == 1150 && f[2] == 2900 && f[3] == 3900);
  CHECK(g[0] == 0 && g[1] == -6 && g[2] == -32 && g[3] == -20);
  CHECK(bw[0] == 80 && bw[3] == 130);
  m.formants(1, f, bw, g); CHECK(f[0] == 350 && f[1] == 2000);
  m.formants(4, f, bw, g); CHECK(f[0] == 325);
  m.formants(0.5, f, bw, g);                            // между a и e: геометрическое среднее частот, линейное — дБ и ширина
  CHECK(std::fabs(f[0] - std::sqrt(800.0 * 350)) < 1e-9 && std::fabs(g[1] - (-13)) < 1e-9 && std::fabs(bw[0] - 70) < 1e-9);
  m.formants(4.5, f, bw, g);                            // u → a по кругу
  CHECK(std::fabs(f[0] - std::sqrt(325.0 * 800)) < 1e-9);
  for (double v = -3; v < 12; v += 0.07) {              // всё конечно, внутри таблицы; мусор не падает
    m.formants(v, f, bw, g);
    for (int q = 0; q < 4; q++) CHECK(std::isfinite(f[q]) && f[q] >= 270 && f[q] <= 3900 && bw[q] >= 50 && bw[q] <= 180);
  }
  m.formants(NAN, f, bw, g); CHECK(f[0] == 800);
}

static void pinLimit() {
  Scale sc; Model m(sc); m.reset(6);
  int ids[MAX_PIN];
  for (int i = 0; i < MAX_PIN; i++) { m.touch(i, i, i); CHECK(m.togglePin(i)); ids[i] = m.pinId(); CHECK(ids[i] < 0); }
  m.touch(10, 8, 1);                                    // пятый: отказ, остаётся поющим, пока держишь
  CHECK(!m.togglePin(10)); CHECK(m.pinnedCount() == MAX_PIN); CHECK(m.find(10) && !m.find(10)->pinned);
  m.release(10); CHECK(m.find(10) == nullptr);
  m.release(ids[0]); CHECK(m.find(ids[0]) != nullptr);  // палец с закреплённого — он остаётся
  CHECK(!m.togglePin(ids[0])); CHECK(m.find(ids[0]) == nullptr); CHECK(m.pinnedCount() == MAX_PIN - 1);
  m.touch(11, 3, 2); CHECK(m.togglePin(11)); CHECK(m.pinnedCount() == MAX_PIN);
  CHECK(!m.togglePin(12345));
}

static void hzOfScaleOff() {
  Scale sc; Model m(sc);                                // лад выключен
  for (int root = 0; root < 12; root++) {
    sc.root = root;
    for (int k = 0; k < NSPH; k++) for (double v = 0; v < 5; v += 0.5) {
      m.touch(0, k, v);
      for (int r = 0; r < 3; r++) {
        m.setAngel(0, REG, (r + 0.5) / 3);
        const double hz = m.hzOf(m.angel(0));
        CHECK(std::isfinite(hz) && hz >= 20 && hz <= 8000);
      }
    }
  }
  sc.root = 3; m.touch(0, 4, 0); m.setAngel(0, REG, 0.9); CHECK(m.hzOf(m.angel(0)) == 110 * 5 * 2);
  sc.on = true; CHECK(std::fabs(m.hzOf(m.angel(0)) - 110 * std::pow(2.0, 3 / 12.0) * 10) < 1e-9);
  for (int root = 0; root < 12; root++) { sc.root = root; for (int k = 0; k < NSPH; k++) { m.touch(0, k, 0); const double hz = m.hzOf(m.angel(0)); CHECK(hz >= 20 && hz <= 8000); } }
}

static void snapshotRoundTrip() {
  Scale sc; Model m(sc); m.reset(7);
  m.dice(); m.setFx(SHIMMER, 0.8); m.setFx(ORBITS, 0.9); m.touch(1, 2, 0.5);   // временный в снимок не идёт
  const int id = m.angel(1).id;
  m.setAngel(id, SINGERS, 0.3); m.setAngel(id, REG, 0.9); m.setAngel(id, BREATH, 0.7);
  const std::string js = m.snapshot();
  Model m2(sc); m2.reset(99); m2.touch(5, 0, 0); m2.togglePin(5);
  m2.restore(js);
  CHECK(m2.snapshot() == js); CHECK(m2.count() == 3); CHECK(m2.pinnedCount() == 3);
  CHECK(m2.angel(1).singers == 2 && m2.angel(1).reg == 2 && m2.angel(1).breath == 0.7);
  CHECK(m2.fx().shimmer == 0.8 && m2.fx().orbits == 0.9);
  CHECK(js.find("\"fx\"") != std::string::npos && js.find("\"cathedral\"") != std::string::npos);
  Model m3(sc); m3.restore(js); CHECK(m3.snapshot() == js);
}

static void restoreGarbage() {
  Scale sc; Model m(sc);
  const char* bad[] = {"", "{", "[]", "null", "не json", "{\"angels\":5,\"fx\":[]}", "{\"angels\":[1,null,{\"k\":1e999}],\"fx\":{\"shimmer\":\"x\"}}",
                       "{\"angels\":[{\"k\":99,\"v\":-7,\"singers\":50,\"reg\":-3,\"detune\":9}],\"fx\":{\"mix\":-4}}", "{\"rng\":-5,\"angels\":[]}"};
  for (const char* js : bad) {
    m.touch(0, 1, 1); m.togglePin(0); m.setFx(MIX, 0.9);
    m.restore(js);
    for (int i = 0; i < m.count(); i++) {
      const Angel& a = m.angel(i);
      CHECK(a.k >= 0 && a.k < NSPH && a.v >= 0 && a.v < 5 && a.singers >= 1 && a.singers <= 5 && a.reg >= 0 && a.reg <= 2 && a.detune >= 0 && a.detune <= 1);
    }
    CHECK(m.fx().mix >= 0 && m.fx().mix <= 1);
  }
  m.restore(""); CHECK(m.count() == 0); CHECK(m.fx().shimmer == 0.35 && m.fx().mix == 0.5 && m.fx().cathedral == 0.5);
  m.restore("{"); CHECK(m.count() == 0 && m.fx().orbits == 0.35);
}

static void orbitsAndChime() {
  Scale sc; Model m(sc); m.reset(2);
  m.setFx(GLORIA, 0); m.touch(0, 5, 2.3); m.togglePin(0); const int id = m.pinId();
  m.setFx(ORBITS, 1);
  const double lap = 70 / (0.3 + 1.7) / m.harm(5);
  Chime c[16]; int n = 0;
  for (double t = 0; t < lap * 3; t += 1.0 / 60) { m.tick(1.0 / 60); n += m.takeChimes(c, 16); }
  CHECK(n == 0);                                        // голос не собран — планета не звенит
  m.setSung(id, true);
  n = 0;
  for (double t = 0; t < lap * 3; t += 1.0 / 60) { m.tick(1.0 / 60); const int g = m.takeChimes(c, 16); for (int i = 0; i < g; i++) CHECK(c[i].id == id && c[i].hz == m.hzOf(*m.find(id))); n += g; }
  CHECK(n == 3);
  CHECK(m.find(id)->lvl > 0.99);
  m.setFx(ORBITS, 0); const double o = m.orbitT(); for (int i = 0; i < 600; i++) m.tick(1.0 / 60); CHECK(m.orbitT() == o);
}

static void driftBounded() {
  Scale sc; Model m(sc); m.reset(12);
  m.setFx(VIBRATO, 1); m.setFx(ORBITS, 0);
  m.touch(0, 4, 2); m.togglePin(0); const Angel* a = m.find(m.pinId());
  m.touch(1, 5, 2);
  double lo = 9, hi = -9;
  for (int i = 0; i < 600; i++) { m.tick(1.0 / 60); const double v = m.vEff(*a); lo = std::fmin(lo, v); hi = std::fmax(hi, v); CHECK(std::fabs(v - 2) <= 0.45 + 1e-9); CHECK(m.vEff(*m.find(1)) == 2); }
  CHECK(hi - lo > 0.1);
}

static void diceDeterministic() {
  Scale sc;
  const int chords[5][3] = {{4, 5, 6}, {3, 4, 5}, {4, 6, 8}, {5, 6, 8}, {2, 3, 5}};
  Model a(sc), b(sc); a.reset(11); b.reset(11); a.dice(); b.dice();
  CHECK(a.snapshot() == b.snapshot());
  for (uint32_t s = 0; s < 20; s++) {
    Model m(sc); m.reset(s); m.dice();
    CHECK(m.count() == 3 && m.pinnedCount() == 3);
    bool hit = false;
    for (auto& ch : chords) if (m.harm(m.angel(0).k) == ch[0] && m.harm(m.angel(1).k) == ch[1] && m.harm(m.angel(2).k) == ch[2]) hit = true;
    CHECK(hit);
    for (int i = 0; i < 3; i++) CHECK(m.angel(i).v >= 0 && m.angel(i).v < 5);
  }
}

static void miscClamps() {
  Scale sc; Model m(sc);
  m.touch(0, 99, 1e9); CHECK(m.angel(0).k == 8 && m.angel(0).v >= 0 && m.angel(0).v < 5);
  m.touch(0, -5, NAN); CHECK(m.angel(0).k == 0 && m.angel(0).v == 0);
  m.setAngel(0, SINGERS, 0); CHECK(m.angel(0).singers == 1); m.setAngel(0, SINGERS, 1); CHECK(m.angel(0).singers == 5);
  m.setAngel(0, REG, 1); CHECK(m.angel(0).reg == 2); m.setAngel(0, DETUNE, NAN); CHECK(m.angel(0).detune == 0.5);
  m.setFx(99, 1); m.setFx(-1, 1); m.setFx(MIX, 7); CHECK(m.fx().mix == 1);
  m.setAngel(77, VIB, 1); m.release(77); m.setSung(77, true);
  CHECK(m.roseHz() == 220); m.setSung(0, true); CHECK(m.roseHz() == m.hzOf(m.angel(0)));
  for (int i = 0; i < MAX_ANGEL + 5; i++) m.touch(100 + i, 1, 1); CHECK(m.count() == MAX_ANGEL);
  m.clear(); CHECK(m.count() == 0 && !m.sounding());
}

int main() {
  harmMapping(); formantsInterpolate(); pinLimit(); hzOfScaleOff(); snapshotRoundTrip(); restoreGarbage();
  orbitsAndChime(); driftBounded(); diceDeterministic(); miscClamps();
  std::printf("paradisus-test: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
