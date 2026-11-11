// Shimmer — реверберационный «блеск»: сигнал сдвигается на октаву вверх, возвращается в петлю
// и каждый круг уходит ещё выше. Звук берётся только влажный, сухой остаётся у вызывающего.
//
//   вход + fb·петля → сдвиг +12 полутонов → DC-блок → выход × mix
//                                              └→ ФНЧ → tanh → петля (× fb·amount)
//
// Сдвиг: две точки чтения в кольце задержки, каждая бежит вдвое быстрее записи (задержка
// уменьшается на сэмпл за сэмпл от окна W до нуля), веса — окна Ханна со сдвигом на пол-окна,
// сумма весов всегда 1. Окно ≈ 60 мс, длина нечётная (чтобы пик веса не попадал в целый сэмпл).
//
// ЛОВУШКИ:
//   - обратная связь жёстко ограничена kMaxFb = 0.55 при любых ручках; сдвиг не усиливает, tanh
//     и жёсткий клип ±1 на выходе — последняя страховка.
//   - память выделяется в prepare(); process() не выделяет.
//   - decay/amount/mix/tone сглаживаются (~20 мс), первый set() после prepare/reset — без сглаживания.
// Цена — два чтения с интерполяцией, два биквадратных веса, ФНЧ и DC-блок на сэмпл.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "dc.hpp"

namespace sdk {

class Shimmer {
public:
  static constexpr float kMaxFb = 0.55f;

  void prepare(double sr, int /*maxBlock*/) {
    sr_ = sr > 1000 ? sr : 48000;
    win_ = static_cast<int>(0.06 * sr_) | 1;           // нечётное
    size_ = win_ + 4;
    buf_.assign(static_cast<size_t>(size_), 0.f);
    const double r = std::exp(-2 * M_PI * 10.0 / sr_);   // DC-блок ~10 Гц
    dcOut_.R = r; dcOut_.previousInput = dcOut_.previousOutput = 0;
    dcLoop_.R = r;
    smoothK_ = 1 - std::exp(-1.0 / (0.02 * sr_));
    reset();
  }

  void reset() {
    std::fill(buf_.begin(), buf_.end(), 0.f);
    wpos_ = 0; phase_ = 0; loop_ = 0; lp_ = 0;
    dcOut_.previousInput = dcOut_.previousOutput = 0;
    dcLoop_.previousInput = dcLoop_.previousOutput = 0;
    fresh_ = true;
  }

  void flush() {
    std::fill(buf_.begin(), buf_.end(), 0.f);
    loop_ = 0; lp_ = 0;
    dcOut_.previousInput = dcOut_.previousOutput = 0;
    dcLoop_.previousInput = dcLoop_.previousOutput = 0;
  }

  void set(float amount, float decay, float tone, float mix) {
    auto c01 = [](float v) { return v == v ? std::min(1.f, std::max(0.f, v)) : 0.f; };
    tAmount_ = c01(amount);
    tFb_ = c01(decay) * kMaxFb;
    tMix_ = c01(mix);
    const double fc = 2000.0 * std::pow(4.0, c01(tone));   // 2..8 кГц
    tLp_ = static_cast<float>(1 - std::exp(-2 * M_PI * fc / sr_));
    if (fresh_) { amount_ = tAmount_; fb_ = tFb_; mix_ = tMix_; lpA_ = tLp_; fresh_ = false; }
  }

  void process(const float* inL, const float* inR, float* outL, float* outR, int n) {
    if (buf_.empty()) { for (int i = 0; i < n; i++) outL[i] = outR[i] = 0; return; }
    const float dPhase = 1.f / static_cast<float>(win_);
    for (int i = 0; i < n; i++) {
      amount_ += (tAmount_ - amount_) * smoothK_;
      fb_ += (tFb_ - fb_) * smoothK_;
      mix_ += (tMix_ - mix_) * smoothK_;
      lpA_ += (tLp_ - lpA_) * smoothK_;

      const float l = std::isfinite(inL[i]) ? inL[i] : 0.f, r = std::isfinite(inR[i]) ? inR[i] : 0.f;
      const float in = 0.5f * (l + r);
      if (!std::isfinite(loop_) || !std::isfinite(lp_)) flush();   // страховка: состояние не должно залипать
      buf_[static_cast<size_t>(wpos_)] = in + std::min(fb_, kMaxFb) * loop_ * amount_;

      // две точки: задержка = win·(1 − фаза), вес = Hann(фаза)
      float shifted = 0;
      for (int t = 0; t < 2; t++) {
        float ph = phase_ + 0.5f * static_cast<float>(t);
        if (ph >= 1.f) ph -= 1.f;
        const float d = static_cast<float>(win_) * (1.f - ph);
        const float w = 0.5f - 0.5f * std::cos(2.f * static_cast<float>(M_PI) * ph);
        float rp = static_cast<float>(wpos_) - d;
        while (rp < 0) rp += static_cast<float>(size_);
        int i0 = static_cast<int>(rp);
        const float fr = rp - static_cast<float>(i0);
        if (i0 >= size_) i0 -= size_;
        const int i1 = i0 + 1 >= size_ ? 0 : i0 + 1;
        const float s = buf_[static_cast<size_t>(i0)] * (1 - fr) + buf_[static_cast<size_t>(i1)] * fr;
        shifted += w * s;
      }
      phase_ += dPhase;
      if (phase_ >= 1.f) phase_ -= 1.f;
      if (++wpos_ >= size_) wpos_ = 0;

      const float wet = static_cast<float>(dcOut_.step(shifted));
      lp_ += lpA_ * (wet - lp_);
      loop_ = std::tanh(static_cast<float>(dcLoop_.step(lp_)));

      float o = wet * mix_;
      o = o == o ? std::min(1.f, std::max(-1.f, o)) : 0.f;
      outL[i] = outR[i] = o;
    }
  }

private:
  double sr_ = 48000;
  int win_ = 2881, size_ = 2885, wpos_ = 0;
  std::vector<float> buf_;
  float phase_ = 0, loop_ = 0, lp_ = 0;
  float amount_ = 0, fb_ = 0, mix_ = 0, lpA_ = 0.2f;
  float tAmount_ = 0, tFb_ = 0, tMix_ = 0, tLp_ = 0.2f;
  float smoothK_ = 0.001f;
  bool fresh_ = true;
  DcBlock dcOut_, dcLoop_;
};

}  // namespace sdk
