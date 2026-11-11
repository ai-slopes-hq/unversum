// Paradisus — звук, см. sound.hpp.
#include "sound.hpp"

#include <algorithm>
#include <cmath>

namespace paradisus {

namespace {
constexpr double TAU = 6.283185307179586;
constexpr double BWK = 2.2, FGK = 1.4, BODYK = 0.3, VCAK = 0.32, OUTK = 1.1, DBK = 0.55, DRYK = 0.6;   // engine.js
constexpr double SHIM_IN = 0.5, SHIM_OUT = 0.5, SHIM_SEND = 0.5;       // уровни шиммера: вход (запас у Shimmer — на входе до ±0.5), к сухому, к посылу
constexpr double SEND_K = 0.3;                                          // посыл в собор ядра: полный посыл engine.js давал возврат ядра ~3.5× громче сухого и пик 2–5 (длинный зал копит ровный хор)
constexpr double GRAIN_AMP = 0.06;
constexpr double QUIET = 1e-6;
double rate(double dt, double tau) { return 1 - std::exp(-dt / tau); }
bool bad(double x) { return !std::isfinite(x); }
}  // namespace

void Sound::prepare(double sr, uint32_t seed) {
  sr_ = sr > 1000 ? sr : 48000;
  rng_ = sdk::Rng(seed * 2654435761u + 6);
  sdk::Saw::init();
  shim_.prepare(sr_, BLOCK);
  hpL_ = sdk::Biquad(); hpR_ = sdk::Biquad();
  hpL_.set(sdk::Biquad::highpass, 160, 1, sr_); hpR_.set(sdk::Biquad::highpass, 160, 1, sr_);
  for (Voice& v : voice_) v = Voice();
  for (Grain& g : grain_) g = Grain();
  sg_ = tsg_ = 0.8; gloAcc_ = 0; level_ = 0; prePeak_ = 0; quiet_ = 1 << 30; ne_ = 0;
  fx_ = Fx();
}

void Sound::silence() {
  for (Voice& v : voice_) v.used = false;
  for (Grain& g : grain_) g.on = false;
  shim_.flush(); hpL_.reset(); hpR_.reset(); quiet_ = 1 << 30; level_ = 0;
}

bool Sound::sounding() const {
  if (quiet_ < static_cast<int>(0.1 * sr_)) return true;
  for (const Voice& v : voice_) if (v.used) return true;
  for (const Grain& g : grain_) if (g.on) return true;
  return false;
}

Sound::Voice* Sound::find(int id) {
  for (Voice& v : voice_) if (v.used && !v.releasing && v.id == id) return &v;
  return nullptr;
}

Sound::Voice* Sound::alloc() {
  for (Voice& v : voice_) if (!v.used) return &v;
  Voice* best = nullptr;                                  // пул полон: самый тихий из уходящих гасим быстро (~35 мс), место освободится в следующих блоках
  for (Voice& v : voice_) if (v.releasing && (!best || v.env < best->env)) best = &v;
  if (best) best->fast = true;
  return nullptr;
}

void Sound::remap(int oldId, int newId) { if (Voice* v = find(oldId)) v->id = newId; }

void Sound::aim(Voice& v, const Model& m, const Angel& a, bool snap) {
  const int N = std::min(NSAW, std::max(1, a.singers));
  for (int i = 0; i < NSAW; i++) {
    v.tg[i] = i < N ? 1.0 / std::sqrt(static_cast<double>(N)) : 0.0;
    const double cents = N > 1 ? (static_cast<double>(i) / (N - 1) - 0.5) * 2 * (4 + a.detune * 22) : 0.0;
    v.tRatio[i] = std::exp2(cents / 1200.0);
  }
  v.tHz = std::min(std::max(m.hzOf(a), 20.0), 0.45 * sr_);
  v.tBodyHz = std::min(2400.0, v.tHz * 5);
  v.tVibK = 30 * fx_.vibrato * (0.3 + a.vib * 1.4) * (std::log(2.0) / 1200.0);   // доля частоты на единицу синуса вибрато
  v.tBreath = a.breath * 0.25;
  double f[4], bw[4], g[4];
  m.formants(m.vEff(a), f, bw, g);
  for (int q = 0; q < 4; q++) v.bank.target(q, f[q], f[q] / (bw[q] * BWK), std::pow(10.0, g[q] * DBK / 20.0) * FGK);
  if (snap) {
    v.hz = v.tHz; v.bodyHz = v.tBodyHz; v.vibK = v.tVibK; v.breath = v.tBreath;
    for (int i = 0; i < NSAW; i++) { v.ratio[i] = v.tRatio[i]; v.g[i] = 0; }
    v.bank.snap();
  }
}

void Sound::begin(Voice& v, const Model& m, const Angel& a) {
  v = Voice();
  v.rng = sdk::Rng(static_cast<uint32_t>(rnd() * 4294967296.0));
  v.used = true; v.id = a.id;
  v.vinc = TAU * (5 + rnd() * 1.2) / sr_;
  v.sinc = TAU * (0.07 + rnd() * 0.08) / sr_;
  v.vph = rnd() * TAU; v.sph = rnd() * TAU;
  v.body.set(sdk::Biquad::lowpass, 550, 0.5, sr_);
  v.air.set(sdk::Biquad::highpass, 3500, 1, sr_);
  aim(v, m, a, true);
  v.bank.update(sr_);
}

void Sound::step(Model& m, double dt) {
  if (!(dt > 0) || !std::isfinite(dt)) dt = 0;
  fx_ = m.fx();
  for (Voice& v : voice_) v.held = false;
  const double cHz = rate(dt, 0.04), cBody = rate(dt, 0.05), cF = rate(dt, 0.05), cVib = rate(dt, 0.1);
  for (int i = 0; i < m.count(); i++) {
    const Angel a = m.angel(i);
    Voice* v = find(a.id);
    if (!v) {
      v = alloc();
      if (!v) continue;                                   // нет места — попробуем в следующем блоке
      begin(*v, m, a);
      m.setSung(a.id, true);
    } else {
      if (!a.sung) m.setSung(a.id, true);                 // голос уже есть (id совпал после restore) — ангел всё равно поёт
      aim(*v, m, a, false);
      v->hz += (v->tHz - v->hz) * cHz;
      v->bodyHz += (v->tBodyHz - v->bodyHz) * cBody;
      v->vibK += (v->tVibK - v->vibK) * cVib; v->breath += (v->tBreath - v->breath) * cVib;
      for (int s = 0; s < NSAW; s++) v->ratio[s] += (v->tRatio[s] - v->ratio[s]) * cHz;
      v->bank.glide(cF, cF, cF);
    }
    v->held = true;
  }
  for (Voice& v : voice_) if (v.used && !v.held) v.releasing = true;
  for (Voice& v : voice_) if (v.used) {
    v.bank.update(sr_);
    v.body.set(sdk::Biquad::lowpass, std::min(v.bodyHz, 0.45 * sr_), 0.5, sr_);
  }

  m.tick(dt);
  Chime ch[MAX_CHIME];
  const int nc = m.takeChimes(ch, MAX_CHIME);
  for (int i = 0; i < nc; i++) { scatter(ch[i].hz, 6, 0.045); note(H_CHIME, ch[i].id); }

  double sing[MAX_ANGEL]; int ns = 0;
  for (int i = 0; i < m.count(); i++) if (m.angel(i).sung) sing[ns++] = m.hzOf(m.angel(i));
  if (fx_.gloria > 0.01 && ns > 0) {                      // Gloria: ∝ времени и fx.gloria, на гармонике случайного поющего
    gloAcc_ += dt * fx_.gloria * 24;
    for (int guard = 0; gloAcc_ >= 1 && guard < 8; guard++) { gloAcc_ -= 1; spawn(sing[static_cast<int>(rnd() * ns) % ns], 0); }
    if (gloAcc_ > 8) gloAcc_ = 0;
  } else gloAcc_ = 0;

  shim_.set(static_cast<float>(fx_.shimmer), static_cast<float>(fx_.decay), static_cast<float>(fx_.tone), static_cast<float>(fx_.mix));
  const double c = fx_.cathedral;
  tsg_ = SEND_K * (0.35 + c * 0.5) * (0.9 + c * 0.5) * OUTK / 0.9;
}

void Sound::scatter(double hz, int n, double gapSec) {
  for (int i = 0; i < n; i++) spawn(hz, static_cast<int>(i * gapSec * sr_));
}

void Sound::spawn(double hz, int wait) {
  Grain* g = nullptr;
  for (Grain& x : grain_) if (!x.on) { g = &x; break; }
  const double dur = (0.06 + rnd() * 0.14) * sr_;
  const int h = 1 + static_cast<int>(rnd() * 4) % 4;
  const double oct = rnd() < 0.3 ? 2 : 1, pan = rnd() * 1.6 - 0.8;
  if (!g) return;                                         // пул полон — зерно пропало (случайные числа уже взяты: поток от занятости не зависит)
  double f = std::isfinite(hz) ? hz * h * oct : 440;
  f = std::min(std::max(f, 20.0), 0.45 * sr_);
  *g = Grain();
  g->on = true; g->wait = std::max(0, wait);
  g->dur = std::max(8, static_cast<int>(dur)); g->att = static_cast<int>(g->dur * 0.4); g->tail = std::max(1, static_cast<int>(0.01 * sr_));
  g->inc = TAU * f / sr_;
  g->fac = std::pow(0.0005 / GRAIN_AMP, 1.0 / std::max(1, g->dur - g->att));
  g->pan = pan;
  const double ang = (pan + 1) * M_PI / 4;
  g->pl = std::cos(ang); g->pr = std::sin(ang);
}

int Sound::takeEvents(double* buf, int cap) {
  const int n = std::min(ne_, cap / 2);
  for (int i = 0; i < n; i++) { buf[2 * i] = et_[i]; buf[2 * i + 1] = ea_[i]; }
  ne_ = 0;
  return n;
}

void Sound::render(float* L, float* R, float* sL, float* sR, int n) {
  if (n <= 0) return;
  if (n > BLOCK) n = BLOCK;
  bool active = quiet_ < static_cast<int>(0.1 * sr_);
  for (const Voice& v : voice_) active = active || v.used;
  for (const Grain& g : grain_) active = active || g.on;
  if (!active) {                                          // тишина: ничего не считаем
    for (int i = 0; i < n; i++) L[i] = R[i] = sL[i] = sR[i] = 0.f;
    level_ *= std::exp(-n / (0.2 * sr_));
    return;
  }
  double inL[BLOCK], inR[BLOCK], mono[BLOCK];
  for (int i = 0; i < n; i++) inL[i] = inR[i] = mono[i] = 0;

  const double kEnv = 1 - std::exp(-1.0 / (0.35 * sr_)), kRel = 1 - std::exp(-1.0 / (0.5 * sr_)), kFast = 1 - std::exp(-1.0 / (0.005 * sr_)), kG = 1 - std::exp(-1.0 / (0.02 * sr_));
  for (Voice& v : voice_) {
    if (!v.used) continue;
    double sum = 0, tmp[BLOCK];
    for (int i = 0; i < n; i++) {
      v.env += ((v.releasing ? 0.0 : 1.0) - v.env) * (v.releasing ? (v.fast ? kFast : kRel) : kEnv);
      const double vib = std::sin(v.vph); v.vph += v.vinc; if (v.vph > TAU) v.vph -= TAU;
      const double sw = std::sin(v.sph); v.sph += v.sinc; if (v.sph > TAU) v.sph -= TAU;
      double src = 0;
      for (int s = 0; s < NSAW; s++) {
        v.g[s] += (v.tg[s] - v.g[s]) * kG;
        if (!v.on[s]) {
          if (v.tg[s] <= 0) continue;
          v.on[s] = true; v.saw[s].reset(v.rng.next());
        } else if (v.tg[s] <= 0 && v.g[s] < 1e-5) { v.on[s] = false; v.g[s] = 0; continue; }
        src += v.g[s] * v.saw[s].step(v.hz * v.ratio[s] * (1 + v.vibK * vib) / sr_);
      }
      const double nz = (v.rng.next() * 2 - 1) * v.breath;
      double y = v.bank.step(src + nz) + BODYK * v.body.step(src) + 0.5 * v.air.step(nz);
      y *= v.env * (VCAK + 0.12 * sw);
      tmp[i] = y; sum += y;
    }
    if (bad(sum)) { v.used = false; continue; }          // залипший голос — убрать (редкая страховка)
    for (int i = 0; i < n; i++) mono[i] += tmp[i];
    if (v.releasing && v.env < 1e-3) { v.used = false; v.bank.reset(); v.body.reset(); v.air.reset(); }
  }
  for (int i = 0; i < n; i++) inL[i] = inR[i] = mono[i];

  for (Grain& g : grain_) {
    if (!g.on) continue;
    for (int i = 0; i < n; i++) {
      if (g.wait > 0) { g.wait--; continue; }
      if (g.pos == 0) note(H_GRAIN, g.pan);
      if (g.pos < g.att) g.env = GRAIN_AMP * g.pos / g.att;
      else if (g.pos < g.dur) g.env = g.pos == g.att ? GRAIN_AMP : g.env * g.fac;
      else { if (g.pos == g.dur) g.e0 = g.env; g.env = g.e0 * (1.0 - static_cast<double>(g.pos - g.dur) / g.tail); }
      const double s = std::sin(g.ph) * g.env; g.ph += g.inc; if (g.ph > TAU) g.ph -= TAU;
      inL[i] += s * g.pl; inR[i] += s * g.pr;
      if (++g.pos >= g.dur + g.tail) { g.on = false; break; }
    }
  }

  float shin[BLOCK], shL[BLOCK], shR[BLOCK];
  for (int i = 0; i < n; i++) shin[i] = static_cast<float>(0.5 * (inL[i] + inR[i]) * SHIM_IN);
  shim_.process(shin, shin, shL, shR, n);

  double e = 0, pk = 0, lev = level_;
  const double kS = 1 - std::exp(-1.0 / (0.05 * sr_)), dec = std::exp(-1.0 / (0.2 * sr_));
  for (int i = 0; i < n; i++) {
    sg_ += (tsg_ - sg_) * kS;
    const double w = shR[i];
    e = std::max(e, std::fabs(w) + std::fabs(shin[i]));
    double dl = inL[i] * DRYK * OUTK + w * SHIM_OUT, dr = inR[i] * DRYK * OUTK + w * SHIM_OUT;
    double xl = hpL_.step(inL[i] * sg_) + w * SHIM_SEND, xr = hpR_.step(inR[i] * sg_) + w * SHIM_SEND;
    if (bad(dl) || bad(dr) || bad(xl) || bad(xr)) { dl = dr = xl = xr = 0; hpL_.reset(); hpR_.reset(); }
    pk = std::max(pk, std::max(std::fabs(dl), std::fabs(dr)));
    const double ol = dryKnee_(dl), or_ = dryKnee_(dr);
    L[i] = static_cast<float>(ol); R[i] = static_cast<float>(or_);
    sL[i] = static_cast<float>(sendKnee_(xl)); sR[i] = static_cast<float>(sendKnee_(xr));
    lev = std::max(lev * dec, std::max(std::fabs(ol), std::fabs(or_)));
  }
  level_ = lev; prePeak_ = std::max(prePeak_, pk);
  if (e < QUIET) quiet_ = std::min(quiet_ + n, 1 << 30); else quiet_ = 0;
  if (quiet_ >= static_cast<int>(0.1 * sr_)) { shim_.flush(); hpL_.reset(); hpR_.reset(); }
}

}  // namespace paradisus
