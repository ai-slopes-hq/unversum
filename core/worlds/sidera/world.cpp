// Sidera — мир SDK, см. world.hpp.
#include "world.hpp"

#include <algorithm>
#include <cmath>

#include "../../sdk/json.hpp"
#include "../../sdk/knee.hpp"

namespace sidera {

namespace {
int keyOf(double v) { return std::isfinite(v) ? static_cast<int>(std::max(-1e6, std::min(1e6, v))) : -1; }
const sdk::Knee KNEE{1, 0.6, 0.35};                    // потолок 0.95: три голоса вместе не выходят за 1
constexpr int LFO_KEYS[4] = {LVL, PAN, WD, WA};
}  // namespace

void World::prepare(double sr, uint32_t seed) {
  sr_ = sr > 0 ? sr : 48000;
  model_.reset(seed);
  for (Voice& v : voice_) v.prepare(sr_);
  env_ = 0; run_ = false;
  for (sdk::Biquad& f : flt_) f.reset();
  cut_ = model_.fx(CUTOFF); res_ = model_.fx(RESO);
}

void World::action(int kind, int i, const double* a, int n) {
  const auto has = [&](int k) { return a && n >= k; };
  switch (kind) {
    case VOICE: if (has(2)) model_.setVoice(i, keyOf(a[0]), a[1]); break;
    case STAR: if (has(3)) model_.moveHome(i, keyOf(a[0]), a[1], a[2]); break;
    case DICE: {
      int v = -1;
      if (has(1)) { if (!std::isfinite(a[0])) break; v = keyOf(a[0]); }
      if (v < -1 || v >= NV) break;
      model_.roll(v);
      break;
    }
    case CLEAR: model_.silence(); break;
    case MOON: if (has(1)) model_.setStep(a[0]); break;
    case KICK: if (has(1) && std::isfinite(a[0])) model_.kick(i, keyOf(a[0])); break;
    case FX: if (has(2)) model_.setFx(keyOf(a[0]), a[1]); break;
    case RUN: if (has(1) && !std::isnan(a[0])) run_ = a[0] != 0; break;
    case sdk::ACT_LFO: {
      if (!has(2) || i != -1) break;
      const int key = keyOf(a[0]);
      if (key == 0) model_.setStep(a[1]);
      else if (key >= 4 * NV + 1 && key <= 4 * NV + NFX) model_.setFx(key - 4 * NV - 1, a[1]);
      else if (key >= 1 && key <= 4 * NV) {
        const int v = (key - 1) / 4, k = LFO_KEYS[(key - 1) % 4];
        model_.setVoice(v, k, k == PAN ? 2 * a[1] - 1 : a[1]);
      }
      break;
    }
    default: break;
  }
}

void World::process(const sdk::Out& out, int n) {
  if (n <= 0) return;
  if (n > 128) n = 128;
  for (int k = 0; k < n; k++) out.L[k] = out.R[k] = out.sL[k] = out.sR[k] = 0.f;
  if (!run_) { env_ *= 0.9; return; }
  for (int v = 0; v < NV; v++) voice_[v].render(model_, v, out.L, out.R, out.sL, out.sR, n, model_.fx(SPACE));
  cut_ += (model_.fx(CUTOFF) - cut_) * 0.3; res_ += (model_.fx(RESO) - res_) * 0.3;     // сглаживание по блокам: без щелчка
  const double hz = 30.0 * std::pow(18000.0 / 30.0, cut_), qdb = -3.0 + 24.0 * res_;       // срез 30 Гц…18 кГц по экспоненте; Q в дБ (−3 — без горба)
  for (sdk::Biquad& f : flt_) f.set(sdk::Biquad::lowpass, std::min(hz, 0.45 * sr_), qdb, sr_);
  for (int k = 0; k < n; k++) {
    out.L[k] = static_cast<float>(flt_[0].step(out.L[k])); out.R[k] = static_cast<float>(flt_[1].step(out.R[k]));
    out.sL[k] = static_cast<float>(flt_[2].step(out.sL[k])); out.sR[k] = static_cast<float>(flt_[3].step(out.sR[k]));
  }
  for (sdk::Biquad& f : flt_) f.flushTiny(1e-15);
  double sum = 0;
  for (int k = 0; k < n; k++) {
    out.L[k] = static_cast<float>(KNEE(out.L[k]));
    out.R[k] = static_cast<float>(KNEE(out.R[k]));
    sum += std::fabs(out.L[k]) + std::fabs(out.R[k]);
  }
  env_ += (sum / (2.0 * n) - env_) * std::min(1.0, n / (0.1 * sr_));   // ~0.1 с
}

int World::mirror(double* buf, int cap) const {
  if (!buf || cap < MIRROR_N) return 0;
  double* p = buf;
  *p++ = std::min(1.0, env_); *p++ = model_.step(); *p++ = model_.fx(SPACE);
  for (int v = 0; v < NV; v++) {
    for (int k = 0; k < VSTEP; k++) *p++ = model_.voice(v, k);          // NOTE … DIST
    *p++ = std::min(1.0, voice_[v].energy());
    for (int i = 0; i < NPT_MAX; i++) { const Point q = model_.pt(v, i); *p++ = q.d; *p++ = q.a; *p++ = q.hd; *p++ = q.ha; }
  }
  for (int k = 0; k < NFX; k++) *p++ = model_.fx(k);
  for (int v = 0; v < NV; v++) *p++ = model_.voice(v, OCT);
  return MIRROR_N;
}

std::string World::snapshot() const {
  using sdk::Json;
  Json o = Json::object();
  o.set("model", Json::parse(model_.snapshot()));
  o.set("run", Json::boolean(run_));
  return o.dump();
}

void World::restore(const std::string& js) {
  using sdk::Json;
  bool ok = false;
  const Json j = Json::parse(js, &ok);
  model_.reset(1); run_ = false;
  if (!ok || j.type != Json::Obj) return;
  if (const Json* m = j.get("model")) model_.restore(m->dump());
  if (const Json* r = j.get("run")) if (r->type == Json::Bool) run_ = r->b;
}

}  // namespace sidera
