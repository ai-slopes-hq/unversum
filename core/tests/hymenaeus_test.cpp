// Тесты мира Hymenaeus (XIV): модель, кольца, облака, мир. Сборка и запуск — core/build.sh → core/build/hymenaeus-test.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "../host/scale.hpp"
#include "../worlds/hymenaeus/cloud.hpp"
#include "../worlds/hymenaeus/model.hpp"
#include "../worlds/hymenaeus/world.hpp"
#include "../worlds/hymenaeus/rings/plucker.hpp"
#include "../worlds/hymenaeus/rings/resonator.hpp"
#include "../worlds/hymenaeus/sound.hpp"

static int checks = 0, fails = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

using namespace hymenaeus;

// ---------- Task 1: модель ----------

static void modelSeedDeterministic() {
  Scale sc; Model a(sc), b(sc); a.reset(5); b.reset(5);
  for (int i = 0; i < 40; i++) {
    const Step x = a.next(), y = b.next();
    CHECK(x.hit == y.hit && x.ring == y.ring && x.hz == y.hz && x.pearl == y.pearl);
  }
}

static void modelDejaVuOneRepeats() {
  Scale sc; Model m(sc); m.reset(3); m.setDejaVu(1);
  const int len = m.len();
  std::vector<double> first;
  for (int i = 0; i < len; i++) first.push_back(m.next().hz);
  for (int round = 0; round < 3; round++)
    for (int i = 0; i < len; i++) CHECK(m.next().hz == first[i]);
}

static void modelDejaVuZeroVaries() {
  Scale sc; Model m(sc); m.reset(3); m.setDejaVu(0);
  const int len = m.len();
  std::vector<std::vector<double>> rounds;
  for (int r = 0; r < 8; r++) { std::vector<double> v; for (int i = 0; i < len; i++) v.push_back(m.next().hz); rounds.push_back(v); }
  int different = 0;
  for (int r = 1; r < 8; r++) if (rounds[r] != rounds[0]) different++;
  CHECK(different >= 6);
}

static void modelRestsSilent() {
  Scale sc; Model m(sc); m.reset(2); m.setDejaVu(1);
  for (int i = 0; i < PEARL_MAX; i++) m.setRest(i, true);
  CHECK(!m.sounding());
  for (int i = 0; i < m.len() * 3; i++) CHECK(!m.next().hit);
  m.togglePearl(0); CHECK(m.pearl(0).rest == false); CHECK(m.sounding());
  m.togglePearl(0); CHECK(m.pearl(0).rest == true);
}

static void modelRingBands() {
  Scale sc; Model m(sc);
  CHECK(m.ringFor(0.1) == 0); CHECK(m.ringFor(0.5) == 1); CHECK(m.ringFor(0.9) == 2);
  CHECK(m.ringFor(0) == 0); CHECK(m.ringFor(1) == 2);
  CHECK(m.ringFor(NAN) == 1);
}

static void modelPitchToRing() {
  Scale sc; Model m(sc); m.reset(1); m.setDejaVu(1); m.setLen(3);
  m.setPearlPitch(0, 0.1); m.setPearlPitch(1, 0.5); m.setPearlPitch(2, 0.9);
  for (int i = 0; i < 3; i++) m.setRest(i, false);
  const Step a = m.next(), b = m.next(), c = m.next();
  CHECK(a.ring == 0 && b.ring == 1 && c.ring == 2);
  CHECK(a.hz < b.hz && b.hz < c.hz);
  CHECK(a.pearl == 0 && c.pearl == 2);
}

static void modelScaleSnap() {
  Scale sc; sc.on = true; sc.idx = 0; sc.root = 0;          // мажор от A
  Model m(sc); m.reset(7); m.setDejaVu(1);
  for (int i = 0; i < PEARL_MAX; i++) { m.setRest(i, false); m.setPearlPitch(i, i / 15.0); }
  const int* st = Scale::stepsOf(0);
  for (int i = 0; i < m.len(); i++) {
    const Step s = m.next();
    const int semi = static_cast<int>(std::lround(12 * std::log2(s.hz / 55.0)));
    CHECK(std::fabs(semi - 12 * std::log2(s.hz / 55.0)) < 1e-6);   // нота целая
    bool in = false; for (int k = 0; k < 7; k++) if (((semi % 12) + 12) % 12 == st[k]) in = true;
    CHECK(in);
  }
}

static void modelClamps() {
  Scale sc; Model m(sc);
  m.setLen(0); CHECK(m.len() == 1);
  m.setLen(99); CHECK(m.len() == PEARL_MAX);
  m.setLen(7.6); CHECK(m.len() == 8);
  m.setLen(5); m.setLen(NAN); CHECK(m.len() == 5);
  m.setLen(INFINITY); CHECK(m.len() == 5);
  m.setDejaVu(2); CHECK(m.dejavu() == 1);
  m.setDejaVu(-1); CHECK(m.dejavu() == 0);
  m.setDejaVu(0.4); m.setDejaVu(NAN); CHECK(m.dejavu() == 0.4);
  m.setRing(1, STRUCT, 0.3); m.setRing(1, STRUCT, NAN); CHECK(m.ring(1, STRUCT) == 0.3);
  m.setRing(1, BRIGHT, INFINITY); CHECK(m.ring(1, BRIGHT) == 1);
  m.setRing(1, DAMP, -INFINITY); CHECK(m.ring(1, DAMP) == 0);
  m.setRing(-1, DAMP, 1); m.setRing(NR, DAMP, 1); m.setRing(0, 99, 1);   // мимо: ничего не падает
  m.setCloud(POS, 5); CHECK(m.cloud(POS) == 1);
  m.setCloud(99, 1); m.setCloud(-1, 1);
  m.setPearlPitch(-1, 0.5); m.setPearlPitch(PEARL_MAX, 0.5); m.togglePearl(99);
  m.setPearlPitch(3, 7); CHECK(m.pearl(3).pitch == 1);
  m.setPearlPitch(3, NAN); CHECK(m.pearl(3).pitch == 1);
}

static void modelLenOne() {
  Scale sc; Model m(sc); m.reset(4); m.setLen(1);
  for (int i = 0; i < 20; i++) CHECK(m.next().pearl == 0);
  m.setLen(16); for (int i = 0; i < 5; i++) m.next();
  m.setLen(2);                                              // курсор был за новой длиной
  for (int i = 0; i < 6; i++) { const int p = m.next().pearl; CHECK(p >= 0 && p < 2); }
}

static void modelSnapshotRoundTrip() {
  Scale sc; Model m(sc); m.reset(9);
  m.setLen(7); m.setDejaVu(0.35); m.setRing(2, DAMP, 0.9); m.setCloud(ECHO, 0.77); m.setPearlPitch(1, 0.123);
  for (int i = 0; i < 5; i++) m.next();
  const std::string js = m.snapshot();
  Model b(sc); b.restore(js);
  CHECK(b.snapshot() == js);
  for (int i = 0; i < 30; i++) { const Step x = m.next(), y = b.next(); CHECK(x.hz == y.hz && x.hit == y.hit && x.ring == y.ring); }
}

static void modelRestoreGarbage() {
  Scale sc; Model m(sc); m.reset(1);
  const char* bad[] = {"{", "[]", "", "null", "{\"len\":\"x\"}", "{\"p\":5}", "{\"len\":99,\"dejavu\":9,\"p\":[1,2,{\"p\":\"a\"}]}", "{\"rings\":[[1,2]]}"};
  for (const char* js : bad) {
    m.restore(js);
    CHECK(m.len() >= 1 && m.len() <= PEARL_MAX);
    CHECK(m.dejavu() >= 0 && m.dejavu() <= 1);
    for (int i = 0; i < PEARL_MAX; i++) CHECK(m.pearl(i).pitch >= 0 && m.pearl(i).pitch <= 1);
    for (int i = 0; i < 20; i++) m.next();
  }
}

static void modelCloudSend() {
  Scale sc; Model m(sc);
  CHECK(NCK == 8); CHECK(m.cloud(SEND) == 0.35);
  m.setCloud(SEND, 2); CHECK(m.cloud(SEND) == 1);
}

static void modelRollAndClear() {
  Scale sc; Model m(sc); m.reset(1);
  const std::string a = m.snapshot(); m.roll(); CHECK(m.snapshot() != a);
  m.clear();
  for (int i = 0; i < PEARL_MAX; i++) CHECK(m.pearl(i).rest);
  CHECK(!m.sounding());
}


// ---------- Task 2: кольца (порт Rings) ----------

// удар в резонатор: импульс на первом сэмпле, дальше тишина; возвращает odd+even
static std::vector<float> ringImpulse(double hz, double structure, double bright, double damp, double seconds, double sr = 48000) {
  rings::Resonator r; r.prepare(sr);
  r.setFreq(hz); r.setStructure(structure); r.setBrightness(bright); r.setDamping(damp); r.setPosition(0.3);
  const int total = static_cast<int>(seconds * sr);
  std::vector<float> out(total), in(128), o(128), e(128);
  for (int pos = 0; pos < total; pos += 128) {
    const int n = std::min(128, total - pos);
    for (int i = 0; i < n; i++) in[i] = (pos == 0 && i == 0) ? 1.0f : 0.0f;
    r.process(in.data(), o.data(), e.data(), n);
    for (int i = 0; i < n; i++) out[pos + i] = o[i] + e[i];
  }
  return out;
}
static double energy(const std::vector<float>& v, double sr, double t0, double t1) {
  double s = 0; for (size_t i = static_cast<size_t>(t0 * sr); i < v.size() && i < static_cast<size_t>(t1 * sr); i++) s += double(v[i]) * v[i];
  return s;
}

static void ringsRingsDown() {
  const auto v = ringImpulse(220, 0.5, 0.5, 0.5, 3);
  double peak = 0; for (size_t i = 0; i < 9600; i++) peak = std::max(peak, double(std::fabs(v[i])));
  CHECK(peak > 1e-4);
  double tail = 0; for (size_t i = 96000; i < 100800; i++) tail = std::max(tail, double(std::fabs(v[i])));
  CHECK(tail < peak / 100);
}

static void ringsDampingMonotone() {
  const double e1 = energy(ringImpulse(220, 0.5, 0.5, 0.2, 1), 48000, 0.1, 0.3);
  const double e2 = energy(ringImpulse(220, 0.5, 0.5, 0.5, 1), 48000, 0.1, 0.3);
  const double e3 = energy(ringImpulse(220, 0.5, 0.5, 0.8, 1), 48000, 0.1, 0.3);
  CHECK(e1 > e2); CHECK(e2 > e3);
}

static double centroid(const std::vector<float>& v, size_t from, int N = 2048) {
  double num = 0, den = 0;
  for (int k = 1; k < N / 2; k++) {
    double re = 0, im = 0;
    for (int i = 0; i < N; i++) { const double w = 0.5 - 0.5 * std::cos(2 * M_PI * i / N), x = v[from + i] * w, a = 2 * M_PI * k * i / N; re += x * std::cos(a); im += x * std::sin(a); }
    const double m = std::sqrt(re * re + im * im); num += k * m; den += m;
  }
  return den > 0 ? num / den : 0;
}
static void ringsBrightnessCentroid() {
  const auto dark = ringImpulse(220, 0.5, 0.1, 0.6, 1), bright = ringImpulse(220, 0.5, 0.9, 0.6, 1);
  CHECK(centroid(bright, 2000) > centroid(dark, 2000));
}

static void ringsStable() {
  // реальное применение: щипок → резонатор, любые крайние параметры — конечно и в пределах (пик < 4)
  for (double hz : {20.0, 8000.0}) for (double structure : {0.0, 1.0}) for (double bright : {0.0, 1.0}) for (double damp : {0.0, 1.0}) {
    rings::Resonator r; r.prepare(48000); rings::Plucker pl; pl.prepare(48000, 7);
    r.setFreq(hz); r.setStructure(structure); r.setBrightness(bright); r.setDamping(damp); r.setPosition(0.5);
    std::vector<float> in(128), o(128), e(128);
    double peak = 0; bool finite = true;
    pl.trigger(hz, bright, 0.3, 1.0);
    for (int b = 0; b < 48000 * 10 / 128; b++) {
      pl.process(in.data(), 128);
      r.process(in.data(), o.data(), e.data(), 128);
      for (int i = 0; i < 128; i++) { if (!std::isfinite(o[i]) || !std::isfinite(e[i])) finite = false; peak = std::max(peak, double(std::max(std::fabs(o[i]), std::fabs(e[i])))); }
    }
    CHECK(finite); CHECK(peak < 4.0);
    // линейная устойчивость: импульсный отклик затухает монотонно и к 60-й секунде тише пика минимум на 40 дБ
    // (самая долгая крайность — structure 1, brightness 1, damping 0, 20 Гц: верхние моды у Найквиста звенят дольше основной)
    const auto h = ringImpulse(hz, structure, bright, damp, 60);
    double pk = 0, mid = 0, tail = 0; bool fin = true;
    for (size_t i = 0; i < h.size(); i++) {
      if (!std::isfinite(h[i])) fin = false;
      const double a = std::fabs(h[i]);
      if (i < 48000 * 5) pk = std::max(pk, a);
      if (i >= 48000 * 30 && i < 48000 * 35) mid = std::max(mid, a);
      if (i >= 48000 * 55) tail = std::max(tail, a);
    }
    CHECK(fin); CHECK(tail <= pk * 1e-2 + 1e-12); CHECK(tail <= mid + 1e-12);
  }
}

static void ringsIdleCheap() {
  rings::Resonator r; r.prepare(48000); r.setFreq(220); r.setDamping(0.5);
  std::vector<float> in(128, 0.0f), o(128), e(128);
  CHECK(!r.active());
  in[0] = 1; r.process(in.data(), o.data(), e.data(), 128); in[0] = 0;
  CHECK(r.active());
  for (int b = 0; b < 48000 * 30 / 128; b++) r.process(in.data(), o.data(), e.data(), 128);
  CHECK(!r.active());
  for (int i = 0; i < 128; i++) CHECK(o[i] == 0 && e[i] == 0);
  in[0] = 1; r.process(in.data(), o.data(), e.data(), 128);   // снова оживает
  CHECK(r.active());
}

static void ringsNoAlloc() {
  static_assert(sizeof(rings::Resonator) <= 64 * 1024, "resonator size");
  static_assert(sizeof(rings::Plucker) <= 8 * 1024, "plucker size");
  CHECK(rings::kModes == 28);
}

static void ringsPluckerBurst() {
  rings::Plucker p; p.prepare(48000, 1);
  std::vector<float> out(4800);
  p.trigger(220, 0.5, 0.3, 1.0); p.process(out.data(), 4800);
  double e0 = 0, e1 = 0; for (int i = 0; i < 400; i++) e0 += out[i] * out[i]; for (int i = 4000; i < 4800; i++) e1 += out[i] * out[i];
  CHECK(e0 > 0.1); CHECK(e1 < e0 * 0.01);
  p.trigger(220, 0.5, 0.3, 0.0); p.process(out.data(), 4800);   // нулевая сила — тишина
  double e2 = 0; for (float x : out) e2 += x * x; CHECK(e2 < 1e-9);
  p.trigger(NAN, NAN, NAN, NAN); p.process(out.data(), 4800);
  for (float x : out) CHECK(std::isfinite(x));
}

// ---------- Task 3: облака ----------

static hymenaeus::Cloud& theCloud(uint32_t seed = 1) { static hymenaeus::Cloud c; c.prepare(48000, seed); return c; }   // 1+ МБ — не на стек

static uint32_t cloudSt = 99;
static float cloudNoise() { cloudSt = cloudSt * 1664525u + 1013904223u; return (cloudSt >> 8) / 8388608.0f - 1; }

// n сэмплов: вход — шум амплитуды amp (0 — тишина), возвращает пик, суммарную энергию и признак NaN
struct CloudRun { double peak = 0, energy = 0; bool finite = true; };
static CloudRun runCloud(hymenaeus::Cloud& c, int n, float amp, CloudRun* acc = nullptr) {
  CloudRun r;
  for (int i = 0; i < n; i++) {
    const float x = amp * cloudNoise();
    float l, rr; c.write(x, x * 0.5f); c.process(l, rr);
    if (!std::isfinite(l) || !std::isfinite(rr)) r.finite = false;
    r.peak = std::max(r.peak, double(std::max(std::fabs(l), std::fabs(rr)))); r.energy += double(l) * l + double(rr) * rr;
  }
  if (acc) { acc->peak = std::max(acc->peak, r.peak); acc->energy += r.energy; acc->finite = acc->finite && r.finite; }
  return r;
}

static void cloudSilentIn() {
  auto& c = theCloud(); c.set(0.5, 0.4, 0.8, 0.5, false, 0.5);
  CHECK(c.sleeping());
  const CloudRun r = runCloud(c, 48000, 0);
  CHECK(r.energy == 0); CHECK(c.sleeping());
}

static void cloudEchoesInput() {
  auto& c = theCloud(); c.set(0.2, 0.3, 0.8, 0.5, false, 0);
  runCloud(c, 24000, 0.5f);                                // полсекунды шума
  CHECK(!c.sleeping());
  CloudRun later = runCloud(c, 48000, 0);                  // дальше тишина на входе, облако ещё звучит
  CHECK(later.energy > 1e-3);
}

static void cloudFreezeHolds() {
  auto& c = theCloud(); c.set(0.3, 0.3, 0.7, 0.5, false, 0);
  const CloudRun live = runCloud(c, 48000, 0.5f);
  c.set(0.3, 0.3, 0.7, 0.5, true, 0);
  runCloud(c, 48000, 0);                                   // секунда на затухание живого хвоста
  const CloudRun held = runCloud(c, 3 * 48000, 0);         // замороженное держится
  CHECK(held.energy > 0.01 * live.energy);
  CHECK(!c.sleeping());
  c.set(0.3, 0.3, 0.7, 0.5, false, 0);                     // разморозка: снова пишет, за ~3.5 с буфер пустеет и облако засыпает
  runCloud(c, 4 * 48000, 0);
  CHECK(c.sleeping());
}

static void cloudEchoDecays() {                             // echo = 1 на плотном облаке: обратная связь затухает, облако засыпает
  for (double dens : {0.5, 1.0}) for (double size : {0.3, 1.0}) {
    auto& c = theCloud(); c.set(0.0, size, dens, 0.5, false, 1);
    runCloud(c, 2000, 0.8f);                               // короткий всплеск
    runCloud(c, 20 * 48000, 0);
    const CloudRun tail = runCloud(c, 48000, 0);
    CHECK(tail.energy < 1e-3); CHECK(c.sleeping());
  }
}

static void cloudFreezeEmpty() {
  auto& c = theCloud(); c.set(0.5, 0.5, 1, 0.5, true, 1);
  const CloudRun r = runCloud(c, 48000, 0);
  CHECK(r.finite); CHECK(r.energy == 0); CHECK(c.sleeping());
}

static void cloudNoNan() {
  auto& c = theCloud(5);
  const double vals[] = {0, 1, -3, 7, NAN, INFINITY, -INFINITY, 0.5};
  CloudRun all;
  for (double a : vals) for (double b : vals) {
    c.set(a, b, a, b, a > 0.5, b);
    runCloud(c, 128 * 4, 0.7f, &all);
  }
  CHECK(all.finite); CHECK(all.peak < 4.0);
}

static void cloudDensityBounded() {
  auto& c = theCloud(2); c.set(0.4, 1, 1, 0.5, false, 0.8);   // крупные зёрна, предельная плотность и эхо
  CloudRun all; runCloud(c, 5 * 48000, 1.0f, &all);
  CHECK(all.finite); CHECK(all.peak < 4.0);
  CHECK(c.grains() <= hymenaeus::Cloud::MAX_GRAINS);
}

static void cloudSleepsAgain() {
  auto& c = theCloud(); c.set(0.5, 0.4, 0.8, 0.5, false, 0);
  runCloud(c, 12000, 0.5f);
  CHECK(!c.sleeping());
  runCloud(c, 4 * 48000, 0);                               // буфер 3 с: к концу пятой секунды в нём тишина
  CHECK(c.sleeping());
}

// ---------- Task 4: мир ----------

static hymenaeus::World& theWorld(const Scale&, uint32_t seed = 1) { static Scale gs; static hymenaeus::World w(gs); w.prepare(48000, seed); return w; }   // мир держит ссылку на лад — свой, статический

struct WRun { double peak = 0, energy = 0; int heard = 0, maxPerBlock = 0; bool finite = true; };
// blocks блоков по 128; clk — часы хоста (или nullptr); вход мира — только его действия
static WRun runWorld(hymenaeus::World& w, int blocks, sdk::Clock* clk = nullptr, double* heardRings = nullptr) {
  static float L[128], R[128], sL[128], sR[128];
  sdk::Out out{L, R, sL, sR};
  WRun r; double hb[64];
  for (int b = 0; b < blocks; b++) {
    if (clk) clk->now = b * 128.0 / 48000;
    w.process(out, 128);
    for (int i = 0; i < 128; i++) {
      for (float x : {L[i], R[i], sL[i], sR[i]}) if (!std::isfinite(x)) r.finite = false;
      r.peak = std::max(r.peak, double(std::max(std::fabs(L[i]), std::fabs(R[i])))); r.energy += double(L[i]) * L[i] + double(R[i]) * R[i];
    }
    const int k = w.heard(hb, 64);
    if (k > 0) { r.heard += static_cast<int>(hb[0]); r.maxPerBlock = std::max(r.maxPerBlock, static_cast<int>(hb[0])); if (heardRings) for (int j = 0; j < hb[0]; j++) heardRings[static_cast<int>(hb[1 + 2 * j])]++; }
  }
  return r;
}
static void act(hymenaeus::World& w, int kind, int i, std::initializer_list<double> a) { std::vector<double> v(a); w.action(kind, i, v.data(), static_cast<int>(v.size())); }

static void worldSilentWhenStopped() {
  Scale sc; auto& w = theWorld(sc);
  const WRun r = runWorld(w, 200);
  CHECK(r.energy == 0); CHECK(!w.sounding()); CHECK(r.heard == 0);
}

static void worldStepsInTime() {
  Scale sc; auto& w = theWorld(sc);
  act(w, hymenaeus::THREAD, 0, {0, 1});                                   // déjà vu 1
  for (int i = 0; i < hymenaeus::PEARL_MAX; i++) act(w, hymenaeus::PEARL, i, {1, 0});   // ни одной паузы
  act(w, hymenaeus::BPM, 0, {120}); act(w, hymenaeus::RUN, 0, {1});
  const WRun r = runWorld(w, 4 * 48000 / 128);
  CHECK(r.heard >= 15 && r.heard <= 17);                                  // 120 уд/мин, восьмые: 4 шага в секунду
  CHECK(r.energy > 0);
}

static void worldStrikeSounds() {
  Scale sc; auto& w = theWorld(sc);
  act(w, hymenaeus::STRIKE, 1, {0.3, 1});
  const WRun a = runWorld(w, 48000 * 3 / 10 / 128);
  CHECK(a.peak > 0.01); CHECK(a.finite);
  CHECK(w.sounding());
  const WRun b = runWorld(w, 48000 * 90 / 128);                           // облако с эхом гаснет за минуту
  CHECK(b.finite);
  CHECK(!w.sounding());
}

static void worldNoStepFlood() {
  Scale sc; auto& w = theWorld(sc);
  for (double bpm : {0.0, -5.0, 1e9, double(NAN)}) act(w, hymenaeus::BPM, 0, {bpm});
  double m[hymenaeus::MIRROR_N]; CHECK(w.mirror(m, hymenaeus::MIRROR_N) == hymenaeus::MIRROR_N);
  CHECK(m[1] >= 20 && m[1] <= 300);                                       // bpm в зеркале всегда в допустимых границах
  act(w, hymenaeus::RUN, 0, {1});
  sdk::Clock clk; clk.sr = 48000; clk.grid.on = true; clk.grid.t0 = 0; clk.grid.p = 1e-4;   // безумная сетка: доля 0.1 мс
  w.attach(&clk);
  const WRun r = runWorld(w, 400, &clk);
  CHECK(r.maxPerBlock <= 4); CHECK(r.finite);
  clk.grid.p = -1; const WRun r2 = runWorld(w, 100, &clk); CHECK(r2.maxPerBlock <= 4 && r2.finite);
  w.attach(nullptr);
}

static void worldStopNoStuck() {
  Scale sc; auto& w = theWorld(sc);
  act(w, hymenaeus::RUN, 0, {1});
  runWorld(w, 2 * 48000 / 128);
  act(w, hymenaeus::RUN, 0, {0});
  const WRun r = runWorld(w, 60 * 48000 / 128);
  CHECK(r.finite); CHECK(!w.sounding());
}

static void worldSnapshotRoundTrip() {
  Scale sc;
  auto& w = theWorld(sc, 4);
  act(w, hymenaeus::THREAD, 0, {0, 0.3}); act(w, hymenaeus::THREAD, 0, {1, 9}); act(w, hymenaeus::BPM, 0, {77});
  act(w, hymenaeus::RING, 2, {2, 0.9}); act(w, hymenaeus::CLOUD, 0, {5, 0.6});
  const std::string js = w.snapshot();
  hymenaeus::World* b = new hymenaeus::World(sc); b->prepare(48000, 4); b->restore(js);
  CHECK(b->snapshot() == js);
  b->restore("{"); CHECK(b->snapshot() != "");
  b->restore(js); CHECK(b->snapshot() == js);
  delete b;
}

static void worldGarbageActions() {
  Scale sc; auto& w = theWorld(sc);
  const double vals[] = {NAN, INFINITY, -INFINITY, 1e30, -7, 0, 0.5, 3};
  bool finite = true;
  for (int kind = -4; kind < 14; kind++) for (int i : {-5, -1, 0, 1, 2, 3, 15, 99})
    for (double v : vals) {
      double a[3] = {v, v, v};
      w.action(kind, i, a, 3); w.action(kind, i, a, 1); w.action(kind, i, a, 0); w.action(kind, i, nullptr, 0);
      finite = runWorld(w, 2).finite && finite;
    }
  CHECK(finite);
  double m[hymenaeus::MIRROR_N]; w.mirror(m, hymenaeus::MIRROR_N);
  for (double x : m) CHECK(std::isfinite(x));
  CHECK(w.mirror(m, 3) == 0);                                              // буфер короче зеркала — 0 чисел
}

static void worldBudget() {
  Scale sc; auto& w = theWorld(sc);
  act(w, hymenaeus::CLOUD, 0, {2, 1}); act(w, hymenaeus::CLOUD, 0, {1, 1}); act(w, hymenaeus::CLOUD, 0, {5, 0.8});
  act(w, hymenaeus::BPM, 0, {300}); act(w, hymenaeus::RUN, 0, {1});
  for (int r = 0; r < 3; r++) act(w, hymenaeus::RING, r, {2, 0.1});       // damping 0.1: звенят долго
  for (int i = 0; i < hymenaeus::PEARL_MAX; i++) act(w, hymenaeus::PEARL, i, {1, 0});
  runWorld(w, 48000 * 2 / 128);                                            // разогнать
  const int blocks = 48000 * 4 / 128;
  const auto t0 = std::chrono::steady_clock::now();
  runWorld(w, blocks);
  const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), audio = blocks * 128.0 / 48000;
  CHECK(sec < audio * 0.25);
  std::printf("  budget: %.2f%% of real time\n", 100 * sec / audio);
}

static void worldRingsByPitch() {
  Scale sc; auto& w = theWorld(sc, 3);
  act(w, hymenaeus::THREAD, 0, {0, 1}); act(w, hymenaeus::THREAD, 1, {1, 3});
  for (int i = 0; i < 3; i++) act(w, hymenaeus::PEARL, i, {1, 0});
  act(w, hymenaeus::PEARL, 0, {0, 0.1}); act(w, hymenaeus::PEARL, 1, {0, 0.5}); act(w, hymenaeus::PEARL, 2, {0, 0.9});
  act(w, hymenaeus::BPM, 0, {240}); act(w, hymenaeus::RUN, 0, {1});
  double rings[8] = {};
  runWorld(w, 48000 * 3 / 128, nullptr, rings);
  CHECK(rings[0] > 3 && rings[1] > 3 && rings[2] > 3);                     // по кругу бьют все три кольца
}


// ---------- Task 4b: режимы колец и полифония ----------

static void modelRingModePoly() {
  Scale sc; Model m(sc);
  CHECK(NRK == 6); CHECK(m.ring(0, MODE) == 0); CHECK(m.ring(2, POLY) == 0);
  m.setRing(1, MODE, 1); m.setRing(1, POLY, 0.5);
  CHECK(m.ring(1, MODE) == 1); CHECK(m.ring(1, POLY) == 0.5);
  m.setRing(1, MODE, 7); CHECK(m.ring(1, MODE) == 1);
  m.setRing(1, POLY, -3); CHECK(m.ring(1, POLY) == 0);
  m.setRing(1, POLY, NAN); CHECK(m.ring(1, POLY) == 0);
  m.setRing(1, MODE, INFINITY); CHECK(m.ring(1, MODE) == 1);
  CHECK(modeOf(0) == 0 && modeOf(1) == 1 && modeOf(0.49) == 0 && modeOf(0.5) == 1);
  CHECK(polyOf(0) == 1 && polyOf(0.5) == 2 && polyOf(1) == 4 && polyOf(0.3) == 1 && polyOf(0.34) == 2 && polyOf(0.67) == 4);
  m.setRing(0, MODE, 1); m.setRing(0, POLY, 1);
  Model b(sc); b.restore(m.snapshot());
  CHECK(b.snapshot() == m.snapshot()); CHECK(b.ring(0, MODE) == 1 && b.ring(0, POLY) == 1);
  Model g(sc); g.restore("{\"rings\":[[0.5,0.5,0.5,0.8],[1,2,3,4,5,6,7,8],\"x\"]}");   // старый снимок (4 ключа) и лишние ключи
  CHECK(g.ring(0, MODE) == 0 && g.ring(1, MODE) == 1 && g.ring(1, POLY) == 1);
}

static hymenaeus::Sound& theSound(uint32_t seed = 1) { static hymenaeus::Sound s; s.prepare(48000, seed); return s; }   // кольца и струны — сотни КБ

struct SRun { std::vector<float> l; bool finite = true; double peak = 0; };
static SRun renderSound(hymenaeus::Sound& s, int samples, const std::function<void(int)>& perBlock = nullptr) {
  SRun r; float L[128], R[128], sL[128], sR[128];
  for (int done = 0; done < samples; done += 128) {
    if (perBlock) perBlock(done / 128);
    s.render(L, R, sL, sR, 128);
    for (int i = 0; i < 128; i++) {
      if (!std::isfinite(L[i]) || !std::isfinite(R[i])) r.finite = false;
      r.l.push_back(L[i]); r.peak = std::max(r.peak, double(std::max(std::fabs(L[i]), std::fabs(R[i]))));
    }
  }
  return r;
}
static double goertzel(const std::vector<float>& x, size_t from, size_t to, double hz) {
  const double w = 2 * 3.14159265358979 * hz / 48000; double s1 = 0, s2 = 0, c = 2 * std::cos(w);
  for (size_t i = from; i < to; i++) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
  return (s1 * s1 + s2 * s2 - c * s1 * s2) / double(to - from);
}
// максимальный скачок между соседними сэмплами на границах блоков и внутри блоков
static void jumps(const std::vector<float>& x, double& boundary, double& inner) {
  boundary = inner = 0;
  for (size_t i = 1; i < x.size(); i++) { const double d = std::fabs(x[i] - x[i - 1]); (i % 128 == 0 ? boundary : inner) = std::max(i % 128 == 0 ? boundary : inner, d); }
}

static void soundStringsSound() {
  auto& s = theSound();
  s.setRing(0, 0.5, 0.5, 0.5, 0.8, 1, 0);
  s.strike(0, 220, 0.3, 0.8);
  const SRun a = renderSound(s, 48000 * 3 / 10);
  CHECK(a.finite); CHECK(a.peak > 0.01); CHECK(a.peak <= 1.0001);
  CHECK(s.sounding());
  const SRun b = renderSound(s, 48000 * 120);
  CHECK(b.finite); CHECK(!s.sounding());
  s.setRing(0, 0.5, 0.5, 0.0, 1.0, 1, 0);                  // damp 0 — самый долгий звон: всё равно гаснет
  s.strike(0, 110, 0.3, 1.0);
  renderSound(s, 48000 * 150);
  CHECK(!s.sounding());
}

// «частота» окна: sqrt(E[Δx²]/E[x²]) ∝ спектральный центр
static double centroidOf(const std::vector<float>& x, size_t a, size_t b) {
  double e = 0, d = 0;
  for (size_t i = a + 1; i < b; i++) { e += double(x[i]) * x[i]; const double dx = x[i] - x[i - 1]; d += dx * dx; }
  return std::sqrt(d / std::max(e, 1e-12));
}

static void soundStructureGlide() {
  auto& s = theSound();
  double cen[4];                                           // структура 0…1 при одном ударе: строй сочувственных струн, значит и центр, поднимается
  for (int k = 0; k < 4; k++) {
    s.prepare(48000, 1);
    s.setRing(0, k / 3.0, 0.7, 0.2, 0.8, 1, 0);
    s.strike(0, 196, 0.3, 0.9);
    const SRun r = renderSound(s, 48000);
    CHECK(r.finite);
    cen[k] = centroidOf(r.l, 4800, 48000);
  }
  std::printf("  structure centre: %.3f %.3f %.3f %.3f\n", cen[0], cen[1], cen[2], cen[3]);
  CHECK(cen[3] > cen[0] * 1.2);
  for (int k = 1; k < 4; k++) CHECK(cen[k] > cen[k - 1] * 0.95);
  // развёртка 0→1 за 2 с на звенящем кольце: звук идёт без щелчков на границах блоков
  s.prepare(48000, 1);
  s.setRing(0, 0.0, 0.7, 0.2, 0.8, 1, 0);
  s.strike(0, 196, 0.3, 0.9);
  renderSound(s, 4800);
  const int blocks = 48000 * 2 / 128;
  const SRun r = renderSound(s, blocks * 128, [&](int b) { s.setRing(0, double(b) / blocks, 0.7, 0.2, 0.8, 1, 0); });
  CHECK(r.finite); CHECK(r.peak > 0.01);
  double bo, in; jumps(r.l, bo, in);
  CHECK(bo <= in * 1.5 + 1e-4);
}

static void soundPolyVoices() {
  const double f[5] = {200, 273, 351, 467, 613};
  for (int p : {0, 1}) {                                    // 0 — одно кольцо-голос, 1 — четыре
    auto& s = theSound();
    s.setRing(1, 0.5, 0.5, 0.2, 0.8, 0, p);
    std::vector<float> all;
    for (int k = 0; k < 4; k++) { s.strike(1, f[k], 0.3, 0.8); const SRun r = renderSound(s, 48000 / 4); all.insert(all.end(), r.l.begin(), r.l.end()); }
    const size_t n = all.size(), w = 48000 / 8;
    const double p1 = goertzel(all, n - w, n, f[0]), p4 = goertzel(all, n - w, n, f[3]);
    if (p == 0) CHECK(p1 < p4 * 1e-3); else CHECK(p1 > p4 * 0.005);      // 4 голоса держат первый тон, 1 голос — перебивает
    if (p == 1) {
      s.strike(1, f[4], 0.3, 0.8);                                   // пятый забирает самый старый
      const SRun r = renderSound(s, 48000 / 4); const size_t m = r.l.size();
      const double q1 = goertzel(r.l, m - w, m, f[0]), q3 = goertzel(r.l, m - w, m, f[2]), q5 = goertzel(r.l, m - w, m, f[4]);
      CHECK(q5 > 0); CHECK(q1 < q3 * 0.05);
    }
  }
}

static void soundPolyStrings() {
  auto& s = theSound();
  s.setRing(2, 0.5, 0.5, 0.4, 0.8, 1, 1.0);                // strings, poly 4 (по 2 струны)
  float mx = 0;
  for (int k = 0; k < 6; k++) { s.strike(2, 300 + 40 * k, 0.3, 0.8); const SRun r = renderSound(s, 4800); CHECK(r.finite); mx = std::max(mx, float(r.peak)); }
  CHECK(mx > 0.01); CHECK(mx <= 1.0001);
  s.setRing(2, 0.5, 0.5, 0.4, 0.8, 1, 0.5);                // смена полифонии на ходу
  s.strike(2, 330, 0.3, 0.8);
  const SRun r = renderSound(s, 48000); CHECK(r.finite); CHECK(r.peak > 0.01);
}

static void soundModeSwitchLive() {
  auto& s = theSound();
  s.setRing(0, 0.5, 0.5, 0.3, 0.8, 0, 0);
  s.strike(0, 220, 0.3, 0.9);
  SRun r = renderSound(s, 48000 / 5);
  for (int k = 0; k < 6; k++) {
    s.setRing(0, 0.5, 0.5, 0.3, 0.8, k & 1 ? 0 : 1, 0);     // режим меняется на звенящем кольце
    const SRun x = renderSound(s, 4800); r.l.insert(r.l.end(), x.l.begin(), x.l.end()); r.finite = r.finite && x.finite;
    if (k == 2) s.strike(0, 330, 0.3, 0.9);
  }
  CHECK(r.finite);
  double bo, in; jumps(r.l, bo, in);
  CHECK(bo <= in * 1.5 + 1e-4);
}

static void worldMirrorSize() {
  CHECK(MIRROR_RING == 7); CHECK(MIRROR_N == 5 + 32 + NR * 7 + NCK + 1); CHECK(MIRROR_N == 67);
  Scale sc; auto& w = theWorld(sc);
  act(w, hymenaeus::RING, 1, {4, 1}); act(w, hymenaeus::RING, 1, {5, 0.5});
  double m[MIRROR_N]; CHECK(w.mirror(m, MIRROR_N) == MIRROR_N);
  const int base = 5 + 32 + 1 * 7;
  CHECK(m[base + 4] == 1); CHECK(m[base + 5] == 0.5); CHECK(m[base + 6] == 0);          // mode, poly, energy
  act(w, hymenaeus::RING, 1, {6, 1}); CHECK(w.mirror(m, MIRROR_N) == MIRROR_N); CHECK(m[base + 4] == 1);   // ключ 6 вне диапазона
  act(w, sdk::ACT_LFO, -1, {double(NRK + 4), 0.75});                                  // LFO-ключ кольца 1, MODE
  w.mirror(m, MIRROR_N); CHECK(m[base + 4] == 0.75);
}

int main() {
  modelSeedDeterministic(); modelDejaVuOneRepeats(); modelDejaVuZeroVaries(); modelRestsSilent();
  modelRingBands(); modelPitchToRing(); modelScaleSnap(); modelClamps(); modelLenOne();
  modelSnapshotRoundTrip(); modelRestoreGarbage(); modelRollAndClear(); modelCloudSend();
  ringsRingsDown(); ringsDampingMonotone(); ringsBrightnessCentroid(); ringsStable(); ringsIdleCheap(); ringsNoAlloc(); ringsPluckerBurst();
  cloudSilentIn(); cloudEchoesInput(); cloudFreezeHolds(); cloudFreezeEmpty(); cloudEchoDecays(); cloudNoNan(); cloudDensityBounded(); cloudSleepsAgain();
  worldSilentWhenStopped(); worldStepsInTime(); worldStrikeSounds(); worldNoStepFlood(); worldStopNoStuck(); worldSnapshotRoundTrip();
  worldGarbageActions(); worldBudget(); worldRingsByPitch();
  modelRingModePoly(); soundStringsSound(); soundStructureGlide(); soundPolyVoices(); soundPolyStrings(); soundModeSwitchLive(); worldMirrorSize();
  std::printf("hymenaeus-test: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
