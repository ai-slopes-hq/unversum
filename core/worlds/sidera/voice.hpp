// Sidera (XIV) — голос: ломаная по точкам модели, период нормируется под sr / f0. Линейная интерполяция без оверсэмплинга (алиасинг — часть звука GENDYN).
// Выход голоса → DcBlock → OnePole (мягкий выход) → панорама (равная мощность) → накопление в L/R и в посыл sL/sR.
// Точки периода копируются в начале периода: правка модели посреди периода слышна со следующего (без щелчка и без разрыва).
// По окончании периода голос вызывает Model::walk(v) — модель блуждает только отсюда.
#pragma once
#include "../../sdk/dc.hpp"
#include "../../sdk/smooth.hpp"
#include "model.hpp"

namespace sidera {

class Voice {
 public:
  void prepare(double sr);
  // накапливает n сэмплов голоса v в L/R и (если не nullptr) в посыл sL/sR (доля send); n любой
  void render(Model& m, int v, float* L, float* R, float* sL, float* sR, int n, double send);
  double energy() const { return env_; }                // сглаженная громкость выхода 0…1
  int segs() const { return n_; }                       // число отрезков текущего периода
  double seg(int i) const { return (i >= 0 && i < n_) ? len_[i] : 0; }   // длина отрезка в сэмплах
  double period() const { return period_; }             // длина периода в сэмплах (≥ число отрезков)

 private:
  void startPeriod(const Model& m, int v);
  double sr_ = 48000;
  int n_ = 0, seg_ = 0;
  double pos_ = 0, period_ = 0;
  double len_[NPT_MAX] = {}, amp_[NPT_MAX] = {};
  bool started_ = false;
  double gain_ = 0, env_ = 0, pan_ = 2, gl_ = 0.7071, gr_ = 0.7071;   // pan_ = 2 — ещё не считан
  sdk::DcBlock dc_;
  sdk::OnePole lp_;
};

}  // namespace sidera
