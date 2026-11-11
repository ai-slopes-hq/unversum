// Тесты мира Hymenaeus (XIV): модель, кольца, облака, мир. Сборка и запуск — core/build.sh → core/build/hymenaeus-test.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../host/scale.hpp"
#include "../worlds/hymenaeus/model.hpp"
#include "../worlds/hymenaeus/rings/plucker.hpp"
#include "../worlds/hymenaeus/rings/resonator.hpp"

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

int main() {
  modelSeedDeterministic(); modelDejaVuOneRepeats(); modelDejaVuZeroVaries(); modelRestsSilent();
  modelRingBands(); modelPitchToRing(); modelScaleSnap(); modelClamps(); modelLenOne();
  modelSnapshotRoundTrip(); modelRestoreGarbage(); modelRollAndClear();
  ringsRingsDown(); ringsDampingMonotone(); ringsBrightnessCentroid(); ringsStable(); ringsIdleCheap(); ringsNoAlloc(); ringsPluckerBurst();
  std::printf("hymenaeus-test: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
