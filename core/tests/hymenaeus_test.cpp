// Тесты мира Hymenaeus (XIV): модель, кольца, облака, мир. Сборка и запуск — core/build.sh → core/build/hymenaeus-test.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../host/scale.hpp"
#include "../worlds/hymenaeus/model.hpp"

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

int main() {
  modelSeedDeterministic(); modelDejaVuOneRepeats(); modelDejaVuZeroVaries(); modelRestsSilent();
  modelRingBands(); modelPitchToRing(); modelScaleSnap(); modelClamps(); modelLenOne();
  modelSnapshotRoundTrip(); modelRestoreGarbage(); modelRollAndClear();
  std::printf("hymenaeus-test: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
