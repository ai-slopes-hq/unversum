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
    const Step x = a.next(i & 1), y = b.next(i & 1);
    CHECK(x.hit == y.hit && x.ring == y.ring && x.hz == y.hz && x.pearl == y.pearl);
  }
}

static void modelDejaVuOneRepeats() {
  Scale sc; Model m(sc); m.reset(3); m.setDejaVu(0, 1);
  const int len = m.thread(0).len;
  std::vector<double> first;
  for (int i = 0; i < len; i++) first.push_back(m.next(0).hz);
  for (int round = 0; round < 3; round++)
    for (int i = 0; i < len; i++) CHECK(m.next(0).hz == first[i]);
}

static void modelDejaVuZeroVaries() {
  Scale sc; Model m(sc); m.reset(3); m.setDejaVu(0, 0);
  const int len = m.thread(0).len;
  std::vector<std::vector<double>> rounds;
  for (int r = 0; r < 8; r++) { std::vector<double> v; for (int i = 0; i < len; i++) v.push_back(m.next(0).hz); rounds.push_back(v); }
  int different = 0;
  for (int r = 1; r < 8; r++) if (rounds[r] != rounds[0]) different++;
  CHECK(different >= 6);
}

static void modelRestsSilent() {
  Scale sc; Model m(sc); m.reset(2); m.setDejaVu(0, 1);
  for (int i = 0; i < PEARL_MAX; i++) for (int t = 0; t < NR; t++) m.setRest(t, i, true);
  CHECK(!m.sounding());
  for (int i = 0; i < m.thread(0).len * 3; i++) CHECK(!m.next(0).hit);
  m.togglePearl(0, 0); CHECK(m.thread(0).p[0].rest == false); CHECK(m.sounding());
  m.togglePearl(0, 0); CHECK(m.thread(0).p[0].rest == true);
}

static void modelRingsCount() { CHECK(NR == 2); CHECK(NRK == 5); CHECK(NLOC == 4); }

static void modelDefaultRanges() {
  Scale sc; Model m(sc);
  CHECK(m.thread(0).lo == 45 && m.thread(0).hi == 69);
  CHECK(m.thread(1).lo == 57 && m.thread(1).hi == 81);
}

static void modelTwoThreadsIndependent() {
  Scale sc; Model m(sc); m.reset(1); m.setDejaVu(0, 1); m.setDejaVu(1, 1);
  m.setLen(0, 3); m.setLen(1, 5);
  for (int i = 0; i < 15; i++) { const Step a = m.next(0), b = m.next(1); CHECK(a.ring == 0 && b.ring == 1); }
  CHECK(m.thread(0).cur == 15 % 3); CHECK(m.thread(1).cur == 15 % 5);
  m.next(1); CHECK(m.thread(0).cur == 0 && m.thread(1).cur == 1);   // нити не влияют на курсоры друг друга
  const Step bad = m.next(2); CHECK(!bad.hit);                       // нить вне диапазона — тишина, не падает
}

static void modelRangeClamp() {
  Scale sc; Model m(sc);
  m.setRange(0, 10, 200); CHECK(m.thread(0).lo == 24 && m.thread(0).hi == 96);
  m.setRange(0, 60, 62); CHECK(m.thread(0).hi - m.thread(0).lo == 5 && m.thread(0).lo == 60);
  m.setRange(0, 95, 96); CHECK(m.thread(0).hi == 96 && m.thread(0).hi - m.thread(0).lo == 5);
  m.setRange(0, 70, 50); CHECK(m.thread(0).lo == 50 && m.thread(0).hi == 70);
  m.setRange(0, 50.4, 70.6); CHECK(m.thread(0).lo == 50 && m.thread(0).hi == 71);
  m.setRange(0, 40, 60);
  m.setRange(0, NAN, 80); m.setRange(0, 30, NAN); CHECK(m.thread(0).lo == 40 && m.thread(0).hi == 60);
  m.setRange(0, -INFINITY, INFINITY); CHECK(m.thread(0).lo == 24 && m.thread(0).hi == 96);
  m.setRange(-1, 30, 60); m.setRange(NR, 30, 60);                    // мимо: не падает
  CHECK(m.thread(1).lo == 57 && m.thread(1).hi == 81);
}

static void modelPitchWithinRange() {
  Scale sc; Model m(sc); m.reset(3); m.setDejaVu(0, 0);              // dejavu 0: нить всё время перебрасывается
  m.setRange(0, 50, 62);
  for (int i = 0; i < 1000; i++) {
    const Step s = m.next(0);
    const double midi = 69 + 12 * std::log2(s.hz / 440.0);
    CHECK(midi >= 50 - 1 && midi <= 62 + 1);
  }
  m.setPearlPitch(0, 0, 0); m.setPearlPitch(0, 1, 1);
  CHECK(std::fabs(69 + 12 * std::log2(m.hzOf(0, 0) / 440.0) - 50) < 1e-6);
  CHECK(std::fabs(69 + 12 * std::log2(m.hzOf(0, 1) / 440.0) - 62) < 1e-6);
}

static void modelDeterministic() {
  Scale sc; Model a(sc), b(sc); a.reset(7); b.reset(7);
  CHECK(a.snapshot() == b.snapshot());
  for (int i = 0; i < 50; i++) { a.next(i % 2); b.next(i % 2); }
  CHECK(a.snapshot() == b.snapshot());
  Model c(sc); c.reset(8); CHECK(c.snapshot() != Model(sc).snapshot());
  Model d(sc), e(sc); d.reset(8); e.reset(8);
  for (int i = 0; i < 37; i++) { d.next(i % 2); e.next(i % 2); }
  CHECK(d.snapshot() == e.snapshot());
}

static void modelScaleSnap() {
  Scale sc; sc.on = true; sc.idx = 0; sc.root = 0;          // мажор от A
  Model m(sc); m.reset(7); m.setDejaVu(0, 1);
  for (int i = 0; i < PEARL_MAX; i++) { m.setRest(0, i, false); m.setPearlPitch(0, i, i / 15.0); }
  const int* st = Scale::stepsOf(0);
  for (int i = 0; i < m.thread(0).len; i++) {
    const Step s = m.next(0);
    const int semi = static_cast<int>(std::lround(12 * std::log2(s.hz / 55.0)));
    CHECK(std::fabs(semi - 12 * std::log2(s.hz / 55.0)) < 1e-6);   // нота целая
    bool in = false; for (int k = 0; k < 7; k++) if (((semi % 12) + 12) % 12 == st[k]) in = true;
    CHECK(in);
  }
}

static void modelScaleSnapLowRange() {
  Scale sc; sc.on = true; sc.idx = 0; sc.root = 0;
  Model m(sc); m.reset(5); m.setRange(0, 24, 40);
  const int* st = Scale::stepsOf(0);
  bool below = false;
  for (int k = 0; k <= 40; k++) {
    const double midi = 69 + 12 * std::log2(m.hzOf(0, k / 40.0) / 440.0);
    const int semi = static_cast<int>(std::lround(midi - 33));
    CHECK(std::fabs(midi - 33 - semi) < 1e-6);
    bool in = false; for (int j = 0; j < 7; j++) if (((semi % 12) + 12) % 12 == st[j]) in = true;
    CHECK(in);
    CHECK(midi >= 24 - 1 - 1e-6 && midi <= 40 + 1 + 1e-6);
    if (k == 0) below = midi <= 33 + 1e-6;
  }
  CHECK(below);
}

static void modelClamps() {
  Scale sc; Model m(sc);
  for (int t = 0; t < NR; t++) {
    const Thread& th = m.thread(t);
    m.setLen(t, 0); CHECK(th.len == 1);
    m.setLen(t, 99); CHECK(th.len == PEARL_MAX);
    m.setLen(t, 7.6); CHECK(th.len == 8);
    m.setLen(t, 5); m.setLen(t, NAN); CHECK(th.len == 5);
    m.setLen(t, INFINITY); CHECK(th.len == 5);
    m.setDejaVu(t, 2); CHECK(th.dejavu == 1);
    m.setDejaVu(t, -1); CHECK(th.dejavu == 0);
    m.setDejaVu(t, 0.4); m.setDejaVu(t, NAN); CHECK(th.dejavu == 0.4);
  }
  m.setLen(-1, 3); m.setLen(NR, 3); m.setDejaVu(NR, 1); m.togglePearl(NR, 0); m.setRest(-1, 0, true); m.setPearlPitch(NR, 0, 1);
  m.setRing(1, STRUCT, 0.3); m.setRing(1, STRUCT, NAN); CHECK(m.ring(1, STRUCT) == 0.3);
  m.setRing(1, BRIGHT, INFINITY); CHECK(m.ring(1, BRIGHT) == 1);
  m.setRing(1, DAMP, -INFINITY); CHECK(m.ring(1, DAMP) == 0);
  m.setRing(-1, DAMP, 1); m.setRing(NR, DAMP, 1); m.setRing(0, 99, 1);   // мимо: ничего не падает
  m.setCloud(POS, 5); CHECK(m.cloud(POS) == 1);
  m.setCloud(99, 1); m.setCloud(-1, 1);
  m.setPearlPitch(0, -1, 0.5); m.setPearlPitch(0, PEARL_MAX, 0.5); m.togglePearl(0, 99);
  m.setPearlPitch(0, 3, 7); CHECK(m.thread(0).p[3].pitch == 1);
  m.setPearlPitch(0, 3, NAN); CHECK(m.thread(0).p[3].pitch == 1);
}

static void modelLenOne() {
  Scale sc; Model m(sc); m.reset(4); m.setLen(0, 1);
  for (int i = 0; i < 20; i++) CHECK(m.next(0).pearl == 0);
  m.setLen(0, 16); for (int i = 0; i < 5; i++) m.next(0);
  m.setLen(0, 2);                                              // курсор был за новой длиной
  for (int i = 0; i < 6; i++) { const int p = m.next(0).pearl; CHECK(p >= 0 && p < 2); }
}

static void modelSnapshotRoundTrip() {
  Scale sc; Model m(sc); m.reset(9);
  m.setLen(0, 7); m.setLen(1, 4); m.setDejaVu(0, 0.35); m.setRange(1, 40, 80); m.setRing(1, DAMP, 0.9); m.setCloud(ECHO, 0.77); m.setPearlPitch(1, 1, 0.123);
  for (int i = 0; i < 5; i++) { m.next(0); m.next(1); }
  const std::string js = m.snapshot();
  Model b(sc); b.restore(js);
  CHECK(b.snapshot() == js);
  for (int i = 0; i < 30; i++) { const Step x = m.next(i & 1), y = b.next(i & 1); CHECK(x.hz == y.hz && x.hit == y.hit && x.ring == y.ring); }
}

static void modelRestoreGarbage() {
  Scale sc; Model m(sc); m.reset(1);
  const char* bad[] = {"{", "[]", "", "null", "{\"len\":\"x\"}", "{\"p\":5}", "{\"len\":99,\"dejavu\":9,\"p\":[1,2,{\"p\":\"a\"}]}", "{\"rings\":[[1,2]]}", "{\"thr\":[{\"len\":99,\"lo\":-5,\"hi\":\"x\",\"p\":[[2,1]]},5]}", "{\"len\":3,\"p\":[[0.5,0]]}"};
  for (const char* js : bad) {
    m.restore(js);
    for (int t = 0; t < NR; t++) {
      const Thread& th = m.thread(t);
      CHECK(th.len >= 1 && th.len <= PEARL_MAX);
      CHECK(th.dejavu >= 0 && th.dejavu <= 1);
      CHECK(th.lo >= NOTE_MIN && th.hi <= NOTE_MAX && th.hi - th.lo >= RANGE_MIN_W);
      for (int i = 0; i < PEARL_MAX; i++) CHECK(th.p[i].pitch >= 0 && th.p[i].pitch <= 1);
      for (int i = 0; i < 20; i++) m.next(t);
    }
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
  const Pearl keep = m.thread(1).p[0]; m.roll(0); CHECK(m.thread(1).p[0].pitch == keep.pitch);   // roll(0) нить 1 не трогает
  m.roll(1); CHECK(m.thread(1).p[0].pitch != keep.pitch);
  m.clear();
  for (int t = 0; t < NR; t++) for (int i = 0; i < PEARL_MAX; i++) CHECK(m.thread(t).p[i].rest);
  CHECK(!m.sounding());
}


// ---------- Task 2 (two rings): локации, mix, тембр ----------

static bool sameNotes(const Thread& a, const Thread& b) {
  if (a.len != b.len || a.dejavu != b.dejavu || a.lo != b.lo || a.hi != b.hi) return false;
  for (int i = 0; i < PEARL_MAX; i++) if (a.p[i].pitch != b.p[i].pitch || a.p[i].rest != b.p[i].rest) return false;
  return true;
}

static void modelLocSwapKeepsSound() {
  Scale sc; Model m(sc); m.reset(3);
  m.setRing(1, DAMP, 0.9); m.setCloud(ECHO, 0.77); m.setMix(0.8);
  double r[NR][NRK], c[NCK];
  for (int i = 0; i < NR; i++) for (int k = 0; k < NRK; k++) r[i][k] = m.ring(i, k);
  for (int k = 0; k < NCK; k++) c[k] = m.cloud(k);
  Model ref(sc); ref.reset(3);
  m.setLoc(1);
  CHECK(m.loc() == 1);
  for (int i = 0; i < NR; i++) for (int k = 0; k < NRK; k++) CHECK(m.ring(i, k) == r[i][k]);
  for (int k = 0; k < NCK; k++) CHECK(m.cloud(k) == c[k]);
  CHECK(m.mix() == 0.8);
  CHECK(!sameNotes(m.thread(0), ref.thread(0)));            // ноты стали другими, не прежними
}

static void modelLocKeepsEdits() {
  Scale sc; Model m(sc); m.reset(4);
  m.setDejaVu(0, 1); m.setDejaVu(1, 1);
  m.setPearlPitch(0, 2, 0.123); m.setRest(1, 3, true); m.setLen(0, 6);
  for (int i = 0; i < 4; i++) { m.next(0); m.next(1); }
  const int c0 = m.thread(0).cur, c1 = m.thread(1).cur;
  m.setLoc(1);
  CHECK(m.thread(0).cur == c0 % m.thread(0).len && m.thread(1).cur == c1 % m.thread(1).len);   // курсор не сброшен
  m.setLoc(0);
  CHECK(m.thread(0).p[2].pitch == 0.123 && m.thread(1).p[3].rest && m.thread(0).len == 6);
  // быстрые повторные переключения без потерь
  m.setPearlPitch(0, 5, 0.42);
  m.setLoc(2); m.setLoc(2); m.setLoc(3); m.setLoc(0);
  CHECK(m.thread(0).p[5].pitch == 0.42 && m.thread(0).p[2].pitch == 0.123);
}

static void modelLocOutOfRange() {
  Scale sc; Model m(sc); m.reset(5);
  m.setLoc(2); const std::string a = m.snapshot();
  m.setLoc(-1); m.setLoc(4); m.setLoc(1000);
  CHECK(m.loc() == 2); CHECK(m.snapshot() == a);
}

static void modelLocsDiffer() {
  Scale sc; Model m(sc); m.reset(6);
  Thread t[NLOC][NR];
  for (int l = 0; l < NLOC; l++) { m.setLoc(l); for (int k = 0; k < NR; k++) t[l][k] = m.thread(k); }
  for (int a = 0; a < NLOC; a++) for (int b = a + 1; b < NLOC; b++) for (int k = 0; k < NR; k++) CHECK(!sameNotes(t[a][k], t[b][k]));
}

static void modelDiceKeepsTimbre() {
  Scale sc; Model m(sc); m.reset(7);
  m.setRing(0, STRUCT, 0.2); m.setCloud(POS, 0.9); m.setMix(0.3);
  double r[NR][NRK], c[NCK];
  for (int i = 0; i < NR; i++) for (int k = 0; k < NRK; k++) r[i][k] = m.ring(i, k);
  for (int k = 0; k < NCK; k++) c[k] = m.cloud(k);
  m.roll(); m.roll(0); m.roll(1);
  for (int i = 0; i < NR; i++) for (int k = 0; k < NRK; k++) CHECK(m.ring(i, k) == r[i][k]);
  for (int k = 0; k < NCK; k++) CHECK(m.cloud(k) == c[k]);
  CHECK(m.mix() == 0.3);
}

static void modelTimbreKeepsNotes() {
  Scale sc; Model m(sc); m.reset(8);
  m.setRing(0, LVL, 0.33); m.setRing(1, POLY, 1); m.setCloud(SEND, 0.21); m.setCloud(FREEZE, 1); m.setCloud(CLEVEL, 0.44); m.setCloud(ECHO, 0.12); m.setMix(0.66);
  const Thread t0 = m.thread(0), t1 = m.thread(1);
  double r[NR][NRK], c[NCK];
  for (int i = 0; i < NR; i++) for (int k = 0; k < NRK; k++) r[i][k] = m.ring(i, k);
  for (int k = 0; k < NCK; k++) c[k] = m.cloud(k);
  bool changed = false;
  for (int n = 0; n < 20; n++) {
    m.rollTimbre();
    CHECK(sameNotes(m.thread(0), t0) && sameNotes(m.thread(1), t1));
    CHECK(m.thread(0).cur == t0.cur);
    for (int i = 0; i < NR; i++) {
      CHECK(m.ring(i, LVL) == r[i][LVL] && m.ring(i, POLY) == r[i][POLY]);
      for (int k : {int(STRUCT), int(BRIGHT), int(DAMP)}) { CHECK(m.ring(i, k) >= 0 && m.ring(i, k) <= 1); if (m.ring(i, k) != r[i][k]) changed = true; }
    }
    for (int k : {int(SEND), int(FREEZE), int(CLEVEL), int(ECHO)}) CHECK(m.cloud(k) == c[k]);
    for (int k : {int(POS), int(SIZE), int(DENS), int(PITCH)}) { CHECK(m.cloud(k) >= 0 && m.cloud(k) <= 1); if (m.cloud(k) != c[k]) changed = true; }
    CHECK(m.mix() == 0.66);
  }
  CHECK(changed);
}

static void modelMixClamp() {
  Scale sc; Model m(sc);
  CHECK(m.mix() == 0.5);
  m.setMix(0.3); m.setMix(NAN); CHECK(m.mix() == 0.3);
  m.setMix(2); CHECK(m.mix() == 1);
  m.setMix(-1); CHECK(m.mix() == 0);
  m.setMix(INFINITY); CHECK(m.mix() == 1);
  m.setMix(-INFINITY); CHECK(m.mix() == 0);
}

static void modelLocSnapshotRoundTrip() {
  Scale sc; Model m(sc); m.reset(11);
  m.setMix(0.77); m.setPearlPitch(0, 1, 0.31); m.setLoc(2); m.setPearlPitch(1, 4, 0.66); m.setLen(0, 9);
  for (int i = 0; i < 5; i++) { m.next(0); m.next(1); }
  const std::string js = m.snapshot();
  Model b(sc); b.restore(js);
  CHECK(b.snapshot() == js);
  CHECK(b.loc() == 2 && b.mix() == 0.77);
  b.setLoc(0); CHECK(b.thread(0).p[1].pitch == 0.31);
  b.setLoc(2); CHECK(b.thread(1).p[4].pitch == 0.66 && b.thread(0).len == 9);
}

static void modelLocRestoreGarbage() {
  Scale sc; Model m(sc);
  const char* bad[] = {"", "{}", "{\"p\":[[0.5,0]],\"len\":3}", "{\"thr\":5,\"loc\":7,\"curLoc\":\"x\",\"mix\":[]}",
    "{\"thr\":[],\"loc\":[1,2,[3],[[1],{}]],\"curLoc\":99,\"mix\":9}",
    "{\"thr\":[{}],\"loc\":[[{\"len\":99,\"lo\":-5,\"hi\":\"x\",\"p\":5},7],[],[[]],{}],\"curLoc\":-3,\"mix\":null}",
    "{\"thr\":[{}],\"loc\":[[{},{}],[{},{}],[{},{}],[{},{}]],\"curLoc\":1.5,\"mix\":-4}"};
  for (const char* js : bad) {
    m.restore(js);
    CHECK(m.loc() >= 0 && m.loc() < NLOC); CHECK(m.mix() >= 0 && m.mix() <= 1);
    for (int l = 0; l < NLOC; l++) {
      m.setLoc(l);
      for (int t = 0; t < NR; t++) {
        const Thread& th = m.thread(t);
        CHECK(th.len >= 1 && th.len <= PEARL_MAX); CHECK(th.cur >= 0 && th.cur < th.len);
        CHECK(th.lo >= NOTE_MIN && th.hi <= NOTE_MAX && th.hi - th.lo >= RANGE_MIN_W);
        for (int i = 0; i < PEARL_MAX; i++) CHECK(th.p[i].pitch >= 0 && th.p[i].pitch <= 1);
        for (int i = 0; i < 20; i++) m.next(t);
      }
    }
  }
  m.restore("");                                          // пустое — рабочий дефолт
  CHECK(m.snapshot() == Model(sc).snapshot());
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
  double rg[8] = {};
  const WRun r = runWorld(w, 4 * 48000 / 128, nullptr, rg);
  CHECK(rg[0] >= 15 && rg[0] <= 17);                                      // 120 уд/мин, восьмые: 4 шага в секунду (нить 0 — кольцо 0)
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
  CHECK(r.maxPerBlock <= 4 * NR); CHECK(r.finite);
  clk.grid.p = -1; const WRun r2 = runWorld(w, 100, &clk); CHECK(r2.maxPerBlock <= 4 * NR && r2.finite);
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
  act(w, hymenaeus::RING, 1, {2, 0.9}); act(w, hymenaeus::CLOUD, 0, {5, 0.6});
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
  for (int r = 0; r < NR; r++) act(w, hymenaeus::RING, r, {2, 0.1});       // damping 0.1: звенят долго
  for (int i = 0; i < hymenaeus::PEARL_MAX; i++) act(w, hymenaeus::PEARL, i, {1, 0});
  runWorld(w, 48000 * 2 / 128);                                            // разогнать
  const int blocks = 48000 * 4 / 128;
  const auto t0 = std::chrono::steady_clock::now();
  runWorld(w, blocks);
  const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), audio = blocks * 128.0 / 48000;
  CHECK(sec < audio * 0.25);
  std::printf("  budget: %.2f%% of real time\n", 100 * sec / audio);
}

static void worldBothRingsStep() {
  Scale sc; auto& w = theWorld(sc, 3);
  act(w, hymenaeus::BPM, 0, {240}); act(w, hymenaeus::RUN, 0, {1});
  double rings[8] = {};
  runWorld(w, 48000 * 3 / 128, nullptr, rings);
  CHECK(rings[0] > 3 && rings[1] > 3);                                     // каждая нить бьёт своё кольцо
  CHECK(rings[2] == 0);
}


// ---------- Task 4b: режимы колец и полифония ----------

static void modelRingModePoly() {
  Scale sc; Model m(sc);
  CHECK(NRK == 5); CHECK(m.ring(0, POLY) == 0); CHECK(m.ring(1, POLY) == 0);
  m.setRing(1, POLY, 0.5); CHECK(m.ring(1, POLY) == 0.5);
  m.setRing(1, 5, 1); CHECK(m.ring(1, 5) == 0);                              // ключа MODE больше нет
  m.setRing(1, POLY, -3); CHECK(m.ring(1, POLY) == 0);
  m.setRing(1, POLY, NAN); CHECK(m.ring(1, POLY) == 0);
  CHECK(polyOf(0) == 1 && polyOf(0.5) == 2 && polyOf(1) == 4 && polyOf(0.3) == 1 && polyOf(0.34) == 2 && polyOf(0.67) == 4);
  m.setRing(0, POLY, 1);
  Model b(sc); b.restore(m.snapshot());
  CHECK(b.snapshot() == m.snapshot()); CHECK(b.ring(0, POLY) == 1);
  Model g(sc); g.restore("{\"thr\":[{},{}],\"rings\":[[0.5,0.5,0.5,0.8],[1,2,3,4,5,6,7,8],\"x\"]}");   // короткий ряд ключей и лишние ключи
  CHECK(g.ring(0, STRUCT) == 0.5 && g.ring(0, POLY) == 0 && g.ring(1, POLY) == 1);
  Model o(sc); o.reset(2);
  o.restore("{\"len\":3,\"dejavu\":0.2,\"p\":[[0.5,0]],\"rings\":[[1,1,1,1,1,1]]}");
  Model def(sc); CHECK(o.snapshot() == def.snapshot());                       // старый одно-нитевой формат и пустая строка — дефолт
  o.restore(""); CHECK(o.snapshot() == def.snapshot());
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
  s.setRing(1, 0.5, 0.5, 0.4, 0.8, 1, 1.0);                // strings, poly 4 (по 2 струны)
  float mx = 0;
  for (int k = 0; k < 6; k++) { s.strike(1, 300 + 40 * k, 0.3, 0.8); const SRun r = renderSound(s, 4800); CHECK(r.finite); mx = std::max(mx, float(r.peak)); }
  CHECK(mx > 0.01); CHECK(mx <= 1.0001);
  s.setRing(1, 0.5, 0.5, 0.4, 0.8, 1, 0.5);                // смена полифонии на ходу
  s.strike(1, 330, 0.3, 0.8);
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
  CHECK(MIRROR_RING == 7); CHECK(MIRROR_N == 5 + 32 + NR * 7 + NCK + 1); CHECK(MIRROR_N == 60);
  Scale sc; auto& w = theWorld(sc);
  act(w, hymenaeus::RING, 1, {4, 0.5});
  double m[MIRROR_N]; CHECK(w.mirror(m, MIRROR_N) == MIRROR_N);
  const int base = 5 + 32 + 1 * 7;
  CHECK(m[base + 4] == 1); CHECK(m[base + 5] == 0.5); CHECK(m[base + 6] == 0);          // mode (временно: номер кольца), poly, energy
  act(w, hymenaeus::RING, 1, {5, 1}); CHECK(w.mirror(m, MIRROR_N) == MIRROR_N); CHECK(m[base + 5] == 0.5);   // ключ 5 вне диапазона
  act(w, sdk::ACT_LFO, -1, {double(NRK + 4), 0.75});                                  // LFO-ключ кольца 1, POLY
  w.mirror(m, MIRROR_N); CHECK(m[base + 5] == 0.75);
}

int main() {
  modelSeedDeterministic(); modelDejaVuOneRepeats(); modelDejaVuZeroVaries(); modelRestsSilent();
  modelRingsCount(); modelDefaultRanges(); modelTwoThreadsIndependent(); modelRangeClamp(); modelPitchWithinRange(); modelDeterministic(); modelScaleSnap(); modelScaleSnapLowRange(); modelClamps(); modelLenOne();
  modelSnapshotRoundTrip(); modelRestoreGarbage(); modelRollAndClear(); modelCloudSend();
  modelLocSwapKeepsSound(); modelLocKeepsEdits(); modelLocOutOfRange(); modelLocsDiffer(); modelDiceKeepsTimbre(); modelTimbreKeepsNotes(); modelMixClamp(); modelLocSnapshotRoundTrip(); modelLocRestoreGarbage();
  ringsRingsDown(); ringsDampingMonotone(); ringsBrightnessCentroid(); ringsStable(); ringsIdleCheap(); ringsNoAlloc(); ringsPluckerBurst();
  cloudSilentIn(); cloudEchoesInput(); cloudFreezeHolds(); cloudFreezeEmpty(); cloudEchoDecays(); cloudNoNan(); cloudDensityBounded(); cloudSleepsAgain();
  worldSilentWhenStopped(); worldStepsInTime(); worldStrikeSounds(); worldNoStepFlood(); worldStopNoStuck(); worldSnapshotRoundTrip();
  worldGarbageActions(); worldBudget(); worldBothRingsStep();
  modelRingModePoly(); soundStringsSound(); soundStructureGlide(); soundPolyVoices(); soundPolyStrings(); soundModeSwitchLive(); worldMirrorSize();
  std::printf("hymenaeus-test: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
