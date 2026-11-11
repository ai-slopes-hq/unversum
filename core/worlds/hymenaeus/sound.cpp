// Hymenaeus — звук, см. sound.hpp.
#include "sound.hpp"

#include <algorithm>
#include <cmath>

namespace hymenaeus {

namespace {
constexpr double STR_GAIN = 0.5;                         // струны громче modal в 2–3 раза по rms (замер 11.11) — ставим вровень
double midiOf(double hz) { return 69.0 + 12.0 * std::log2(std::max(hz, 1.0) / 440.0); }
}  // namespace

void Sound::prepare(double sr, uint32_t seed) {
  sr_ = sr > 0 ? sr : 48000;
  for (int i = 0; i < NR; i++) {
    for (int v = 0; v < VOICES; v++) {
      res_[i][v].prepare(sr_); pl_[i][v].prepare(sr_, seed + 17 * (i * VOICES + v) + 1);
      pluckLeft_[i][v] = 0; vmode_[i][v] = 0; note_[i][v] = 57; dcx_[i][v] = dcy_[i][v] = 0;
    }
    for (int s = 0; s < STRINGS; s++) { str_[i][s].prepare(sr_); lfo_[i][s] = 0; }
    structure_[i] = 0.5; bright_[i] = 0.5; damp_[i] = 0.5; lvl_[i] = 0.8; pos_[i] = 0.3; env_[i] = 0;
    mode_[i] = 0; poly_[i] = polyS_[i] = 1; next_[i] = 0; strActive_[i] = false;
  }
  cloud_.prepare(sr_, seed + 101);
  send_ = 0.35; clevel_ = 0.6; cenv_ = 0;
}

void Sound::setRing(int i, double structure, double bright, double damp, double lvl, double mode, double poly) {
  if (i < 0 || i >= NR) return;
  for (int v = 0; v < VOICES; v++) { res_[i][v].setStructure(structure); res_[i][v].setBrightness(bright); res_[i][v].setDamping(damp); }
  const auto c01 = [](double x, double d) { return std::isnan(x) ? d : std::min(std::max(x, 0.0), 1.0); };
  structure_[i] = c01(structure, structure_[i]); bright_[i] = c01(bright, bright_[i]); damp_[i] = c01(damp, damp_[i]);
  lvl_[i] = c01(lvl, lvl_[i]);
  if (!std::isnan(mode)) mode_[i] = modeOf(mode);
  if (!std::isnan(poly)) poly_[i] = polyOf(poly);
}

void Sound::setCloud(const double* c) {
  cloud_.set(c[POS], c[SIZE], c[DENS], c[PITCH], c[FREEZE] >= 0.5, c[ECHO]);
  clevel_ = c[CLEVEL]; send_ = c[SEND];
}

void Sound::strike(int ring, double hz, double pos, double vel) {
  if (ring < 0 || ring >= NR) return;
  const int p = poly_[ring];
  if (mode_[ring] == 1 && p != polyS_[ring]) {             // раскладка струн зависит от полифонии: при смене — заново (под новую атаку)
    for (int s = 0; s < STRINGS; s++) str_[ring][s].reset();
    polyS_[ring] = p; strActive_[ring] = false;
    for (int v = 0; v < VOICES; v++) if (vmode_[ring][v] == 1) pluckLeft_[ring][v] = 0;
  }
  const int v = next_[ring] % p;
  next_[ring] = (v + 1) % p;
  vmode_[ring][v] = mode_[ring];
  pos_[ring] = std::min(std::max(std::isnan(pos) ? 0.3 : pos, 0.0), 1.0);
  if (mode_[ring] == 0) { res_[ring][v].setFreq(hz); res_[ring][v].setPosition(pos); }
  else { note_[ring][v] = midiOf(hz); strActive_[ring] = true; }
  pl_[ring][v].trigger(hz, bright_[ring], pos, vel);
  pluckLeft_[ring][v] = static_cast<int>(sr_ * 0.5) + 256 * 64;   // шум щипка + хвост гребёнки (0.8^64 < 1e-6)
}

bool Sound::sounding() const {
  for (int i = 0; i < NR; i++) {
    if (strActive_[i]) return true;
    for (int v = 0; v < VOICES; v++) if (res_[i][v].active() || pluckLeft_[i][v] > 0) return true;
  }
  return !cloud_.sleeping();
}

void Sound::renderStrings(int r, const float exc[VOICES][BLOCK], float* o, float* e, int n) {
  const int p = polyS_[r], ns = STRINGS / p;
  const float dcPole = 1.0f - 20.0f / static_cast<float>(sr_);
  float peak = 0;
  for (int v = 0; v < p; v++) {
    float in[BLOCK], vo[BLOCK] = {}, va[BLOCK] = {}, sym[BLOCK];
    for (int k = 0; k < n; k++) {
      const float x = vmode_[r][v] == 1 ? exc[v][k] : 0.0f;
      const float y = x - dcx_[r][v] + dcPole * dcy_[r][v];
      dcx_[r][v] = x; dcy_[r][v] = y; in[k] = y;
    }
    float semis[STRINGS], tonic = static_cast<float>(note_[r][v] - 24.0);
    rings::sympatheticNotes(tonic, static_cast<float>(note_[r][v]), static_cast<float>(structure_[r]), semis, ns);
    const float b = static_cast<float>(bright_[r]);
    // у сочувственных струн своя яркость и глайд (оригинал считает на блок 24 сэмпла — приводим к длине блока)
    float b2 = b * (2.0f - b); b2 *= (2.0f - b2);
    const float glideBase = std::exp2((b2 - 1.0f) * 3.0f);
    const float glide = 1.0f - std::pow(1.0f - glideBase, static_cast<float>(n) / 24.0f);
    const float damp = std::min(static_cast<float>(1.0 - damp_[r]), 0.9f);   // у Rings «больше — дольше»; выше 0.9 — бесконечный звон, не берём
    for (int s = 0; s < ns; s++) {
      const int i = v + s * p;
      rings::String& S = str_[r][i];
      double& ph = lfo_[r][i];
      static const double lfoHz[STRINGS] = {0.5, 0.4, 0.35, 0.23, 0.211, 0.2, 0.171, 0.0};
      ph += lfoHz[i] * n / sr_; ph -= std::floor(ph);
      const float lfo = static_cast<float>(std::cos(6.28318530717959 * ph));
      float brightness = b, damping = damp, position = static_cast<float>(pos_[r]), g = 1.0f;
      const float* src = in;
      if (s > 0) {
        brightness = b2;
        damping = 0.7f + damp * 0.27f;
        position = position + lfo * (0.5f - std::fabs(0.5f - position)) * 0.9f;
        g = glide; src = sym;
      }
      S.setFrequency(static_cast<float>(440.0 * std::exp2((semis[s] - 69.0) / 12.0) / sr_), g);
      S.setBrightness(brightness); S.setPosition(position);
      S.setDamping(damping + (static_cast<float>(s) / ns) * (0.95f - damping));
      S.process(src, vo, va, n);
      if (s == 0) { const float gain = 0.2f / static_cast<float>(ns); for (int k = 0; k < n; k++) sym[k] = gain * (vo[k] - va[k]); }
    }
    for (int k = 0; k < n; k++) { o[k] += vo[k] * static_cast<float>(STR_GAIN); e[k] += va[k] * static_cast<float>(STR_GAIN); }
  }
  for (int k = 0; k < n; k++) peak = std::max(peak, std::max(std::fabs(o[k]), std::fabs(e[k])));
  bool anyPluck = false;
  for (int v = 0; v < VOICES; v++) if (vmode_[r][v] == 1 && pluckLeft_[r][v] > 0) anyPluck = true;
  if (!anyPluck && peak < 1e-5f) {                         // дозвенело: чистим линии, чтобы спать без денормалов
    for (int s = 0; s < STRINGS; s++) str_[r][s].reset();
    for (int v = 0; v < VOICES; v++) dcx_[r][v] = dcy_[r][v] = 0;
    strActive_[r] = false;
    for (int k = 0; k < n; k++) o[k] = e[k] = 0;
  }
}

void Sound::renderRing(int r, float* o, float* e, int n) {
  float exc[VOICES][BLOCK], ro[BLOCK], re[BLOCK], so[BLOCK] = {}, se[BLOCK] = {};
  for (int v = 0; v < VOICES; v++) {
    if (pluckLeft_[r][v] > 0) { pl_[r][v].process(exc[v], n); pluckLeft_[r][v] -= n; } else for (int k = 0; k < n; k++) exc[v][k] = 0;
  }
  for (int k = 0; k < n; k++) o[k] = e[k] = 0;
  for (int v = 0; v < VOICES; v++) {
    static const float zero[BLOCK] = {};
    res_[r][v].process(vmode_[r][v] == 0 ? exc[v] : zero, ro, re, n);
    for (int k = 0; k < n; k++) { o[k] += ro[k]; e[k] += re[k]; }
  }
  if (strActive_[r]) {
    renderStrings(r, exc, so, se, n);
    for (int k = 0; k < n; k++) { o[k] += so[k]; e[k] += se[k]; }
  }
}

void Sound::render(float* L, float* R, float* sL, float* sR, int n) {
  if (n <= 0) return;
  if (n > BLOCK) n = BLOCK;
  float mixL[BLOCK] = {}, mixR[BLOCK] = {}, o[BLOCK], e[BLOCK];
  for (int i = 0; i < NR; i++) {
    renderRing(i, o, e, n);
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
