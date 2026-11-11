// Модальный резонатор Rings: банк полосовых фильтров (моды) с растяжением гармоник (structure), потерями на верхних модах
// (brightness), затуханием (damping) и положением съёма (position). Порт rings/dsp/resonator.{h,cc} Mutable Instruments
// (MIT, Emilie Gillet; см. LICENSE-MI.txt). Отличия от оригинала:
//   - 28 мод (kModes), а не 64; блоки любой длины; нет lut_* — stiffness считается формулой из rings/resources/lookup_tables.py;
//   - damping: чем больше, тем короче звон (у ручки Rings наоборот); при damping 0 t60 ≈ 20 с. Добавлена поправка Q на искажение частоты
//     в SVF (warp, см. computeFilters): затухает всегда, самоколебания нет;
//   - молчащий резонатор (нет входа и выход < 1e-6) ничего не считает и обнуляет состояния фильтров (без денормалов).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

#include "svf.hpp"

namespace rings {

constexpr int kModes = 28;
constexpr float kMaxDecay = 0.735f;                      // 10^(4·0.735)·500/(π·48000)·6.9 ≈ 20 с для основной моды

inline float stiffnessOf(float g) {                      // rings/resources/lookup_tables.py, 'stiffness'
  if (g < 0.25f) return -(0.25f - g) * 0.25f;
  if (g < 0.3f) return 0.0f;
  if (g < 0.9f) return 0.01f * std::pow(10.0f, (g - 0.3f) / 0.6f * 2.005f) - 0.01f;
  const float h = (g - 0.9f) / 0.1f;
  return 1.5f - std::cos(h * h * 3.14159265358979f) / 2.0f;
}

class Resonator {
 public:
  void prepare(double sr) {
    sr_ = static_cast<float>(sr > 0 ? sr : 48000);
    for (auto& f : f_) f.reset();
    freq_ = 220.0f / sr_; structure_ = 0.25f; brightness_ = 0.5f; damping_ = 0.7f; position_ = prevPos_ = 0.999f;
    dirty_ = true; active_ = false;
  }
  void setFreq(double hz) { if (std::isnan(hz)) return; const float f = std::min(std::max(static_cast<float>(hz) / sr_, 10.0f / sr_), 0.49f); if (f != freq_) { freq_ = f; dirty_ = true; } }
  void setStructure(double v) { set(structure_, v); }
  void setBrightness(double v) { set(brightness_, v); }
  void setDamping(double v) { set(damping_, v); }
  void setPosition(double v) { if (!std::isnan(v)) position_ = static_cast<float>(std::min(std::max(v, 0.0), 0.999)); }
  bool active() const { return active_; }

  // in → odd (моды 1, 3, 5…) и even (2, 4, 6…); n любое
  void process(const float* in, float* odd, float* even, int n) {
    bool any = false;
    for (int i = 0; i < n; i++) if (std::fabs(in[i]) > 1e-9f) { any = true; break; }
    if (!any && !active_) { for (int i = 0; i < n; i++) odd[i] = even[i] = 0; return; }
    if (dirty_) { modes_ = computeFilters(); dirty_ = false; }
    const float p0 = prevPos_, dp = (position_ - p0) / static_cast<float>(n > 0 ? n : 1);
    float peak = 0, pos = p0;
    for (int i = 0; i < n; i++) {
      pos += dp;
      // косинусные веса мод — CosineOscillator::InitApproximate из stmlib
      float fq = pos - 0.25f, sign = 16.0f;
      if (fq < 0.0f) fq = -fq; else if (fq > 0.5f) fq -= 0.5f; else sign = -16.0f;
      const float iir = sign * fq * (1.0f - 2.0f * fq);
      float y1 = iir * 0.25f, y0 = 0.5f;
      const float x = in[i] * 0.125f;
      float o = 0, e = 0;
      for (int m = 0; m < modes_;) {
        float t = y0; y0 = iir * y0 - y1; y1 = t; o += (t + 0.5f) * f_[m++].bandPass(x);
        t = y0; y0 = iir * y0 - y1; y1 = t; e += (t + 0.5f) * f_[m++].bandPass(x);
      }
      odd[i] = o; even[i] = e;
      peak = std::max(peak, std::max(std::fabs(o), std::fabs(e)));
    }
    prevPos_ = position_;
    active_ = any || peak > 1e-6f;
    if (!active_) { for (auto& f : f_) f.reset(); for (int i = 0; i < n; i++) odd[i] = even[i] = 0; }
  }

 private:
  void set(float& dst, double v) { if (std::isnan(v)) return; const float x = static_cast<float>(std::min(std::max(v, 0.0), 1.0)); if (x != dst) { dst = x; dirty_ = true; } }

  int computeFilters() {
    float stiffness = stiffnessOf(structure_);
    float harmonic = freq_, stretch = 1.0f;
    const float decay = (1.0f - damping_) * kMaxDecay;
    float q = 500.0f * std::pow(10.0f, 4.0f * decay) * (48000.0f / sr_);
    float att = 1.0f - structure_; att *= att; att *= att; att *= att;
    const float bright = brightness_ * (1.0f - 0.2f * att);
    float qLoss = bright * (2.0f - bright) * 0.85f + 0.15f;
    const float qLossRate = structure_ * (2.0f - structure_) * 0.1f;
    int modes = 0;
    for (int i = 0; i < kModes; i++) {
      float pf = harmonic * stretch;
      if (pf >= 0.49f) pf = 0.49f; else modes = i + 1;
      // у SVF цифровое затухание на частоте f в sin(2πf)/(2πf) раз медленнее аналогового (у Найквиста — в сотни раз):
      // без поправки верхние моды звенят минутами при brightness 1
      const float warp = std::sin(6.28318530717959f * pf) / (6.28318530717959f * pf);
      f_[i].setFQ(pf, 1.0f + pf * q * warp);
      stretch += stiffness;
      stiffness *= stiffness < 0.0f ? 0.93f : 0.98f;
      qLoss += qLossRate * (1.0f - qLoss);
      harmonic += freq_;
      q *= qLoss;
    }
    return std::min(kModes, modes + (modes & 1));        // пары odd/even
  }

  float sr_ = 48000, freq_ = 0.005f, structure_ = 0.25f, brightness_ = 0.5f, damping_ = 0.7f, position_ = 0.999f, prevPos_ = 0.999f;
  int modes_ = 0;
  bool dirty_ = true, active_ = false;
  Svf f_[kModes];
};

}  // namespace rings
