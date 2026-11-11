// Тесты мира Hymenaeus (XIV): модель, кольца, облака, мир. Сборка и запуск — core/build.sh → core/build/hymenaeus-test.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <string>
#include <vector>
#include "../sdk/euclid.hpp"

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

static void modelLoopRepeats() {
  Scale sc; Model m(sc); m.reset(3); 
  const int len = m.thread(0).len;
  std::vector<double> first;
  for (int i = 0; i < len; i++) first.push_back(m.next(0).hz);
  for (int round = 0; round < 3; round++)
    for (int i = 0; i < len; i++) CHECK(m.next(0).hz == first[i]);
}

static void modelRestsSilent() {
  Scale sc; Model m(sc); m.reset(2); 
  for (int i = 0; i < PEARL_MAX; i++) for (int t = 0; t < NR; t++) m.setRest(t, i, true);
  CHECK(!m.sounding());
  for (int i = 0; i < m.thread(0).len * 3; i++) CHECK(!m.next(0).hit);
  m.togglePearl(0, 0); CHECK(m.thread(0).p[0].rest == false); CHECK(m.sounding());
  m.togglePearl(0, 0); CHECK(m.thread(0).p[0].rest == true);
}

static void modelRingsCount() { CHECK(NR == 2); CHECK(NRK == 5); CHECK(NLOC == 4); }

static void modelDefaultRanges() {
  Scale sc; Model m(sc);
  CHECK(m.thread(0).lo == 47 && m.thread(0).hi == 67);
  CHECK(m.thread(1).lo == 59 && m.thread(1).hi == 79);
}

static void modelTwoThreadsIndependent() {
  Scale sc; Model m(sc); m.reset(1); 
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
  CHECK(m.thread(1).lo == 59 && m.thread(1).hi == 79);
}

static void modelPitchWithinRange() {
  Scale sc; Model m(sc); m.reset(3);
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
  Model m(sc); m.reset(7); 
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
  }
  m.setLen(-1, 3); m.setLen(NR, 3); m.togglePearl(NR, 0); m.setRest(-1, 0, true); m.setPearlPitch(NR, 0, 1);
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
  m.setLen(0, 7); m.setLen(1, 4); m.setRange(1, 40, 80); m.setRing(1, DAMP, 0.9); m.setCloud(ECHO, 0.77); m.setPearlPitch(1, 1, 0.123);
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
  const auto sig = [&](int t) { std::string r; for (int i = 0; i < PEARL_MAX; i++) r += std::to_string(m.thread(t).p[i].pitch) + (m.thread(t).p[i].rest ? "r" : "n"); return r; };
  const std::string keep = sig(1), keep0 = sig(0); m.roll(0); CHECK(sig(1) == keep && sig(0) != keep0);   // roll(0) нить 1 не трогает
  m.roll(1); CHECK(sig(1) != keep);
  m.clearAll();
  for (int t = 0; t < NR; t++) for (int i = 0; i < PEARL_MAX; i++) CHECK(m.thread(t).p[i].rest);
  CHECK(!m.sounding());
}

static void modelClearAllIsEmpty() {
  Scale sc; Model m(sc); m.reset(7);
  m.setRing(0, STRUCT, 0.9); m.setRing(1, POLY, 1); m.setCloud(ECHO, 0.9); m.setCloud(FREEZE, 1); m.setMix(0.1);
  m.setLen(1, 4); m.setRange(0, 30, 90);
  for (int l : {2, 1, 3, 0}) { m.setLoc(l); m.setRest(0, 3, false); m.setRest(1, 5, false); m.setPearlPitch(0, 1, 0.9); }
  m.setLoc(2);
  for (int i = 0; i < 5; i++) { m.next(0); m.next(1); }
  m.clearAll();
  Model def(sc); def.reset(7);
  CHECK(m.loc() == 0); CHECK(!m.sounding());
  for (int l = 0; l < NLOC; l++) {
    m.setLoc(l);
    for (int t = 0; t < NR; t++) {
      const Thread& th = m.thread(t);
      for (int i = 0; i < PEARL_MAX; i++) CHECK(th.p[i].rest);
      CHECK(th.len == PEARL_DEF && th.cur == 0);
      CHECK(th.lo == def.thread(t).lo && th.hi == def.thread(t).hi);
    }
    CHECK(!m.sounding());
  }
  m.setLoc(0);
  for (int r = 0; r < NR; r++) for (int k = 0; k < NRK; k++) CHECK(m.ring(r, k) == def.ring(r, k));
  for (int k = 0; k < NCK; k++) CHECK(m.cloud(k) == def.cloud(k));
  CHECK(m.mix() == 0.5);
  for (int t = 0; t < NR; t++) for (int i = 0; i < 40; i++) CHECK(!m.next(t).hit);   // после очистки пустое остаётся пустым: ход нити не перебрасывает
}

static void modelClearUndoRestores() {
  Scale sc; Model m(sc); m.reset(9);
  m.setRing(1, DAMP, 0.2); m.setCloud(FREEZE, 1); m.setMix(0.8); m.setLoc(3); m.setRest(0, 2, false);
  for (int i = 0; i < 7; i++) m.next(0);
  const std::string s = m.snapshot();
  m.clearAll(); CHECK(m.snapshot() != s);
  m.restore(s); CHECK(m.snapshot() == s);
}

static void modelClearTwice() {
  Scale sc; Model m(sc); m.reset(11);
  m.setLoc(2); m.setCloud(ECHO, 0.9);
  m.clearAll(); const std::string a = m.snapshot();
  m.clearAll(); CHECK(m.snapshot() == a);
}


// ---------- Task 2 (two rings): локации, mix, тембр ----------

static bool sameNotes(const Thread& a, const Thread& b) {
  if (a.len != b.len || a.lo != b.lo || a.hi != b.hi) return false;
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

static void modelTimbreDoesNotShiftMelody() {      // кость timbre не двигает общий rng: будущие мелодии те же
  Scale sc; Model a(sc), b(sc); a.reset(9); b.reset(9);
  for (int n = 0; n < 5; n++) b.rollTimbre();
  a.roll(); b.roll();
  CHECK(sameNotes(a.thread(0), b.thread(0)) && sameNotes(a.thread(1), b.thread(1)));
  for (int i = 0; i < 40; i++) { const Step x = a.next(0), y = b.next(0); CHECK(x.hit == y.hit && x.hz == y.hz); }
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
    for (int i = 0; i < hymenaeus::PEARL_MAX; i++) act(w, hymenaeus::PEARL, i, {0, 1, 0});   // ни одной паузы
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
  for (int i = 0; i < hymenaeus::PEARL_MAX; i++) act(w, hymenaeus::PEARL, i, {0, 1, 0});
  runWorld(w, 48000 * 2 / 128);                                            // разогнать
  const int blocks = 48000 * 4 / 128;
  const auto t0 = std::chrono::steady_clock::now();
  runWorld(w, blocks);
  const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), audio = blocks * 128.0 / 48000;
  CHECK(sec < audio * 0.25);
  std::printf("  budget: %.2f%% of real time\n", 100 * sec / audio);
}

static void worldClearSilences() {
  Scale sc; auto& w = theWorld(sc, 5);
  act(w, hymenaeus::BPM, 0, {240}); act(w, hymenaeus::RUN, 0, {1});
  for (int r = 0; r < NR; r++) act(w, hymenaeus::RING, r, {2, 0.3});
  runWorld(w, 48000 * 2 / 128);
  CHECK(w.sounding());
  act(w, hymenaeus::STRIKE, 0, {0.3, 1}); act(w, hymenaeus::STRIKE, 1, {0.3, 1});   // ручные удары ждут блока
  act(w, hymenaeus::CLEAR, 0, {});                                         // ход не останавливаем
  const int half = 48000 / 2 / 128;
  const WRun first = runWorld(w, half * 3);                                // 1.5 с
  const WRun last = runWorld(w, half);                                     // последние 0.5 с из 2 с
  std::printf("  clear: peak 0-1.5 s %.3g, 1.5-2 s %.3g\n", first.peak, last.peak);
  CHECK(first.finite && last.finite);
  CHECK(first.heard == 0 && last.heard == 0);
  CHECK(last.peak < 1e-4);
  const WRun more = runWorld(w, 48000 * 6 / 128);                          // всего 8 с с идущим ходом: ни одного удара
  CHECK(more.heard == 0 && more.peak < 1e-4);
  CHECK(!w.sounding());
  CHECK(!w.model().sounding());
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

// ---------- Task 4 (two rings): Act, mix, зеркало ----------

// эталонный сценарий: seed 5, bpm 240, оба кольца бьются нитями, облака с эхом; 1 с (375 блоков)
struct MixRef { double peakL = 0, peakR = 0, rmsL = 0, rmsR = 0, peakS = 0; bool finite = true; };
static MixRef mixScenario(hymenaeus::World& w, int blocks) {
  static float L[128], R[128], sL[128], sR[128];
  sdk::Out out{L, R, sL, sR};
  MixRef r; double eL = 0, eR = 0;
  for (int b = 0; b < blocks; b++) {
    w.process(out, 128);
    for (int i = 0; i < 128; i++) {
      for (float x : {L[i], R[i], sL[i], sR[i]}) if (!std::isfinite(x)) r.finite = false;
      r.peakL = std::max(r.peakL, double(std::fabs(L[i]))); r.peakR = std::max(r.peakR, double(std::fabs(R[i])));
      r.peakS = std::max(r.peakS, double(std::max(std::fabs(sL[i]), std::fabs(sR[i]))));
      eL += double(L[i]) * L[i]; eR += double(R[i]) * R[i];
    }
  }
  r.rmsL = std::sqrt(eL / (blocks * 128.0)); r.rmsR = std::sqrt(eR / (blocks * 128.0));
  return r;
}
static void mixSetup(hymenaeus::World& w) {
  act(w, hymenaeus::BPM, 0, {240}); act(w, hymenaeus::RUN, 0, {1});
  act(w, hymenaeus::CLOUD, 0, {5, 0.6}); act(w, hymenaeus::CLOUD, 0, {2, 0.7});
}

// Эталон: mixSetup + 375 блоков, seed 5 (theWorld(sc, 5)). Первый снят с коммита 99105d1 (до mix: звук без него); после музыкального генератора и после кубика Евклида
// (мелодии другие) переснят — mix 0.5 при этом звучит как раньше: сухое/влажное в этом положении по формуле равны 1.
static const MixRef MIX_REF = {0.800939202, 0.907557428, 0.119531089, 0.118977414, 0.317645103, true};

static std::vector<double> mirrorOf(hymenaeus::World& w) { std::vector<double> m(MIRROR_N + 4, -777); CHECK(w.mirror(m.data(), MIRROR_N) == MIRROR_N); return m; }
// смещения зеркала (см. world.hpp)
constexpr int M_RUN = 0, M_BPM = 1, M_LOC = 2, M_MIX = 3, M_CUR = 4, M_LEN = 6, M_LO = 8, M_HI = 10, M_PEARL = 12,
              M_RING = M_PEARL + 2 * 2 * PEARL_MAX, M_CLOUD = M_RING + NR * 6, M_CENV = M_CLOUD + NCK;

static void worldActRouting() {
  CHECK(hymenaeus::RING == 0 && hymenaeus::CLOUD == 1 && hymenaeus::PEARL == 2 && hymenaeus::STRIKE == 3 && hymenaeus::THREAD == 4 && hymenaeus::DICE == 5
        && hymenaeus::CLEAR == 6 && hymenaeus::RUN == 7 && hymenaeus::BPM == 8 && hymenaeus::TIMBRE == 9 && hymenaeus::LOC == 10 && hymenaeus::MIX == 11);
  Scale sc; auto& w = theWorld(sc, 3);
  act(w, hymenaeus::THREAD, 1, {1, 50}); CHECK(w.model().thread(1).lo == 50 && w.model().thread(0).lo == 47);
  act(w, hymenaeus::THREAD, 1, {2, 80}); CHECK(w.model().thread(1).hi == 80);
  act(w, hymenaeus::THREAD, 1, {0, 4}); CHECK(w.model().thread(1).len == 4 && w.model().thread(0).len == PEARL_DEF);
  act(w, hymenaeus::PEARL, 3, {1, 0, 0.77}); CHECK(w.model().thread(1).p[3].pitch == 0.77 && w.model().thread(0).p[3].pitch != 0.77);
  act(w, hymenaeus::PEARL, 3, {1, 1, 1}); CHECK(w.model().thread(1).p[3].rest);
  act(w, hymenaeus::PEARL, 3, {1, 1, 0}); CHECK(!w.model().thread(1).p[3].rest);
  act(w, hymenaeus::PEARL, 2, {0, 1, 1}); CHECK(w.model().thread(0).p[2].rest);
  act(w, hymenaeus::RING, 1, {0, 0.1}); CHECK(w.model().ring(1, STRUCT) == 0.1);
  act(w, hymenaeus::CLOUD, 0, {3, 0.9}); CHECK(w.model().cloud(PITCH) == 0.9);
  act(w, hymenaeus::MIX, 0, {0.8}); CHECK(w.model().mix() == 0.8);
  act(w, hymenaeus::LOC, 0, {2}); CHECK(w.model().loc() == 2);
  act(w, hymenaeus::LOC, 0, {9}); CHECK(w.model().loc() == 2);
  act(w, hymenaeus::BPM, 0, {120}); act(w, hymenaeus::RUN, 0, {1});
  { auto m = mirrorOf(w); CHECK(m[M_RUN] == 1 && m[M_BPM] == 120 && m[M_LOC] == 2 && m[M_MIX] == 0.8); }
  const double rs0 = w.model().ring(0, STRUCT), cs0 = w.model().cloud(POS), cur = w.model().ring(1, LVL), lvl = w.model().ring(0, LVL), snd = w.model().cloud(SEND);
  act(w, hymenaeus::TIMBRE, 0, {});
  CHECK(w.model().ring(0, STRUCT) != rs0 || w.model().cloud(POS) != cs0);
  CHECK(w.model().ring(1, LVL) == cur && w.model().ring(0, LVL) == lvl && w.model().cloud(SEND) == snd);
  act(w, hymenaeus::CLEAR, 0, {}); CHECK(!w.model().sounding());
  // неверные индексы и аргументы — тихо игнорируются
  const std::string before = w.model().snapshot();
  act(w, hymenaeus::PEARL, 99, {0, 0, 1}); act(w, hymenaeus::PEARL, -1, {0, 0, 1}); act(w, hymenaeus::PEARL, 1, {2, 0, 1}); act(w, hymenaeus::PEARL, 1, {-1, 1, 1});
  act(w, hymenaeus::PEARL, 1, {0, 5, 1}); act(w, hymenaeus::PEARL, 1, {0, 0}); act(w, hymenaeus::PEARL, 1, {0, 0, NAN});
  act(w, hymenaeus::THREAD, 2, {2, 50}); act(w, hymenaeus::THREAD, -1, {2, 50}); act(w, hymenaeus::THREAD, 0, {9, 1}); act(w, hymenaeus::THREAD, 0, {2, NAN});
  act(w, hymenaeus::LOC, 0, {-1}); act(w, hymenaeus::LOC, 0, {NAN}); act(w, hymenaeus::LOC, 0, {}); act(w, hymenaeus::MIX, 0, {NAN}); act(w, hymenaeus::MIX, 0, {});
  CHECK(w.model().snapshot() == before);
}

static std::string threadSig(const hymenaeus::World& w, int t) {
  std::string r; for (int i = 0; i < PEARL_MAX; i++) r += std::to_string(w.model().thread(t).p[i].pitch) + (w.model().thread(t).p[i].rest ? "r" : "n"); return r;
}
static void worldDiceBothOrOne() {
  Scale sc; auto& w = theWorld(sc, 4);
  const std::string a0 = threadSig(w, 0), b0 = threadSig(w, 1);
  act(w, hymenaeus::DICE, 0, {0});
  CHECK(threadSig(w, 0) != a0); CHECK(threadSig(w, 1) == b0);
  const std::string a1 = threadSig(w, 0);
  act(w, hymenaeus::DICE, 0, {1});
  CHECK(threadSig(w, 0) == a1); CHECK(threadSig(w, 1) != b0);
  const std::string a2 = threadSig(w, 0), b2 = threadSig(w, 1);
  act(w, hymenaeus::DICE, 0, {-1});
  CHECK(threadSig(w, 0) != a2 && threadSig(w, 1) != b2);
  const std::string a3 = threadSig(w, 0), b3 = threadSig(w, 1);
  act(w, hymenaeus::DICE, 0, {});
  CHECK(threadSig(w, 0) != a3 && threadSig(w, 1) != b3);
  const std::string s = w.model().snapshot();
  act(w, hymenaeus::DICE, 0, {2}); act(w, hymenaeus::DICE, 0, {99}); act(w, hymenaeus::DICE, 0, {-5}); act(w, hymenaeus::DICE, 0, {NAN}); act(w, hymenaeus::DICE, 0, {INFINITY}); act(w, hymenaeus::DICE, 0, {-INFINITY});
  CHECK(w.model().snapshot() == s);                                        // вне диапазона — ничего (ни одной нити, ни обеих)
}

static void worldMixSignal() {
  Scale sc;
  { auto& w = theWorld(sc, 5); mixSetup(w); act(w, hymenaeus::MIX, 0, {0.5});     // 0.5 — как до изменения
    const MixRef r = mixScenario(w, 375);
    CHECK(r.finite);
    CHECK(std::fabs(r.peakL - MIX_REF.peakL) < 1e-4 && std::fabs(r.peakR - MIX_REF.peakR) < 1e-4);
    CHECK(std::fabs(r.rmsL - MIX_REF.rmsL) < 1e-4 && std::fabs(r.rmsR - MIX_REF.rmsR) < 1e-4);
    CHECK(std::fabs(r.peakS - MIX_REF.peakS) < 1e-4); }
  { auto& w = theWorld(sc, 5); mixSetup(w); act(w, hymenaeus::MIX, 0, {0});        // 0: облаков нет, сухое то же
    const MixRef r0 = mixScenario(w, 375);
    CHECK(r0.finite && r0.rmsL > 0.01);
    CHECK(r0.peakL <= MIX_REF.peakL + 1e-3 || r0.rmsL <= MIX_REF.rmsL + 1e-3); }
  { auto& w = theWorld(sc, 5); mixSetup(w); act(w, hymenaeus::MIX, 0, {1});        // 1: сухого нет, звучат облака
    const MixRef r1 = mixScenario(w, 375);
    CHECK(r1.finite); CHECK(r1.rmsL < MIX_REF.rmsL); }
  // чистая проверка: облака без входа → при mix 0 ровно сухое; сухое в mix 1 = 0 пока облака пусты
  { auto& w = theWorld(sc, 5); mixSetup(w); act(w, hymenaeus::MIX, 0, {1}); act(w, hymenaeus::CLOUD, 0, {6, 0});   // CLEVEL 0 и mix 1 → полная тишина
    act(w, hymenaeus::CLOUD, 0, {7, 0});
    const MixRef r = mixScenario(w, 100); CHECK(r.peakL == 0 && r.peakR == 0 && r.peakS == 0); }
  { auto& w = theWorld(sc, 5); mixSetup(w); act(w, hymenaeus::MIX, 0, {0}); act(w, hymenaeus::CLOUD, 0, {6, 1}); act(w, hymenaeus::CLOUD, 0, {7, 1});
    const MixRef rA = mixScenario(w, 375);
    auto& w2 = theWorld(sc, 5); mixSetup(w2); act(w2, hymenaeus::MIX, 0, {0}); act(w2, hymenaeus::CLOUD, 0, {6, 0}); act(w2, hymenaeus::CLOUD, 0, {7, 0});
    const MixRef rB = mixScenario(w2, 375);
    CHECK(std::fabs(rA.rmsL - rB.rmsL) < 1e-6 && std::fabs(rA.peakL - rB.peakL) < 1e-6); }   // mix 0: облака (clevel, send) на выход не влияют
}

static void worldMixNoNaN() {
  Scale sc;
  for (double v : {0.0, 1.0, 0.5, -3.0, 9.0, double(NAN), double(INFINITY), -double(INFINITY)}) {
    auto& w = theWorld(sc, 6); mixSetup(w); act(w, hymenaeus::MIX, 0, {v});
    const MixRef r = mixScenario(w, 200);
    CHECK(r.finite); CHECK(r.peakL <= 1.0 + 1e-6 && r.peakR <= 1.0 + 1e-6);
    const double mx = w.model().mix(); CHECK(mx >= 0 && mx <= 1);
  }
}

// считает операторы new/delete в аудио-блоке
static long gAllocs = 0; static bool gCount = false;
void* operator new(std::size_t n) { if (gCount) gAllocs++; void* p = std::malloc(n ? n : 1); if (!p) throw std::bad_alloc(); return p; }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

static void worldNoAllocInProcess() {
  Scale sc; auto& w = theWorld(sc, 5); mixSetup(w); act(w, hymenaeus::MIX, 0, {0.3}); act(w, hymenaeus::LOC, 0, {1}); act(w, hymenaeus::RING, 1, {4, 1});
  runWorld(w, 4);
  static float L[128], R[128], sL[128], sR[128]; sdk::Out out{L, R, sL, sR};
  double hb[64];
  gAllocs = 0; gCount = true;
  for (int b = 0; b < 400; b++) {
    if (b == 100) { const double a[1] = {0.9}; w.action(hymenaeus::MIX, 0, a, 1); }
    w.process(out, 128); w.heard(hb, 64);
  }
  gCount = false;
  CHECK(gAllocs == 0);
}

static void worldMirrorLayout() {
  CHECK(hymenaeus::MIRROR_HEAD == 12); CHECK(hymenaeus::MIRROR_RING == 6);
  CHECK(MIRROR_N == 12 + 2 * 2 * 16 + 2 * 6 + 8 + 1); CHECK(MIRROR_N == 97);
  CHECK(M_CENV == MIRROR_N - 1);
  Scale sc; auto& w = theWorld(sc, 8);
  act(w, hymenaeus::MIX, 0, {0.25}); act(w, hymenaeus::LOC, 0, {3});
  act(w, hymenaeus::THREAD, 1, {0, 6}); act(w, hymenaeus::THREAD, 0, {1, 40}); act(w, hymenaeus::THREAD, 0, {2, 70});
  act(w, hymenaeus::PEARL, 4, {1, 0, 0.6}); act(w, hymenaeus::PEARL, 4, {1, 1, 0}); act(w, hymenaeus::PEARL, 5, {1, 1, 1}); act(w, hymenaeus::RING, 1, {1, 0.42}); act(w, hymenaeus::CLOUD, 0, {7, 0.12});
  const auto m = mirrorOf(w);
  const Model& md = w.model();
  CHECK(m[M_LOC] == 3 && m[M_MIX] == 0.25);
  for (int t = 0; t < NR; t++) {
    CHECK(m[M_CUR + t] == md.thread(t).cur); CHECK(m[M_LEN + t] == md.thread(t).len);
    CHECK(m[M_LO + t] == md.thread(t).lo); CHECK(m[M_HI + t] == md.thread(t).hi);
    for (int i = 0; i < PEARL_MAX; i++) { CHECK(m[M_PEARL + t * 2 * PEARL_MAX + 2 * i] == md.thread(t).p[i].pitch); CHECK(m[M_PEARL + t * 2 * PEARL_MAX + 2 * i + 1] == md.thread(t).p[i].rest); }
  }
  CHECK(m[M_LEN + 1] == 6 && m[M_LO] == 40 && m[M_HI] == 70);
  CHECK(m[M_PEARL + 2 * PEARL_MAX + 8] == 0.6 && m[M_PEARL + 2 * PEARL_MAX + 9] == 0 && m[M_PEARL + 2 * PEARL_MAX + 11] == 1);
  CHECK(m[M_RING + 6 + 1] == 0.42);                                      // кольцо 1, bright
  CHECK(m[M_CLOUD + SEND] == 0.12);
  for (int k = 0; k < MIRROR_N; k++) CHECK(std::isfinite(m[k]));
  CHECK(m[MIRROR_N] == -777);                                            // за пределы не пишет
}

static void worldLfoKeys() {
  Scale sc; auto& w = theWorld(sc, 2);
  act(w, sdk::ACT_LFO, -1, {5, 0.4}); CHECK(w.model().ring(1, STRUCT) == 0.4 && w.model().ring(0, STRUCT) != 0.4);   // key 5 → кольцо 1, ключ 0
  act(w, sdk::ACT_LFO, -1, {9, 0.6}); CHECK(w.model().ring(1, POLY) == 0.6);
  act(w, sdk::ACT_LFO, -1, {3, 0.2}); CHECK(w.model().ring(0, LVL) == 0.2);
  act(w, sdk::ACT_LFO, -1, {10, 0.9}); CHECK(w.model().cloud(POS) == 0.9);                                        // 10 → облако POS
  act(w, sdk::ACT_LFO, -1, {17, 0.15}); CHECK(w.model().cloud(SEND) == 0.15);
  const std::string s = w.model().snapshot();
  act(w, sdk::ACT_LFO, -1, {18, 0.5}); act(w, sdk::ACT_LFO, -1, {-1, 0.5}); act(w, sdk::ACT_LFO, 0, {5, 0.5}); act(w, sdk::ACT_LFO, -1, {NAN, 0.5});
  CHECK(w.model().snapshot() == s);
}

// ---------- Музыкальный генератор мелодий ----------

namespace gen {
struct Deg {
  const Scale& sc; const int* st;
  explicit Deg(const Scale& s) : sc(s), st(Scale::stepsOf(s.idx)) {}
  static int fdiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
  int degOf(int note) const { const int x = note - 33 - sc.root, o = fdiv(x, 12), pc = x - 12 * o; int k = 0; for (int j = 0; j < 7; j++) if (st[j] <= pc) k = j; return 7 * o + k; }
  int noteOf(int g) const { const int o = fdiv(g, 7); return 33 + sc.root + 12 * o + st[g - 7 * o]; }
  bool inScale(int note) const { return noteOf(degOf(note)) == note; }
  int noteAt(const Thread& th, int i) const { return int(std::lround(th.lo + th.p[i].pitch * (th.hi - th.lo))); }
  int gAt(const Thread& th, int i) const { return degOf(noteAt(th, i)); }
  int role(const Thread& th, int i) const { return ((gAt(th, i) % 7) + 7) % 7; }
};
// Евклид: пауза/звучание нити на len жемчужин — ровно k ударов (k подбираем), жемчужины за len повторяют петлю
static int euclidK(const Thread& th) {
  for (int k = 1; k <= th.len; k++) {
    bool ok = true;
    for (int i = 0; i < th.len && ok; i++) ok = th.p[i].rest == !sdk::euclid(i, k, th.len);
    if (ok) return k;
  }
  return -1;
}
struct Stats {
  int steps = 0, small = 0, rests = 0, pearls = 0, outOfRange = 0, offScale = 0, badRhythm = 0, badTail = 0, kLo = 0, kHi = 0;
  int both = 0, cons = 0, seeds = 0;
};
static void collect(const Scale& sc, int seed, bool reroll, Stats* sts) {
  Model m(sc); m.reset(seed); if (reroll) m.roll();
  const Deg D(sc);
  for (int t = 0; t < NR; t++) {
    Stats& st = sts[t]; st.seeds++;
    const Thread& th = m.thread(t);
    const int len = th.len, k = euclidK(th);
    if (k < 0) st.badRhythm++; else if (k < 3) st.kLo++; else if (k > len - 2) st.kHi++;
    for (int i = 0; i < PEARL_MAX; i++) {
      const int n = D.noteAt(th, i);
      if (n < th.lo || n > th.hi) st.outOfRange++;
      if (sc.on && !D.inScale(n)) st.offScale++;
      if (i >= len && (th.p[i].pitch != th.p[i % len].pitch || th.p[i].rest != th.p[i % len].rest)) st.badTail++;
    }
    int prev = -1000;                                                    // шаги между соседними звучащими жемчужинами
    for (int i = 0; i < len; i++) {
      st.pearls++; if (th.p[i].rest) { st.rests++; continue; }
      const int g = D.gAt(th, i);
      if (prev != -1000) { st.steps++; if (std::abs(g - prev) <= 2) st.small++; }
      prev = g;
    }
  }
  const Thread& a = m.thread(0); const Thread& b = m.thread(1);
  for (int i = 0; i < b.len; i++) {                                      // созвучие нити 1 с нитью 0 того же номера (по модулю её длины)
    const int q = i % a.len;
    if (a.p[q].rest || b.p[i].rest) continue;
    sts[1].both++;
    const int d = std::abs(D.gAt(b, i) - D.gAt(a, q)) % 7;
    if (d == 0 || d == 2 || d == 4) sts[1].cons++;
  }
}
}  // namespace gen

static void genInvariants() {
  const struct { bool on; int idx, root; } cfg[] = {{false, 0, 0}, {true, 0, 0}, {true, 1, 3}, {true, 2, 7}, {true, 5, 10}};
  for (const auto& c : cfg) {
    Scale sc; sc.on = c.on; sc.idx = c.idx; sc.root = c.root;
    gen::Stats st[NR];
    for (int seed = 1; seed <= 300; seed++) gen::collect(sc, seed, seed & 1, st);
    for (int t = 0; t < NR; t++) {
      const gen::Stats& s = st[t];
      const double small = double(s.small) / s.steps, rest = double(s.rests) / s.pearls, cons = s.both ? double(s.cons) / s.both : 1;
      std::printf("  gen scale(on=%d idx=%d root=%d) thread %d: steps<=2 %.3f rest %.3f cons %.3f (%d)\n", c.on, c.idx, c.root, t, small, rest, cons, s.both);
      CHECK(s.badRhythm == 0);                                          // ритм каждой нити — евклидов
      CHECK(s.kLo == 0 && s.kHi == 0);                                  // ударов 3…len−2
      CHECK(s.badTail == 0);                                            // жемчужины за len повторяют петлю
      CHECK(s.outOfRange == 0);
      CHECK(s.offScale == 0);
      if (t == 0) CHECK(small >= 0.60);                                 // блуждание ±1…2 ступени, разрыв только на стыке петли мелодии
      CHECK(rest > 0.15 && rest < 0.60);
      if (t == 1) CHECK(cons >= 0.60);                                  // нить 1 созвучна нити 0 (терция/квинта/октава), когда обе звучат
    }
  }
}

static void genDeterministic() {
  Scale sc;
  Model a(sc), b(sc); a.reset(11); b.reset(11);
  for (int k = 0; k < 3; k++) { a.roll(); b.roll(); CHECK(a.snapshot() == b.snapshot()); }
  Model c(sc); c.reset(12); CHECK(c.snapshot() != Model(sc).snapshot());
  Model m(sc); m.reset(4); m.roll();                                    // ход нити не меняет петлю: ни высоты, ни паузы
  const std::string before = m.snapshot();
  Model keep(sc); keep.restore(before);
  std::vector<double> hz;
  for (int i = 0; i < 60; i++) { const Step s = m.next(i & 1); hz.push_back(s.hit ? s.hz : -1); }
  for (int t = 0; t < NR; t++) for (int i = 0; i < PEARL_MAX; i++) { CHECK(m.thread(t).p[i].pitch == keep.thread(t).p[i].pitch); CHECK(m.thread(t).p[i].rest == keep.thread(t).p[i].rest); }
  Model m2(sc); m2.restore(before);
  for (int i = 0; i < 60; i++) { const Step s = m2.next(i & 1); CHECK((s.hit ? s.hz : -1) == hz[i]); }
  CHECK(m2.snapshot() == m.snapshot());
}

static void genRollsDiffer() {                                          // каждый бросок — другая мелодия; roll(0) нить 1 не трогает
  Scale sc; Model m(sc); m.reset(6);
  const auto sig = [&](int t) { std::string r; for (int i = 0; i < PEARL_MAX; i++) r += std::to_string(m.thread(t).p[i].pitch) + (m.thread(t).p[i].rest ? "r" : "n"); return r; };
  int diff = 0;
  for (int k = 0; k < 20; k++) { const std::string a = sig(0); m.roll(0); if (sig(0) != a) diff++; }
  CHECK(diff >= 18);
}

static void genLengths() {                                              // разные длины петли 1…16: ритм Евклида, ноты в диапазоне
  Scale sc;
  for (int len = 1; len <= PEARL_MAX; len++) for (int seed = 1; seed <= 40; seed++) {
    Model m(sc); m.reset(seed); m.setLen(0, len); m.setLen(1, len); m.roll();
    const gen::Deg D(sc);
    for (int t = 0; t < NR; t++) {
      const Thread& th = m.thread(t);
      CHECK(gen::euclidK(th) > 0);
      for (int i = 0; i < PEARL_MAX; i++) { const int n = D.noteAt(th, i); CHECK(n >= th.lo && n <= th.hi); }
    }
  }
}

static void genSetLenKeepsNotes() {                                     // смена длины жемчужины не трогает; удлинение продолжает петлю
  Scale sc; Model m(sc); m.reset(5); m.roll();
  const std::string a = m.snapshot();
  Model b(sc); b.restore(a);
  for (int t = 0; t < NR; t++) for (int len : {2, 5, 16, 8, 3, 13, 1, 11}) {
    m.setLen(t, len);
    for (int i = 0; i < PEARL_MAX; i++) { CHECK(m.thread(t).p[i].pitch == b.thread(t).p[i].pitch); CHECK(m.thread(t).p[i].rest == b.thread(t).p[i].rest); }
  }
  m.setLen(0, m.thread(0).len); m.setLen(1, m.thread(1).len);
  Model r(sc); r.restore(m.snapshot()); CHECK(r.snapshot() == m.snapshot());
}

static void genNoDejaVuInSnapshot() {                                   // застёжки déjà vu больше нет: снимок её не пишет, старый с ней грузится
  Scale sc; Model m(sc); m.reset(2);
  CHECK(m.snapshot().find("dejavu") == std::string::npos);
  Model o(sc); o.restore("{\"thr\":[{\"len\":5,\"dejavu\":0.2},{\"len\":4,\"dejavu\":1}]}");
  CHECK(o.thread(0).len == 5 && o.thread(1).len == 4);
  CHECK(o.snapshot().find("dejavu") == std::string::npos);
}

static void genOldSnapshotLoads() {                                     // снимок до генератора: lo/hi 45…69 и 57…81, ноты вне лада, высоты любые
  const char* old = "{\"rng\":123,\"trng\":5,\"curLoc\":0,\"mix\":0.5,\"thr\":["
    "{\"len\":11,\"dejavu\":0.7,\"lo\":45,\"hi\":69,\"cur\":3,\"p\":[[0.01,0],[0.37,1],[0.99,0],[0.5,0],[0.123,0],[1,1],[0,0],[0.777,0],[0.2,0],[0.9,0],[0.45,0],[0.3,0],[0.3,0],[0.3,0],[0.3,0],[0.3,0]]},"
    "{\"len\":7,\"dejavu\":0.7,\"lo\":57,\"hi\":81,\"cur\":1,\"p\":[[0.11,0],[0.37,0],[0.99,0],[0.5,1],[0.123,0],[1,0],[0,0],[0.777,0],[0.2,0],[0.9,0],[0.45,0],[0.3,0],[0.3,0],[0.3,0],[0.3,0],[0.3,0]]}],"
    "\"rings\":[[0.5,0.5,0.5,0.8,0],[0.5,0.5,0.5,0.8,0]],\"cloud\":[0.5,0.4,0.4,0.5,0,0.3,0.6,0.35]}";
  for (int on = 0; on < 2; on++) {
    Scale sc; sc.on = on; sc.idx = 1; sc.root = 3;
    Model m(sc); m.restore(old);
    CHECK(m.thread(0).lo == 45 && m.thread(0).hi == 69 && m.thread(1).lo == 57 && m.thread(1).hi == 81);
    for (int i = 0; i < 300; i++) {                                     // играем с перебросом поверх старых нот
      if (i == 100) { }
      const Step s = m.next(i & 1);
      const int t = i & 1, semi = int(std::lround(12 * std::log2(s.hz / 55.0))), midi = semi + 33;
      CHECK(std::isfinite(s.hz) && s.hz > 0);
      CHECK(midi >= m.thread(t).lo - 1 && midi <= m.thread(t).hi + 1);
    }
    m.roll();
    for (int t = 0; t < NR; t++) for (int i = 0; i < PEARL_MAX; i++) { const double p = m.thread(t).p[i].pitch; CHECK(p >= 0 && p <= 1); }
  }
}

int main() {
  modelSeedDeterministic(); modelLoopRepeats(); modelRestsSilent();
  modelRingsCount(); modelDefaultRanges(); modelTwoThreadsIndependent(); modelRangeClamp(); modelPitchWithinRange(); modelDeterministic(); modelScaleSnap(); modelScaleSnapLowRange(); modelClamps(); modelLenOne();
  modelSnapshotRoundTrip(); modelRestoreGarbage(); modelRollAndClear(); modelClearAllIsEmpty(); modelClearUndoRestores(); modelClearTwice(); modelCloudSend();
  modelLocSwapKeepsSound(); modelLocKeepsEdits(); modelLocOutOfRange(); modelLocsDiffer(); modelDiceKeepsTimbre(); modelTimbreKeepsNotes(); modelTimbreDoesNotShiftMelody(); modelMixClamp(); modelLocSnapshotRoundTrip(); modelLocRestoreGarbage();
  ringsRingsDown(); ringsDampingMonotone(); ringsBrightnessCentroid(); ringsStable(); ringsIdleCheap(); ringsNoAlloc(); ringsPluckerBurst();
  cloudSilentIn(); cloudEchoesInput(); cloudFreezeHolds(); cloudFreezeEmpty(); cloudEchoDecays(); cloudNoNan(); cloudDensityBounded(); cloudSleepsAgain();
  worldSilentWhenStopped(); worldStepsInTime(); worldStrikeSounds(); worldNoStepFlood(); worldStopNoStuck(); worldSnapshotRoundTrip();
  worldGarbageActions(); worldBudget(); worldBothRingsStep(); worldClearSilences();
  modelRingModePoly(); soundStringsSound(); soundStructureGlide(); soundPolyVoices(); soundPolyStrings(); soundModeSwitchLive(); worldActRouting(); worldDiceBothOrOne(); worldMixSignal(); worldMixNoNaN(); worldNoAllocInProcess(); worldMirrorLayout(); worldLfoKeys();
  genInvariants(); genDeterministic(); genRollsDiffer(); genLengths(); genSetLenKeepsNotes(); genNoDejaVuInSnapshot(); genOldSnapshotLoads();
  std::printf("hymenaeus-test: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
