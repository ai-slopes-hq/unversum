// Облака Hymenaeus, см. cloud.hpp.
#include "cloud.hpp"

#include <algorithm>
#include <cmath>

namespace hymenaeus {

namespace {
float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
float soft(float x) { return x / (1.0f + 0.25f * std::fabs(x)); }   // |soft| < 4
}  // namespace

void Cloud::prepare(double sr, uint32_t seed) {
  sr_ = static_cast<float>(sr > 0 ? sr : 48000);
  len_ = std::min(MAX_FRAMES, static_cast<int>(3 * sr_));
  for (int c = 0; c < 2; c++) std::fill(buf_[c], buf_[c] + MAX_FRAMES, 0.0f);
  for (int i = 0; i <= 512; i++) win_[i] = 0.5f - 0.5f * std::cos(6.28318530717959f * i / 512);
  ng_ = 0; wp_ = 0; written_ = 0; silentRun_ = 0; hasSound_ = false; frozen_ = false;
  pos_ = 0.5f; size_ = 0.4f; dens_ = 0.4f; pitch_ = 0.5f; echo_ = 0.3f; phase_ = 0; gain_ = 1; fbL_ = fbR_ = 0;
  rng_ = sdk::Rng(seed);
}

void Cloud::set(double pos, double size, double dens, double pitch, bool freeze, double echo) {
  const auto put = [](float& dst, double v) { if (!std::isnan(v)) dst = clamp01(static_cast<float>(std::min(std::max(v, -1e9), 1e9))); };
  put(pos_, pos); put(size_, size); put(dens_, dens); put(pitch_, pitch); put(echo_, echo);
  frozen_ = freeze;
  const float gps = 1.0f + dens_ * dens_ * 40.0f, sec = 0.03f + size_ * size_ * 0.53f;
  gain_ = 1.0f / std::sqrt(std::max(1.0f, gps * sec));   // нормировка по ожидаемому перекрытию зёрен
}

void Cloud::write(float l, float r) {
  if (frozen_) return;
  const float fk = echo_ * 0.7f * gain_;                  // gain_ = 1/√перекрытия; ещё раз — когерентные копии складываются линейно, петля < 1
  float vl = l + soft(fk * fbL_), vr = r + soft(fk * fbR_);
  if (!(std::fabs(vl) > 1e-9f)) vl = 0;                  // NaN тоже сюда: в буфер не попадает
  if (!(std::fabs(vr) > 1e-9f)) vr = 0;
  if (!hasSound_ && vl == 0 && vr == 0) return;           // спим: в буфере и так ноль
  buf_[0][wp_] = vl; buf_[1][wp_] = vr;
  wp_ = (wp_ + 1) % len_;
  if (written_ < len_) written_++;
  if (std::fabs(vl) > 1e-6f || std::fabs(vr) > 1e-6f) { silentRun_ = 0; hasSound_ = true; }
  else if (++silentRun_ >= len_) hasSound_ = false;       // весь буфер перезаписан тишиной
}

float Cloud::tap(const float* b, float idx) const {
  while (idx < 0) idx += len_;
  const int i = static_cast<int>(idx), j = (i + 1) % len_;
  const float f = idx - i;
  return b[i % len_] + (b[j] - b[i % len_]) * f;
}

void Cloud::spawn() {
  if (ng_ >= MAX_GRAINS || written_ < 256) return;
  const float sec = 0.03f + size_ * size_ * 0.53f, lenF = sec * sr_;
  const float semi = (pitch_ - 0.5f) * 24.0f;
  const float rate = std::pow(2.0f, semi / 12.0f);
  float d = 64.0f + pos_ * (static_cast<float>(written_) - 128.0f - lenF * 0.5f) + (static_cast<float>(rng_.next()) - 0.5f) * 0.5f * lenF;
  d = std::min(std::max(d, 4.0f), static_cast<float>(written_) - 4.0f);
  const float pan = static_cast<float>(rng_.next()) * 1.5707963f;
  g_[ng_++] = Grain{d, rate, 0.0f, 1.0f / lenF, std::cos(pan), std::sin(pan)};
}

void Cloud::process(float& l, float& r) {
  l = r = 0;
  if (sleeping()) { fbL_ = fbR_ = 0; return; }
  if (hasSound_ && written_ >= 256) {
    const float gps = 1.0f + dens_ * dens_ * 40.0f;
    phase_ += gps / sr_;
    if (phase_ >= 1.0f) { phase_ -= 0.5f + static_cast<float>(rng_.next()); if (phase_ < 0) phase_ = 0; spawn(); }
  }
  float sl = 0, sr = 0;
  for (int k = 0; k < ng_;) {
    Grain& g = g_[k];
    const float t = g.age * g.inv;
    bool dead = t >= 1.0f || g.dist < 2.0f || g.dist > static_cast<float>(written_) - 2.0f;
    if (!dead) {
      const float wf = t * 512.0f; const int wi = static_cast<int>(wf);
      const float w = win_[wi] + (win_[wi + 1 > 512 ? 512 : wi + 1] - win_[wi]) * (wf - wi);
      const float idx = static_cast<float>(wp_) - g.dist;
      sl += tap(buf_[0], idx) * w * g.gl; sr += tap(buf_[1], idx) * w * g.gr;
      g.age += 1.0f;
      g.dist += (frozen_ ? 0.0f : 1.0f) - g.rate;
      k++;
    } else g = g_[--ng_];                                 // убрать зерно: последнее встаёт на его место
  }
  fbL_ = sl * gain_; fbR_ = sr * gain_;
  l = soft(fbL_); r = soft(fbR_);
}

}  // namespace hymenaeus
