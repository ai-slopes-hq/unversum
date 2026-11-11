// Тесты мира Paradisus (VI): модель и мир (звук). Сборка и запуск — core/build.sh → core/build/paradisus-test.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "../host/scale.hpp"
#include "../worlds/paradisus/model.hpp"
#include "../worlds/paradisus/world.hpp"

static int checks = 0, fails = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

using namespace paradisus;

static long gAllocs = 0; static bool gCount = false;      // считает new в аудио-блоке
void* operator new(std::size_t n) { if (gCount) gAllocs++; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

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
                       "{\"angels\":[{\"k\":99,\"v\":-7,\"singers\":50,\"reg\":-3,\"detune\":9}],\"fx\":{\"mix\":-4}}", "{\"rng\":-5,\"angels\":[]}",
                       "{\"angels\":[{\"k\":1e99,\"singers\":1e99,\"reg\":-1e30,\"v\":1e300}]}", "{\"angels\":[{\"k\":-1e30,\"singers\":-1e30,\"reg\":1e99,\"detune\":1e99}]}", "{\"angels\":[{\"k\":null,\"singers\":\"x\",\"reg\":1e999,\"v\":-1e999}]}"};
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

// ---------- мир (звук) ----------

struct Bufs { std::vector<float> L, R, sL, sR; explicit Bufs(int n = 512) : L(n), R(n), sL(n), sR(n) {} sdk::Out out() { return sdk::Out{L.data(), R.data(), sL.data(), sR.data()}; } };
struct Stat { double pk = 0, spk = 0, e = 0; long n = 0; bool finite = true; };
static void eat(Stat& s, const Bufs& b, int n) {
  for (int i = 0; i < n; i++) {
    for (float x : {b.L[i], b.R[i], b.sL[i], b.sR[i]}) if (!std::isfinite(x)) s.finite = false;
    s.pk = std::fmax(s.pk, std::fmax(std::fabs(b.L[i]), std::fabs(b.R[i]))); s.spk = std::fmax(s.spk, std::fmax(std::fabs(b.sL[i]), std::fabs(b.sR[i])));
    s.e += double(b.L[i]) * b.L[i] + double(b.R[i]) * b.R[i]; s.n += 2;
  }
}
static void run(World& w, Bufs& b, double sec, int blk, double sr, Stat& st) {
  const long total = long(sec * sr);
  for (long done = 0; done < total; done += blk) { const int n = int(std::min<long>(blk, total - done)); w.process(b.out(), n); eat(st, b, n); }
}
static void act(World& w, int kind, int i, std::initializer_list<double> a) { std::vector<double> v(a); w.action(kind, i, v.data(), int(v.size())); }
static void pinFour(World& w) {
  const double ks[4] = {0, 2, 5, 8}, vs[4] = {0.3, 1.6, 2.9, 4.2};
  for (int i = 0; i < 4; i++) { act(w, TOUCH, i, {ks[i], vs[i]}); act(w, PIN, i, {}); }
}

static void worldPeakBounded() {
  for (double sr : {48000.0, 44100.0}) {
    Scale sc; World w(sc); w.prepare(sr, 3); Bufs b; Stat st;
    pinFour(w);
    for (int a = 0; a < 4; a++) {                         // у каждого закреплённого — всё на максимум
      const int id = w.model().angel(a).id;
      act(w, ANGEL, id, {0, 1}); act(w, ANGEL, id, {1, 1}); act(w, ANGEL, id, {2, 1}); act(w, ANGEL, id, {3, 1});
    }
    for (int k = 0; k < NFX; k++) act(w, FX, 0, {double(k), 1});
    double pre = 0;
    for (int s = 0; s < 30; s++) {
      if (s % 3 == 0) act(w, ROSE, 0, {});
      run(w, b, 1.0, 128, sr, st);
      pre = std::fmax(pre, w.sound().prePeak());
    }
    CHECK(st.finite); CHECK(st.pk < 1.0 && st.spk < 1.0); CHECK(st.pk > 0.05);
    CHECK(pre < 1.3);                                     // колено не должно быть основным лимитом: до него — не выше ~1.3 (измерено, см. отчёт)
    std::printf("  peak sr=%.0f: out %.3f send %.3f pre-knee %.3f rms %.3f\n", sr, st.pk, st.spk, pre, std::sqrt(st.e / st.n));
  }
}

static void worldBlocksAnySize() {
  Scale sc; World w(sc); w.prepare(48000, 5); Bufs b(512); Stat st;
  pinFour(w); act(w, FX, 0, {double(SHIMMER), 1}); act(w, FX, 0, {double(DECAY), 1});
  const int sizes[] = {1, 2, 3, 7, 31, 64, 127, 128, 129, 255, 300, 512};
  for (int rep = 0; rep < 60; rep++) { for (int n : sizes) { w.process(b.out(), n); eat(st, b, n); } act(w, ROSE, 0, {}); }
  CHECK(st.finite); CHECK(st.pk < 1.0 && st.spk < 1.0); CHECK(st.pk > 0.05);
  w.process(b.out(), 0); w.process(b.out(), -5);          // нулевой и отрицательный блок не ломают
}

static void worldSilentWhenIdle() {
  Scale sc; World w(sc); w.prepare(48000, 1); Bufs b; Stat st;
  run(w, b, 2.0, 128, 48000, st);
  CHECK(std::sqrt(st.e / st.n) < 1e-4); CHECK(st.pk == 0); CHECK(!w.sounding());
  double h[8]; CHECK(w.heard(h, 8) == 0);
}

static double centroid(const std::vector<float>& x) {     // спектральный центроид, 40…6000 Гц, по DFT окна 8192
  const int N = 8192; double num = 0, den = 0;
  for (int k = 1; k < 1000; k++) {
    const double f = k * 48000.0 / N; if (f < 40 || f > 6000) continue;
    double re = 0, im = 0;
    for (int i = 0; i < N; i++) { const double ph = 2 * M_PI * k * i / N, win = 0.5 - 0.5 * std::cos(2 * M_PI * i / N); re += x[i] * win * std::cos(ph); im -= x[i] * win * std::sin(ph); }
    const double m = std::sqrt(re * re + im * im); num += f * m; den += m;
  }
  return num / den;
}
static double vowelCentroid(double v) {
  Scale sc; World w(sc); w.prepare(48000, 2); Bufs b; Stat st;
  act(w, FX, 0, {double(GLORIA), 0}); act(w, FX, 0, {double(SHIMMER), 0}); act(w, FX, 0, {double(MIX), 0});
  act(w, TOUCH, 0, {6, v});                               // сфера 6 → гармоника 3 (330 Гц), все певцы
  run(w, b, 3.0, 128, 48000, st);
  std::vector<float> x; x.reserve(8192);
  while (int(x.size()) < 8192) { w.process(b.out(), 128); for (int i = 0; i < 128; i++) x.push_back(b.L[i]); }
  return centroid(x);
}
static void worldVowelChangesSpectrum() {
  const double a = vowelCentroid(0), i = vowelCentroid(2);
  CHECK(a > 0 && i > 0); CHECK(std::fabs(a - i) / std::fmin(a, i) >= 0.10);
  std::printf("  centroid a=%.0f Hz i=%.0f Hz\n", a, i);
}

static void worldDeterministic() {
  Scale sc; World w1(sc), w2(sc); w1.prepare(48000, 9); w2.prepare(48000, 9); Bufs b1, b2;
  for (World* w : {&w1, &w2}) { act(*w, DICE, 0, {}); act(*w, TOUCH, 7, {3, 2.5}); act(*w, ROSE, 0, {}); act(*w, FX, 0, {double(GLORIA), 0.8}); }
  bool same = true;
  for (int blk = 0; blk < 600; blk++) {
    w1.process(b1.out(), 128); w2.process(b2.out(), 128);
    same = same && !std::memcmp(b1.L.data(), b2.L.data(), 128 * 4) && !std::memcmp(b1.R.data(), b2.R.data(), 128 * 4) && !std::memcmp(b1.sL.data(), b2.sL.data(), 128 * 4);
  }
  CHECK(same);
  World w3(sc); w3.prepare(48000, 10); Bufs b3; act(w3, DICE, 0, {}); w3.process(b3.out(), 128);
  double d = 0; for (int i = 0; i < 128; i++) d += std::fabs(b3.L[i] - b1.L[i]); CHECK(d >= 0);   // другое зерно — просто не падает
}

static void worldSnapshotRoundTrip() {
  Scale sc; World w(sc); w.prepare(48000, 4); Bufs b; Stat st;
  act(w, DICE, 0, {}); act(w, FX, 0, {double(SHIMMER), 0.9}); act(w, FX, 0, {double(CATHEDRAL), 0.2});
  act(w, ANGEL, w.model().angel(0).id, {0, 0.2});
  run(w, b, 0.5, 128, 48000, st);
  const std::string js = w.snapshot();
  World w2(sc); w2.prepare(48000, 77); w2.restore(js);
  CHECK(w2.snapshot() == js); CHECK(w2.model().pinnedCount() == 3); CHECK(w2.model().fx().shimmer == 0.9);
  CHECK(js.find("\"model\"") != std::string::npos);
  Stat s2; run(w2, b, 1.0, 128, 48000, s2); CHECK(s2.finite && s2.pk > 0.01); CHECK(w2.sound().voicesUsed() == 3);
}

static void worldRestoreAnyInput() {
  Scale sc; Bufs b;
  const char* in[] = {"", "{", "null", "[]", "мусор", "{\"model\":5}", "{\"model\":{\"angels\":[{\"k\":99,\"v\":-1e9,\"singers\":-3}],\"fx\":{\"shimmer\":1e99}}}",
                      "{\"angels\":[{\"k\":4,\"v\":1,\"singers\":3,\"detune\":0.4,\"vib\":0.5,\"breath\":0.2,\"reg\":1}],\"fx\":{\"shimmer\":0.5,\"gloria\":0.3,\"aether\":0.7,\"vibrato\":0.4,\"orbits\":0.3}}"};
  for (const char* js : in) {
    World w(sc); w.prepare(48000, 1); act(w, TOUCH, 0, {1, 1});
    w.restore(js);
    Stat st; run(w, b, 1.0, 128, 48000, st);
    CHECK(st.finite && st.pk < 1.0);
    act(w, TOUCH, 5, {4, 2}); run(w, b, 1.0, 128, 48000, st);   // мир после restore рабочий: новый ангел поёт
    CHECK(st.finite && st.pk > 0.01 && w.sounding());
    double m[MIRROR_N]; CHECK(w.mirror(m, MIRROR_N) == MIRROR_N);
  }
  World w(sc); w.prepare(48000, 1); w.restore(in[7]);
  CHECK(w.model().count() == 1 && w.model().angel(0).pinned && w.model().fx().shimmer == 0.5 && w.model().angel(0).singers == 3);
}

static void worldRestoreKeepsSung() {                     // restore с теми же отрицательными id, пока живые голоса ещё есть: ангел должен петь
  Scale sc; World w(sc); w.prepare(48000, 21); Bufs b; Stat st;
  act(w, FX, 0, {double(GLORIA), 0}); act(w, FX, 0, {double(ORBITS), 1});
  act(w, TOUCH, 0, {5, 2.3}); act(w, PIN, 0, {}); run(w, b, 1.5, 128, 48000, st);
  const std::string js = w.snapshot();
  w.restore(js); run(w, b, 1.5, 128, 48000, st);
  CHECK(w.model().count() == 1 && w.model().angel(0).sung && w.model().angel(0).lvl > 0.9);
  CHECK(w.sound().voicesUsed() >= 1);
  int chimes = 0; double h[100];
  for (int s = 0; s < 30; s++) { run(w, b, 1.0, 128, 48000, st); const int n = w.heard(h, 100); for (int j = 0; j < (n ? int(h[0]) : 0); j++) chimes += h[1 + 2 * j] == H_CHIME; }
  CHECK(chimes >= 1);
}

static void worldTouchMovesPinned() {                      // TOUCH с отрицательным id двигает закреплённого, но не рождает ангелов
  Scale sc; World w(sc); w.prepare(48000, 23); Bufs b; Stat st;
  act(w, TOUCH, 3, {2, 1}); act(w, PIN, 3, {}); run(w, b, 0.3, 128, 48000, st);
  const int id = w.model().angel(0).id; CHECK(id < 0);
  act(w, TOUCH, id, {6, 4.2}); run(w, b, 0.3, 128, 48000, st);
  CHECK(w.model().count() == 1 && w.model().angel(0).pinned && w.model().angel(0).k == 6 && std::fabs(w.model().angel(0).v - 4.2) < 1e-9 && w.model().angel(0).id == id);
  act(w, TOUCH, -777, {1, 1}); act(w, TOUCH, 3, {2, 1}); run(w, b, 0.1, 128, 48000, st);   // неизвестный отрицательный — ничего; положительный старый id — новый временный, как раньше
  CHECK(w.model().count() == 2 && w.model().pinnedCount() == 1 && st.finite);
}

static void worldRapidTapsAllSing() {                     // 40 быстрых касаний-отпусканий: пул (24) не должен забиваться уходящими голосами
  Scale sc; World w(sc); w.prepare(48000, 24); Bufs b; Stat st;
  act(w, FX, 0, {double(GLORIA), 0});
  int skipped = 0;
  for (int i = 0; i < 40; i++) {
    act(w, TOUCH, 100 + i, {double(i % 9), i * 0.37});
    run(w, b, 0.06, 128, 48000, st);
    if (w.model().count() != 1 || !w.model().angel(0).sung) skipped++;   // каждое касание получило голос
    act(w, RELEASE, 100 + i, {});
    run(w, b, 0.02, 128, 48000, st);
  }
  CHECK(skipped == 0); CHECK(st.finite); CHECK(st.pk < 1.0);
  Stat tail; run(w, b, 8.0, 128, 48000, tail); CHECK(!w.sounding());
}

static void worldCrowdBounded() {
  Scale sc; World w(sc); w.prepare(48000, 22); Bufs b; Stat st;
  pinFour(w);
  for (int a = 0; a < 4; a++) { const int id = w.model().angel(a).id; for (int k = 0; k < 4; k++) act(w, ANGEL, id, {double(k), 1}); }
  for (int i = 0; i < 12; i++) act(w, TOUCH, 10 + i, {double(i % 9), i * 0.4});
  for (int k = 0; k < NFX; k++) act(w, FX, 0, {double(k), 1});
  double pre = 0;
  for (int s = 0; s < 20; s++) { if (s % 3 == 0) act(w, ROSE, 0, {}); run(w, b, 1.0, 128, 48000, st); pre = std::fmax(pre, w.sound().prePeak()); }
  CHECK(w.model().count() == MAX_ANGEL); CHECK(st.finite); CHECK(st.pk < 1.0 && st.spk < 1.0);
  std::printf("  crowd 4 pins + 12 temp: out %.3f send %.3f pre-knee %.3f\n", st.pk, st.spk, pre);
}

static void worldPinKeepsVoice() {
  Scale sc; World w(sc); w.prepare(48000, 6); Bufs b; Stat st;
  act(w, TOUCH, 4, {3, 1}); run(w, b, 1.0, 128, 48000, st);
  CHECK(w.sound().voicesUsed() == 1);
  act(w, PIN, 4, {}); CHECK(w.model().pinnedCount() == 1);
  act(w, RELEASE, 4, {}); run(w, b, 3.0, 128, 48000, st);
  CHECK(w.sound().voicesUsed() == 1);                     // тот же голос: новый не родился, старый не ушёл в затухание
  CHECK(w.model().angel(0).sung && w.model().angel(0).lvl > 0.99);
  act(w, PIN, w.model().angel(0).id, {}); CHECK(w.model().count() == 0);   // снят — исчез
  run(w, b, 8.0, 128, 48000, st); CHECK(w.sound().voicesUsed() == 0);
}

static void worldReleaseFades() {
  Scale sc; World w(sc); w.prepare(48000, 8); Bufs b; Stat st;
  act(w, FX, 0, {double(GLORIA), 0});
  act(w, TOUCH, 0, {2, 3}); run(w, b, 2.0, 128, 48000, st);
  CHECK(w.sounding() && w.sound().level() > 0.01);
  act(w, RELEASE, 0, {}); CHECK(w.model().count() == 0);
  Stat tail; run(w, b, 10.0, 128, 48000, tail);
  CHECK(!w.sounding()); CHECK(w.sound().level() < 1e-4);
  Stat quiet; run(w, b, 0.5, 128, 48000, quiet); CHECK(quiet.pk == 0);
}

static void worldHeardEvents() {
  Scale sc; World w(sc); w.prepare(48000, 11); Bufs b; Stat st;
  act(w, ROSE, 0, {}); run(w, b, 1.0, 128, 48000, st);
  double h[100]; const int k = w.heard(h, 100);
  CHECK(k == 1 + 2 * 12 && h[0] == 12);
  for (int i = 0; i < 12; i++) CHECK(h[1 + 2 * i] == H_GRAIN && std::fabs(h[2 + 2 * i]) <= 0.8);
  CHECK(w.heard(h, 100) == 0);                            // очищено
  Scale sc2; World c(sc2); c.prepare(48000, 12);
  act(c, FX, 0, {double(ORBITS), 1}); act(c, FX, 0, {double(GLORIA), 0}); act(c, TOUCH, 0, {5, 2.3}); act(c, PIN, 0, {});
  int chimes = 0; double hh[100];
  for (int s = 0; s < 30; s++) { run(c, b, 1.0, 128, 48000, st); const int n = c.heard(hh, 100); for (int j = 0; j < (n ? int(hh[0]) : 0); j++) chimes += hh[1 + 2 * j] == H_CHIME; }
  CHECK(chimes >= 1);
  double none[1]; CHECK(w.heard(none, 0) == 0); CHECK(w.heard(nullptr, 5) == 0);
}

static void worldGloria() {
  Scale sc; World w(sc); w.prepare(48000, 13); Bufs b; Stat st;
  act(w, FX, 0, {double(GLORIA), 1}); act(w, TOUCH, 0, {4, 1});
  run(w, b, 0.5, 128, 48000, st);
  double h[200]; w.heard(h, 200);                         // сброс
  run(w, b, 4.0, 128, 48000, st);
  const int n = w.heard(h, 200); const int cnt = n ? int(h[0]) : 0;
  CHECK(cnt >= 24 && cnt <= 32);                          // ≈ 24 зерна/с на 4 с, но событий между опросами ≤ 32 (лишние теряются)
  act(w, FX, 0, {double(GLORIA), 0}); run(w, b, 1.0, 128, 48000, st); w.heard(h, 200);
  run(w, b, 2.0, 128, 48000, st); CHECK(w.heard(h, 200) == 0);
}

static void worldMirrorLayout() {
  Scale sc; World w(sc); w.prepare(48000, 14); Bufs b; Stat st;
  CHECK(MIRROR_N == 188);
  double m[MIRROR_N + 4]; CHECK(w.mirror(m, MIRROR_N - 1) == 0); CHECK(w.mirror(nullptr, 500) == 0);
  act(w, TOUCH, 3, {4, 2.5}); act(w, ANGEL, 3, {0, 1}); act(w, ANGEL, 3, {4, 1}); act(w, FX, 0, {double(VIBRATO), 0.9});
  run(w, b, 1.5, 128, 48000, st);
  CHECK(w.mirror(m, MIRROR_N + 4) == MIRROR_N);
  CHECK(m[0] == 1 && m[1] == 0 && m[3 + VIBRATO] == 0.9 && m[3 + SHIMMER] == 0.35 && m[11] > 0);
  const double* a = m + MIRROR_HEAD;
  CHECK(a[0] == 3 && a[1] == 4 && a[2] == 2.5 && a[3] == 2.5 && a[4] == 0 && a[5] > 0.5 && a[6] == 5 && a[10] == 2);
  for (int i = MIRROR_HEAD + MIRROR_ANGEL; i < MIRROR_N; i++) CHECK(m[i] == 0);
  act(w, PIN, 3, {}); w.mirror(m, MIRROR_N); CHECK(m[1] == 1 && a[4] == 1 && a[0] < 0);
  act(w, VOWEL, a[0] < 0 ? int(a[0]) : 0, {4.0}); w.mirror(m, MIRROR_N); CHECK(a[2] == 4 && a[1] == 4);
  act(w, CLEAR, 0, {}); w.mirror(m, MIRROR_N); CHECK(m[0] == 0 && m[3 + VIBRATO] == 0.9);
}

static void worldBadActions() {
  Scale sc; World w(sc); w.prepare(48000, 15); Bufs b; Stat st;
  const double nan = NAN, inf = INFINITY;
  w.action(TOUCH, 0, nullptr, 0); w.action(TOUCH, 0, nullptr, 5); act(w, TOUCH, 0, {nan, nan}); act(w, TOUCH, 1, {inf, -inf}); act(w, TOUCH, -3, {1, 1}); act(w, TOUCH, 2, {1e30, 1e30});
  act(w, FX, 0, {nan, 1}); act(w, FX, 0, {2, nan}); act(w, FX, 0, {99, 1}); act(w, ANGEL, 0, {inf, 1}); act(w, VOWEL, 77, {1}); act(w, PIN, 99, {}); act(w, RELEASE, 99, {});
  act(w, sdk::ACT_LFO, -1, {double(TONE), 0.7}); CHECK(w.model().fx().tone == 0.7);
  w.action(-1, 0, nullptr, 0); w.action(1234, 0, nullptr, 0);
  double dt = 0.01; w.action(-1, 0, &dt, 1);
  run(w, b, 1.0, 128, 48000, st); CHECK(st.finite && st.pk < 1.0);
  for (int i = 0; i < 40; i++) act(w, TOUCH, 100 + i, {double(i % 9), double(i)});      // больше ангелов, чем мест: не падает
  run(w, b, 1.0, 128, 48000, st); CHECK(st.finite && st.pk < 1.0 && w.model().count() == MAX_ANGEL);
}

static void worldNoAllocInProcess() {
  Scale sc; World w(sc); w.prepare(48000, 16); Bufs b; Stat st;
  pinFour(w); act(w, TOUCH, 9, {3, 1}); act(w, FX, 0, {double(GLORIA), 1}); run(w, b, 0.5, 128, 48000, st);
  const double t[2] = {1, 1}, an[2] = {0, 1};
  gAllocs = 0; gCount = true;
  w.action(ROSE, 0, nullptr, 0); w.action(TOUCH, 10, t, 2); w.action(RELEASE, 9, nullptr, 0); w.action(ANGEL, 10, an, 2); w.action(DICE, 0, nullptr, 0);
  for (int i = 0; i < 400; i++) w.process(b.out(), 128);
  double h[100]; w.heard(h, 100); double m[MIRROR_N]; w.mirror(m, MIRROR_N);
  gCount = false;
  CHECK(gAllocs == 0);
}

int main() {
  harmMapping(); formantsInterpolate(); pinLimit(); hzOfScaleOff(); snapshotRoundTrip(); restoreGarbage();
  orbitsAndChime(); driftBounded(); diceDeterministic(); miscClamps();
  worldPeakBounded(); worldBlocksAnySize(); worldSilentWhenIdle(); worldVowelChangesSpectrum(); worldDeterministic(); worldSnapshotRoundTrip(); worldRestoreAnyInput();
  worldRestoreKeepsSung(); worldTouchMovesPinned(); worldRapidTapsAllSing(); worldCrowdBounded(); worldPinKeepsVoice(); worldReleaseFades(); worldHeardEvents(); worldGloria(); worldMirrorLayout(); worldBadActions(); worldNoAllocInProcess();
  std::printf("paradisus-test: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
