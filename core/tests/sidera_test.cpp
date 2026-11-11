// Тесты мира Sidera (XIV, GENDYN): модель, голос, мир. Сборка — core/build.sh → core/build/sidera-test.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../host/scale.hpp"
#include "../worlds/sidera/model.hpp"
#include "../worlds/sidera/voice.hpp"

static int checks = 0, fails = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

using namespace sidera;

// ---------- Task 1: модель ----------

static void modelSeedDeterministic() {
  Scale sc; Model a(sc), b(sc); a.reset(5); b.reset(5);
  for (int i = 0; i < 200; i++) for (int v = 0; v < NV; v++) { a.walk(v); b.walk(v); }
  bool same = true;
  for (int v = 0; v < NV; v++) for (int i = 0; i < NPT_MAX; i++) {
    const Point p = a.pt(v, i), q = b.pt(v, i);
    same = same && p.d == q.d && p.a == q.a && p.hd == q.hd && p.ha == q.ha;
  }
  CHECK(same);
}

static void modelBarriersHold() {
  Scale sc;
  for (int dist = 0; dist < NDIST; dist++) {
    Model m(sc); m.reset(7); m.setStep(1);
    for (int v = 0; v < NV; v++) m.setVoice(v, DIST, dist);
    bool ok = true;
    for (int s = 0; s < 5000; s++) for (int v = 0; v < NV; v++) {
      m.walk(v);
      const int n = static_cast<int>(m.voice(v, N));
      for (int i = 0; i < n; i++) {
        const Point p = m.pt(v, i);
        ok = ok && std::fabs(p.a - p.ha) <= m.voice(v, WA) / 2 + 1e-9 && std::fabs(p.d - p.hd) <= m.voice(v, WD) / 2 + 1e-9
             && p.d >= 0.02 && std::fabs(p.a) <= 1 && std::isfinite(p.a) && std::isfinite(p.d);
      }
    }
    CHECK(ok);
  }
}

static void modelStepZeroStatic() {
  Scale sc; Model m(sc); m.setStep(0);
  const Point before = m.pt(0, 2);
  for (int s = 0; s < 100; s++) for (int v = 0; v < NV; v++) m.walk(v);
  const Point after = m.pt(0, 2);
  CHECK(before.d == after.d && before.a == after.a);
}

static void modelCollapsedBox() {
  Scale sc; Model m(sc); m.setStep(1);
  for (int v = 0; v < NV; v++) { m.setVoice(v, WD, 0); m.setVoice(v, WA, 0); }
  bool ok = true;
  for (int s = 0; s < 200; s++) for (int v = 0; v < NV; v++) {
    m.walk(v);
    for (int i = 0; i < static_cast<int>(m.voice(v, N)); i++) { const Point p = m.pt(v, i); ok = ok && p.d == p.hd && p.a == p.ha; }
  }
  CHECK(ok);
}

static void modelClamps() {
  Scale sc; Model m(sc);
  m.setVoice(0, N, 0); CHECK(m.voice(0, N) == 3);
  m.setVoice(0, N, 99); CHECK(m.voice(0, N) == 16);
  m.setVoice(0, N, 7.4); CHECK(m.voice(0, N) == 7);
  m.setVoice(0, N, NAN); CHECK(m.voice(0, N) == 7);
  m.setVoice(0, LVL, INFINITY); CHECK(m.voice(0, LVL) == 1);
  m.setVoice(0, LVL, -INFINITY); CHECK(m.voice(0, LVL) == 0);
  m.setVoice(0, PAN, 5); CHECK(m.voice(0, PAN) == 1);
  m.setVoice(0, DIST, 99); CHECK(m.voice(0, DIST) == NDIST - 1);
  m.setVoice(0, DIST, -5); CHECK(m.voice(0, DIST) == 0);
  m.setVoice(0, NOTE, 1000); CHECK(m.voice(0, NOTE) == 60);
  m.setVoice(9, LVL, 0.1); m.setVoice(-1, LVL, 0.1); m.setVoice(0, 99, 0.1);   // вне диапазона — ничего, без падения
  m.setStep(NAN); const double s0 = m.step(); m.setStep(-3); CHECK(m.step() == 0); m.setStep(9); CHECK(m.step() == 1); (void)s0;
}

static void modelMoveHome() {
  Scale sc; Model m(sc);
  m.moveHome(1, 2, 9, -9);
  CHECK(m.pt(1, 2).hd == 1 && m.pt(1, 2).ha == -1);
  m.moveHome(1, 2, -9, 9);
  CHECK(m.pt(1, 2).hd == 0.02 && m.pt(1, 2).ha == 1);
  m.moveHome(1, 2, NAN, NAN);
  CHECK(m.pt(1, 2).hd == 0.02 && m.pt(1, 2).ha == 1);
  m.moveHome(1, 99, 0.5, 0.5); m.moveHome(7, 0, 0.5, 0.5);                      // вне диапазона — без падения
}

static void modelSilence() {
  Scale sc; Model m(sc);
  CHECK(m.sounding());
  const Point p = m.pt(0, 1);
  m.silence();
  CHECK(!m.sounding());
  for (int v = 0; v < NV; v++) CHECK(m.voice(v, LVL) == 0);
  CHECK(m.pt(0, 1).a == p.a && m.pt(0, 1).d == p.d);
}

static void modelRollResetsToHome() {
  Scale sc; Model m(sc); m.reset(3);
  for (int s = 0; s < 50; s++) m.walk(0);
  m.roll();
  bool home = true;
  for (int i = 0; i < static_cast<int>(m.voice(0, N)); i++) home = home && m.pt(0, i).a == m.pt(0, i).ha && m.pt(0, i).d == m.pt(0, i).hd;
  CHECK(home);
  Model a(sc), b(sc); a.reset(9); b.reset(9); a.roll(); b.roll();
  for (int s = 0; s < 20; s++) { a.walk(1); b.walk(1); }
  CHECK(a.pt(1, 0).a == b.pt(1, 0).a);                                          // кубик с зерном воспроизводим
  a.roll(1); a.walk(1);
  CHECK(a.pt(1, 0).a != b.pt(1, 0).a || a.pt(1, 3).a != b.pt(1, 3).a);          // новая раскладка семян
}

static void modelSnapshotRoundTrip() {
  Scale sc; Model a(sc); a.reset(11);
  a.setStep(0.7); a.setVoice(1, N, 10); a.setVoice(2, DIST, LOGISTIC); a.moveHome(0, 1, 0.3, -0.2);
  for (int s = 0; s < 30; s++) for (int v = 0; v < NV; v++) a.walk(v);
  const std::string js = a.snapshot();
  Model b(sc); b.restore(js);
  CHECK(b.snapshot() == js);
  for (int s = 0; s < 10; s++) { a.walk(2); b.walk(2); }
  CHECK(a.pt(2, 4).a == b.pt(2, 4).a);
}

static void modelRestoreGarbage() {
  Scale sc; Model m(sc);
  const char* bad[] = {"{", "[]", "", "null", "{\"voices\":5}", "{\"voices\":[{\"p\":[1e999,null,\"x\"],\"pts\":[[1]]}],\"step\":\"a\"}"};
  for (const char* js : bad) {
    m.restore(js);
    CHECK(m.step() >= 0 && m.step() <= 1);
    for (int v = 0; v < NV; v++) {
      const int n = static_cast<int>(m.voice(v, N));
      CHECK(n >= NPT_MIN && n <= NPT_MAX);
      for (int i = 0; i < NPT_MAX; i++) CHECK(std::isfinite(m.pt(v, i).a) && std::isfinite(m.pt(v, i).d) && m.pt(v, i).d >= 0.02);
      m.walk(v);
    }
  }
}

static void modelHzFollowsScale() {
  Scale sc; Model m(sc);
  m.setVoice(0, NOTE, 0); CHECK(std::fabs(m.hzOf(0) - 55.0) < 1e-9);
  m.setVoice(0, NOTE, 12); CHECK(std::fabs(m.hzOf(0) - 110.0) < 1e-9);
  sc.on = true; sc.idx = 0; sc.root = 0;                                        // A major: 1 (A#) вне лада
  m.setVoice(0, NOTE, 1);
  CHECK(m.hzOf(0) != 55.0 * std::pow(2.0, 1.0 / 12));
}

// ---------- Task 2: голос ----------

struct Buf {
  std::vector<float> L, R, sL, sR;
  void render(Voice& vo, Model& m, int v, int n, double send = 0.3) {   // n сэмплов блоками по 128, выход накапливается
    for (int done = 0; done < n;) {
      const int k = std::min(128, n - done);
      std::vector<float> l(k, 0.f), r(k, 0.f), sl(k, 0.f), sr(k, 0.f);
      vo.render(m, v, l.data(), r.data(), sl.data(), sr.data(), k, send);
      L.insert(L.end(), l.begin(), l.end()); R.insert(R.end(), r.begin(), r.end());
      sL.insert(sL.end(), sl.begin(), sl.end()); sR.insert(sR.end(), sr.begin(), sr.end());
      done += k;
    }
  }
};

static void voiceSumOfSegments() {
  Scale sc;
  for (int n : {3, 8, 16}) {
    Model m(sc); m.setVoice(0, N, n); Voice vo; vo.prepare(48000);
    Buf b; b.render(vo, m, 0, 128);
    double sum = 0;
    for (int i = 0; i < vo.segs(); i++) sum += vo.seg(i);
    CHECK(vo.segs() == n);
    CHECK(std::fabs(sum - vo.period()) < 1e-9);
    CHECK(std::fabs(vo.period() - 48000.0 / m.hzOf(0)) <= 1.0);
  }
}

static void voiceNoDc() {
  Scale sc; Model m(sc); Voice vo; vo.prepare(48000);
  Buf b; b.render(vo, m, 0, 96000);
  double mean = 0; int cnt = 0;
  for (size_t i = 24000; i < b.L.size(); i++) { mean += b.L[i] + b.R[i]; cnt += 2; }
  CHECK(std::fabs(mean / cnt) < 0.01);
  double pk = 0; for (float x : b.L) pk = std::max(pk, static_cast<double>(std::fabs(x)));
  CHECK(pk > 0.01);                                                              // и звук есть
}

static void voiceFiniteExtremes() {
  Scale sc;
  for (int note : {0, 60}) for (int n : {3, 16}) for (double step : {0.0, 1.0}) for (int dist = 0; dist < NDIST; dist++) {
    Model m(sc); m.reset(4); m.setStep(step); m.setVoice(0, NOTE, note); m.setVoice(0, N, n); m.setVoice(0, DIST, dist); m.setVoice(0, WA, 1); m.setVoice(0, WD, 1);
    Voice vo; vo.prepare(48000);
    Buf b; b.render(vo, m, 0, 96000);
    bool ok = true;
    for (size_t i = 0; i < b.L.size(); i++) ok = ok && std::isfinite(b.L[i]) && std::isfinite(b.R[i]) && std::isfinite(b.sL[i]) && std::fabs(b.L[i]) <= 4 && std::fabs(b.R[i]) <= 4;
    CHECK(ok);
  }
}

static void voiceStepZeroPeriodic() {
  Scale sc; Model m(sc); m.setStep(0); m.setVoice(1, LVL, 0); m.setVoice(2, LVL, 0);
  Voice vo; vo.prepare(44000);                                                   // f0 55 Гц → период ровно 800 сэмплов
  Buf b; b.render(vo, m, 0, 800 * 20);
  double worst = 0;
  for (int i = 0; i < 800; i++) worst = std::max(worst, static_cast<double>(std::fabs(b.L[800 * 18 + i] - b.L[800 * 19 + i])));
  CHECK(worst < 1e-6);
}

static void voiceLevelZeroSilent() {
  Scale sc; Model m(sc); for (int v = 0; v < NV; v++) m.setVoice(v, LVL, 0);
  Voice vo; vo.prepare(48000);
  Buf b; b.render(vo, m, 0, 48000);
  bool zero = true;
  for (size_t i = 0; i < b.L.size(); i++) zero = zero && b.L[i] == 0 && b.R[i] == 0 && b.sL[i] == 0 && b.sR[i] == 0;
  CHECK(zero);
  CHECK(vo.energy() < 1e-6);
}

static void voiceNoteChangeAtBoundary() {
  Scale sc; Model m(sc); m.setStep(1); Voice vo; vo.prepare(48000);
  Buf b;
  for (int blk = 0; blk < 400; blk++) {
    m.setVoice(0, NOTE, (blk * 7) % 61); m.setVoice(0, N, 3 + (blk * 5) % 14);
    b.render(vo, m, 0, 128);
  }
  bool ok = true; double jump = 0;
  for (size_t i = 0; i < b.L.size(); i++) {
    ok = ok && std::isfinite(b.L[i]) && std::isfinite(b.R[i]);
    if (i) jump = std::max(jump, static_cast<double>(std::fabs(b.L[i] - b.L[i - 1])));
  }
  CHECK(ok);
  CHECK(jump <= 2.0);
}

static void voiceSilenceThenResume() {
  Scale sc; Model m(sc); Voice vo; vo.prepare(48000);
  Buf b; b.render(vo, m, 0, 4800);
  m.setVoice(0, LVL, 0); b.render(vo, m, 0, 48000);
  m.setVoice(0, LVL, 0.5); Buf c; c.render(vo, m, 0, 4800);
  double pk = 0; for (float x : c.L) pk = std::max(pk, static_cast<double>(std::fabs(x)));
  CHECK(pk > 0.01);
  bool fin = true; for (float x : c.L) fin = fin && std::isfinite(x);
  CHECK(fin);
}

int main() {
  modelSeedDeterministic(); modelBarriersHold(); modelStepZeroStatic(); modelCollapsedBox(); modelClamps(); modelMoveHome(); modelSilence();
  modelRollResetsToHome(); modelSnapshotRoundTrip(); modelRestoreGarbage(); modelHzFollowsScale();
  voiceSumOfSegments(); voiceNoDc(); voiceFiniteExtremes(); voiceStepZeroPeriodic(); voiceLevelZeroSilent(); voiceNoteChangeAtBoundary(); voiceSilenceThenResume();
  std::printf("sidera-test: %d checks, %d failed\n", checks, fails);
  return fails ? 1 : 0;
}
