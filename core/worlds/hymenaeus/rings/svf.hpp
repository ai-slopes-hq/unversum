// Svf и линия задержки — из stmlib (Mutable Instruments, MIT, Emilie Gillet; см. LICENSE-MI.txt), в float и без приближений tan:
// коэффициенты считаются редко (раз в блок), поэтому точный std::tan.
#pragma once
#include <cmath>
#include <cstddef>

namespace rings {

class Svf {
 public:
  void reset() { s1_ = s2_ = 0; }
  void setFQ(float f, float q) {                         // f — доля частоты дискретизации (0…0.49), q ≥ 1
    g_ = std::tan(3.14159265358979f * f);
    r_ = 1.0f / q;
    h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
  }
  float bandPass(float in) {
    const float hp = (in - r_ * s1_ - g_ * s1_ - s2_) * h_;
    const float bp = g_ * hp + s1_;
    s1_ = g_ * hp + bp;
    const float lp = g_ * bp + s2_;
    s2_ = g_ * bp + lp;
    return bp;
  }
  float lowPass(float in) {
    const float hp = (in - r_ * s1_ - g_ * s1_ - s2_) * h_;
    const float bp = g_ * hp + s1_;
    s1_ = g_ * hp + bp;
    const float lp = g_ * bp + s2_;
    s2_ = g_ * bp + lp;
    return lp;
  }

 private:
  float g_ = 0.01f, r_ = 1.0f, h_ = 1.0f, s1_ = 0, s2_ = 0;
};

template <size_t N>
class DelayLine {
 public:
  void reset() { for (size_t i = 0; i < N; i++) line_[i] = 0; wp_ = 0; }
  void write(float x) { line_[wp_] = x; wp_ = (wp_ + N - 1) % N; }
  float read(float delay) const {                        // delay в сэмплах, 0 ≤ delay < N − 1, линейная интерполяция
    const size_t d = static_cast<size_t>(delay);
    const float f = delay - static_cast<float>(d);
    const float a = line_[(wp_ + d) % N], b = line_[(wp_ + d + 1) % N];
    return a + (b - a) * f;
  }

 private:
  float line_[N] = {};
  size_t wp_ = 0;
};

}  // namespace rings
