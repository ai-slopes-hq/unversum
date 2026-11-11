// Струна Rings (Karplus–Strong с гребёнкой-съёмом) и набор нот «сочувствующих струн». Порт rings/dsp/string.{h,cc} и
// Part::ComputeSympatheticStringsNotes Mutable Instruments (MIT, Emilie Gillet; см. LICENSE-MI.txt). Отличия от оригинала:
//   - без дисперсии (у sympathetic strings её нет: Init(false)) и без передискретизации для частот < 11.7 Гц (частота зажимается линией задержки);
//   - блоки любой длины; частота дискретизации любая (оригинал — 48 кГц); поправка задержки ФНЧ считается формулой (atan), не таблицей lut_svf_shift;
//   - молчащая струна обнуляется через reset(); первый блок после reset() не плавит параметры от старых значений;
//   - аккорды-пресеты (QUANTIZED) не портированы: нужен только режим, где structure двигает аккорд.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

#include "svf.hpp"

namespace rings {

constexpr int kStringDelay = 2048;

class DampingFilter {
 public:
  void reset() { x_ = x__ = 0; }
  void configure(float damping, float brightness, int size, bool snap) {
    if (snap || size <= 0) { damping_ = damping; brightness_ = brightness; dd_ = bd_ = 0; return; }
    const float step = 1.0f / static_cast<float>(size);
    dd_ = (damping - damping_) * step; bd_ = (brightness - brightness_) * step;
  }
  float process(float x) {
    const float h0 = (1.0f + brightness_) * 0.5f, h1 = (1.0f - brightness_) * 0.25f;
    const float y = damping_ * (h0 * x_ + h1 * (x + x__));
    x__ = x_; x_ = x; brightness_ += bd_; damping_ += dd_;
    return y;
  }

 private:
  float x_ = 0, x__ = 0, brightness_ = 0, bd_ = 0, damping_ = 0, dd_ = 0;
};

class String {
 public:
  void prepare(double sr) { sr_ = static_cast<float>(sr > 0 ? sr : 48000); reset(); }
  void reset() { line_.reset(); fir_.reset(); iir_.reset(); fresh_ = true; }
  // frequency — доля частоты дискретизации; glide 1 — сразу, меньше — плавно (за блок)
  void setFrequency(float f, float glide = 1.0f) {
    if (!std::isfinite(f)) return;
    f = std::min(std::max(f, 1.0f / (kStringDelay - 4.0f)), 0.45f);
    if (fresh_ || glide >= 1.0f) frequency_ = f; else frequency_ += glide * (f - frequency_);
  }
  void setBrightness(float b) { if (std::isfinite(b)) brightness_ = std::min(std::max(b, 0.0f), 1.0f); }
  void setDamping(float d) { if (std::isfinite(d)) damping_ = std::min(std::max(d, 0.0f), 0.99f); }   // выше 0.95 в оригинале — бесконечный звон: зажимаем выше
  void setPosition(float p) { if (std::isfinite(p)) position_ = p; }

  // добавляет в out (струна) и aux (съём гребёнкой)
  void process(const float* in, float* out, float* aux, int size) {
    if (size <= 0) return;
    float delay = std::min(std::max(1.0f / frequency_, 4.0f), kStringDelay - 4.0f);
    const float clampedPos = 0.5f - 0.98f * std::fabs(position_ - 0.5f);
    const float lfDamping = damping_ * (2.0f - damping_);
    const float rt60 = 0.07f * semis(lfDamping * 96.0f) * sr_;
    float coeff = semis(std::max(-120.0f * delay / rt60, -127.0f));
    float brightness = brightness_ * brightness_;
    float cutoff = std::min(24.0f + damping_ * damping_ * 48.0f + brightness_ * brightness_ * 24.0f, 84.0f);
    float dampF = std::min(frequency_ * semis(cutoff), 0.499f);
    if (damping_ >= 0.95f) {                             // как в оригинале: к бесконечному звону (до 0.99 в setDamping — всё же затухает)
      const float inf = 20.0f * (damping_ - 0.95f);
      coeff += inf * (1.0f - coeff); brightness += inf * (1.0f - brightness); dampF += inf * (0.4999f - dampF);
    }
    fir_.configure(coeff, brightness, size, fresh_);
    iir_.setFQ(dampF, 0.5f);
    // задержка двухполюсного ФНЧ (Q 0.5) на частоте струны, в долях периода: atan(tan πf / tan πfc) / π
    const float ratio = std::tan(3.14159265358979f * frequency_) / std::tan(3.14159265358979f * dampF);
    const float comp = 1.0f - std::atan(ratio) / 3.14159265358979f;
    if (fresh_) { delay_ = delay; pos_ = clampedPos; comp_ = comp; fresh_ = false; }
    const float inv = 1.0f / static_cast<float>(size);
    const float dDelay = (delay - delay_) * inv, dPos = (clampedPos - pos_) * inv, dComp = (comp - comp_) * inv;
    for (int i = 0; i < size; i++) {
      delay_ += dDelay; pos_ += dPos; comp_ += dComp;
      const float combDelay = delay_ * pos_;
      const float d = std::max(delay_ * comp_ - 1.0f, 2.0f);
      float s = line_.readHermite(d) + in[i];
      s = fir_.process(s);
      s = iir_.lowPass(s);
      line_.write(s);
      out[i] += s;
      aux[i] += line_.read(std::max(combDelay, 1.0f));
    }
  }

 private:
  static float semis(float x) { return std::exp2(x * (1.0f / 12.0f)); }
  float sr_ = 48000, frequency_ = 220.0f / 48000, brightness_ = 0.5f, damping_ = 0.3f, position_ = 0.8f;
  float delay_ = 218, pos_ = 0.1f, comp_ = 1.0f;
  bool fresh_ = true;
  DelayLine<kStringDelay> line_;
  DampingFilter fir_;
  Svf iir_;
};

// Ноты струн (полутона MIDI) для sympathetic strings: parameter 0…1 (structure) ведёт строй от кварт-квинт к октавам;
// верхняя половина струн — те же ноты с микро-расстройкой (хор). num — число струн 2/4/8. Порт ComputeSympatheticStringsNotes (ветка без аккордов).
inline void sympatheticNotes(float tonic, float note, float parameter, float* dest, int num) {
  const float notes[9] = {tonic, note - 12.0f, note - 7.01955f, note, note + 7.01955f, note + 12.0f, note + 19.01955f, note + 24.0f, note + 24.0f};
  static const float detunings[4] = {0.013f, 0.011f, 0.007f, 0.017f};
  parameter = std::min(std::max(parameter, 0.0f), 1.0f);
  const int detuned = (num - 1) >> 1, firstDetuned = num - detuned;
  for (int i = 0; i < firstDetuned; i++) {
    float n = 3.0f;
    if (i != 0) { n = parameter * 7.0f; parameter += (1.0f - parameter) * 0.2f; }
    const int ni = std::min(static_cast<int>(n), 7);
    float fr = n - static_cast<float>(ni);
    fr = fr * fr * (3.0f - 2.0f * fr);                    // Squash
    const float a = notes[ni], b = notes[ni + 1];
    dest[i] = a + (b - a) * fr;
    if (i + firstDetuned < num) dest[i + firstDetuned] = dest[i] + detunings[i & 3];
  }
}

}  // namespace rings
