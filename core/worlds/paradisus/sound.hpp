// Paradisus — звук: хор ангелов, зёрна Gloria и россыпи, шиммер. Модель (model.hpp) решает, кто поёт; здесь только голоса.
//
// Цепочка голоса (ангел): до 5 пил с расстройкой ±(4+22·detune) центов и общим вибрато → четыре форманты (BpBank<4>, гласная — из модели)
//   + тело (сам источник сквозь мягкий ФНЧ ≤2400 Гц) + воздух (шум дыхания сквозь ФВЧ 3.5 кГц) → громкость с медленным дыханием (0.07–0.15 Гц).
//   Вход ангела — сразу, звук — мягко (атака τ 0.35 с, уход τ 0.5 с). Числа (BWK, FGK, BODYK, VCAK, DBK) — из прежнего engine.js.
// Шины: сухой = хор ×0.6·OUTK; шиммер (sdk::Shimmer, октава вверх, только влажное) добавляется к сухому и к посылу; посыл в общий собор
//   ядра = ФВЧ 160 Гц от хора ×SEND_K(0.3)·(0.35+0.5·собор)·(0.9+0.5·собор)·OUTK/0.9
//   (SEND_K — по замеру: без него возврат собора ядра в 3–5 раз громче сухого, пик 2–5). Своего зала нет.
// Зёрна: синусные окна на гармониках, стерео-разброс, расписание в сэмплах (wait), не по таймеру. Пул GRAINS; нет места — зерно пропадает.
// Страховка уровня: мягкое колено на сухом и на посыле (потолок 0.95), нечисла в выходе обнуляются; пик до колена — prePeak().
//
// ЛОВУШКИ:
//   - Всё выделено в prepare; step/render не выделяют и не растят векторы.
//   - Голос ищется по id ангела; закрепление меняет id — мир зовёт remap(старый, новый), иначе голос уйдёт и родится заново (слышно).
//   - step зовёт model.setSung (голос собран) и model.tick — мир не должен делать это сам.
#pragma once
#include <cstdint>

#include "../../sdk/bpbank.hpp"
#include "../../sdk/biquad.hpp"
#include "../../sdk/knee.hpp"
#include "../../sdk/rng.hpp"
#include "../../sdk/saw.hpp"
#include "../../sdk/shimmer.hpp"
#include "model.hpp"

namespace paradisus {

enum HeardType { H_CHIME, H_GRAIN };                     // chime: arg = id ангела; grain: arg = панорама −0.8…0.8

class Sound {
 public:
  static constexpr int BLOCK = 128, VOICES = 24, NSAW = 5, GRAINS = 64, EVENTS = 32;
  void prepare(double sr, uint32_t seed);
  // раз в блок до render: голоса по ангелам модели, model.tick(dt), chime → зёрна, Gloria
  void step(Model& m, double dt);
  void render(float* L, float* R, float* sL, float* sR, int n);   // n ≤ BLOCK; присваивает, не прибавляет
  void remap(int oldId, int newId);                      // ангел закреплён — голос переезжает на новый id
  void scatter(double hz, int n, double gapSec);         // n зёрен через gapSec (первое сразу): россыпь по розе, chime
  void silence();                                        // оборвать всё (голоса, зёрна, шиммер)
  bool sounding() const;
  int takeEvents(double* buf, int cap);                  // пары (HeardType, arg) с прошлого вызова; очищает; возвращает число пар
  int voicesUsed() const { int c = 0; for (const Voice& v : voice_) c += v.used; return c; }
  double level() const { return level_; }                // огибающая сухого выхода 0…1 (для вида)
  double prePeak() { const double p = prePeak_; prePeak_ = 0; return p; }   // пик сухого выхода до колена с прошлого вызова

 private:
  struct Voice {
    bool used = false, held = false; int id = 0;
    sdk::Saw saw[NSAW]; double g[NSAW] = {}, ratio[NSAW] = {}, tRatio[NSAW] = {}, tg[NSAW] = {}; bool on[NSAW] = {};
    sdk::BpBank<4> bank; sdk::Biquad body, air;
    double hz = 110, tHz = 110, bodyHz = 550, tBodyHz = 550, vibK = 0, tVibK = 0, breath = 0, tBreath = 0;
    double env = 0; bool releasing = false;
    double vph = 0, vinc = 0, sph = 0, sinc = 0;
    sdk::Rng rng{1};
  };
  struct Grain { bool on = false; int wait = 0, pos = 0, att = 0, dur = 0, tail = 0; double ph = 0, inc = 0, env = 0, e0 = 0, fac = 1, pl = 0, pr = 0, pan = 0; };
  Voice* find(int id);
  Voice* alloc();
  void begin(Voice& v, const Model& m, const Angel& a);
  void aim(Voice& v, const Model& m, const Angel& a, bool snap);
  void spawn(double hz, int wait);
  void note(int type, double arg) { if (ne_ < EVENTS) { et_[ne_] = type; ea_[ne_] = arg; ne_++; } }
  double rnd() { return rng_.next(); }

  Voice voice_[VOICES];
  Grain grain_[GRAINS];
  sdk::Shimmer shim_;
  sdk::Biquad hpL_, hpR_;
  sdk::Knee dryKnee_{1.0, 0.6, 0.35}, sendKnee_{1.0, 0.7, 0.25};
  sdk::Rng rng_{1};
  double sr_ = 48000, sg_ = 0.8, tsg_ = 0.8, gloAcc_ = 0, level_ = 0, prePeak_ = 0;
  int quiet_ = 0;                                        // сколько сэмплов подряд шиммер тих и входа нет
  Fx fx_;
  int et_[EVENTS] = {}, ne_ = 0;
  double ea_[EVENTS] = {};
};

}  // namespace paradisus
