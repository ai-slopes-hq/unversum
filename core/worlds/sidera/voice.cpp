#include "voice.hpp"

#include <algorithm>
#include <cmath>

#include "../../sdk/pan.hpp"

namespace sidera {

void Voice::prepare(double sr) {
  sr_ = sr > 1000 ? sr : 48000;
  n_ = seg_ = 0; pos_ = period_ = 0; started_ = false;
  gain_ = env_ = 0; pan_ = 2;
  dc_ = sdk::DcBlock(); lp_ = sdk::OnePole();
}

void Voice::startPeriod(const Model& m, int v) {
  n_ = std::max(NPT_MIN, std::min(NPT_MAX, static_cast<int>(m.voice(v, N))));
  const double f0 = std::max(m.hzOf(v), 1.0);
  period_ = std::max(sr_ / f0, static_cast<double>(n_));   // короче N сэмплов период не бывает: звук ниже нормы, но конечный
  double sum = 0;
  for (int i = 0; i < n_; i++) sum += m.pt(v, i).d;
  for (int i = 0; i < n_; i++) {
    const Point p = m.pt(v, i);
    amp_[i] = p.a;
    len_[i] = 1 + (period_ - n_) * (sum > 0 ? p.d / sum : 1.0 / n_);   // каждый отрезок ≥ 1 сэмпл, сумма = период
  }
  seg_ = 0;                                              // pos_ (дробный остаток) переходит в новый период: средняя высота точна
  started_ = true;
}

void Voice::render(Model& m, int v, float* L, float* R, float* sL, float* sR, int n, double send) {
  if (v < 0 || v >= NV || n <= 0) return;
  const double target = m.voice(v, LVL);
  if (target <= 0 && gain_ == 0) {                       // неслышимый голос отцеплен: состояние сброшено, блуждание стоит
    dc_ = sdk::DcBlock(); lp_ = sdk::OnePole(); env_ = 0; started_ = false;
    return;
  }
  const double pan = m.voice(v, PAN);
  if (pan != pan_) { pan_ = pan; sdk::panEq(pan, gl_, gr_); }
  if (!started_) { startPeriod(m, v); pos_ = 0; gain_ = 0; }          // подъём с нуля нарастает (≈10 мс), без ступеньки
  for (int i = 0; i < n; i++) {
    const int nx = seg_ + 1 < n_ ? seg_ + 1 : 0;
    const double x = amp_[seg_] + (amp_[nx] - amp_[seg_]) * (pos_ / len_[seg_]);
    pos_ += 1;
    if (pos_ >= len_[seg_]) {
      pos_ -= len_[seg_];
      if (++seg_ >= n_) { m.walk(v); startPeriod(m, v); }
    }
    gain_ += (target - gain_) * 0.002;
    if (target <= 0 && gain_ < 1e-5) gain_ = 0;
    double y = lp_.step(dc_.step(x), 0.35);
    if (!std::isfinite(y)) { y = 0; dc_ = sdk::DcBlock(); lp_ = sdk::OnePole(); }
    y *= gain_;
    env_ += (std::fabs(y) - env_) * 0.002;
    const float l = static_cast<float>(y * gl_), r = static_cast<float>(y * gr_);
    L[i] += l; R[i] += r;
    if (sL) sL[i] += static_cast<float>(l * send);
    if (sR) sR[i] += static_cast<float>(r * send);
  }
}

}  // namespace sidera
