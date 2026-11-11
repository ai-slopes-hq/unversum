// Щипок Rings: короткий всплеск шума, прогнанный через гребёнку (место щипка) и ФНЧ (яркость) — возбуждение для Resonator.
// Порт rings/dsp/plucker.h Mutable Instruments (MIT, Emilie Gillet; см. LICENSE-MI.txt); шум — sdk::Rng вместо stmlib::Random.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../../../sdk/rng.hpp"
#include "svf.hpp"

namespace rings {

class Plucker {
 public:
  void prepare(double sr, uint32_t seed) {
    sr_ = static_cast<float>(sr > 0 ? sr : 48000);
    rng_ = sdk::Rng(seed); comb_.reset(); svf_.reset();
    remaining_ = 0; period_ = 0; gain_ = 0; amp_ = 0;
  }
  // hz — частота ноты, bright 0…1 — срез ФНЧ, pos 0…1 — место щипка, vel 0…1 — сила. NaN в любом аргументе — щипка нет
  void trigger(double hz, double bright, double pos, double vel) {
    if (std::isnan(hz) || std::isnan(bright) || std::isnan(pos) || std::isnan(vel)) return;
    const float f = std::min(std::max(static_cast<float>(hz) / sr_, 20.0f / sr_), 0.45f);
    const float b = static_cast<float>(std::min(std::max(bright, 0.0), 1.0));
    const float p = static_cast<float>(std::min(std::max(pos, 0.0), 1.0));
    amp_ = static_cast<float>(std::min(std::max(vel, 0.0), 1.0));
    float period = 1.0f / f * (p * 0.9f + 0.05f);
    remaining_ = static_cast<int>(period);
    while (period >= 255.0f) period *= 0.5f;
    period_ = period;
    gain_ = (1.0f - p) * 0.8f;
    comb_.reset(); svf_.reset();                         // новый щипок заменяет старый
    svf_.setFQ(std::min(f * std::pow(2.0f, 1.0f + 5.0f * b), 0.499f), 1.0f);
  }
  void process(float* out, int n) {
    for (int i = 0; i < n; i++) {
      float in = 0;
      if (remaining_ > 0) { in = static_cast<float>(2.0 * rng_.next() - 1.0); remaining_--; }
      const float y = in + gain_ * comb_.read(period_);
      comb_.write(y);
      out[i] = svf_.lowPass(y) * amp_;
    }
  }

 private:
  float sr_ = 48000, period_ = 0, gain_ = 0, amp_ = 0;
  int remaining_ = 0;
  sdk::Rng rng_{1};
  Svf svf_;
  DelayLine<256> comb_;
};

}  // namespace rings
