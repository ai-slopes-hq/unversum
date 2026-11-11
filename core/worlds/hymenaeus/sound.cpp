// Hymenaeus — звук, см. sound.hpp.
#include "sound.hpp"

#include <algorithm>
#include <cmath>

namespace hymenaeus {

void Sound::prepare(double sr, uint32_t seed) {
  sr_ = sr > 0 ? sr : 48000;
  for (int i = 0; i < NR; i++) {
    res_[i].prepare(sr_); pl_[i].prepare(sr_, seed + 17 * i + 1);
    bright_[i] = 0.5; lvl_[i] = 0.8; env_[i] = 0; pluckLeft_[i] = 0;
  }
  cloud_.prepare(sr_, seed + 101);
  send_ = 0.35; clevel_ = 0.6; cenv_ = 0;
}

void Sound::setRing(int i, double structure, double bright, double damp, double lvl) {
  if (i < 0 || i >= NR) return;
  res_[i].setStructure(structure); res_[i].setBrightness(bright); res_[i].setDamping(damp);
  bright_[i] = bright; lvl_[i] = std::min(std::max(lvl, 0.0), 1.0);
}

void Sound::setCloud(const double* c) {
  cloud_.set(c[POS], c[SIZE], c[DENS], c[PITCH], c[FREEZE] >= 0.5, c[ECHO]);
  clevel_ = c[CLEVEL]; send_ = c[SEND];
}

void Sound::strike(int ring, double hz, double pos, double vel) {
  if (ring < 0 || ring >= NR) return;
  res_[ring].setFreq(hz); res_[ring].setPosition(pos);
  pl_[ring].trigger(hz, bright_[ring], pos, vel);
  pluckLeft_[ring] = static_cast<int>(sr_ * 0.5) + 256 * 64;   // шум щипка + хвост гребёнки (0.8^64 < 1e-6)
}

bool Sound::sounding() const {
  for (int i = 0; i < NR; i++) if (res_[i].active() || pluckLeft_[i] > 0) return true;
  return !cloud_.sleeping();
}

void Sound::render(float* L, float* R, float* sL, float* sR, int n) {
  if (n <= 0) return;
  if (n > BLOCK) n = BLOCK;
  float mixL[BLOCK] = {}, mixR[BLOCK] = {}, exc[BLOCK], o[BLOCK], e[BLOCK];
  for (int i = 0; i < NR; i++) {
    if (pluckLeft_[i] > 0) { pl_[i].process(exc, n); pluckLeft_[i] -= n; } else for (int k = 0; k < n; k++) exc[k] = 0;
    res_[i].process(exc, o, e, n);
    const float g = static_cast<float>(lvl_[i] * RING_GAIN);
    float pk = 0;
    for (int k = 0; k < n; k++) { mixL[k] += o[k] * g; mixR[k] += e[k] * g; pk = std::max(pk, std::max(std::fabs(o[k]), std::fabs(e[k]))); }
    env_[i] = std::max(static_cast<double>(pk * g), env_[i] * std::pow(0.999, n));   // огибающая для картинки
    if (env_[i] < 1e-5) env_[i] = 0;
  }
  float cpk = 0;
  for (int k = 0; k < n; k++) {
    cloud_.write(mixL[k], mixR[k]);
    float cl, cr; cloud_.process(cl, cr);
    cpk = std::max(cpk, std::max(std::fabs(cl), std::fabs(cr)));
    const double l = knee_(mixL[k] + cl * clevel_), r = knee_(mixR[k] + cr * clevel_);
    L[k] = static_cast<float>(l); R[k] = static_cast<float>(r);
    sL[k] = static_cast<float>(l * send_); sR[k] = static_cast<float>(r * send_);
  }
  cenv_ = std::max(static_cast<double>(cpk * clevel_), cenv_ * std::pow(0.999, n));
  if (cenv_ < 1e-5) cenv_ = 0;
}

}  // namespace hymenaeus
