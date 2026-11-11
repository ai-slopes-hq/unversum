// Paradisus — мир SDK, см. world.hpp.
#include "world.hpp"

#include <algorithm>
#include <cmath>

#include "../../sdk/json.hpp"

namespace paradisus {

namespace {
int keyOf(double v) { return std::isfinite(v) ? static_cast<int>(std::max(-1e6, std::min(1e6, v))) : -1; }
}  // namespace

void World::prepare(double sr, uint32_t seed) {
  sr_ = sr > 0 ? sr : 48000;
  model_.reset(seed);
  sound_.prepare(sr_, seed);
}

void World::action(int kind, int i, const double* a, int n) {
  const auto has = [&](int k) { return a && n >= k; };
  switch (kind) {
    case TOUCH: if ((i >= 0 || model_.find(i)) && has(2)) model_.touch(i, std::isfinite(a[0]) ? static_cast<int>(std::lround(std::max(-100.0, std::min(100.0, a[0])))) : 0, a[1]); break;
    case RELEASE: model_.release(i); break;
    case PIN: {
      const bool was = model_.find(i) != nullptr;
      if (was && model_.togglePin(i)) sound_.remap(i, model_.pinId());   // тот же голос продолжает петь под новым id
      break;
    }
    case VOWEL: if (has(1)) if (const Angel* an = model_.find(i)) model_.touch(i, an->k, a[0]); break;
    case ROSE: sound_.scatter(model_.roseHz(), 12, 0.035); break;
    case FX: if (has(2)) model_.setFx(keyOf(a[0]), a[1]); break;
    case ANGEL: if (has(2)) model_.setAngel(i, keyOf(a[0]), a[1]); break;
    case DICE: model_.dice(); break;
    case CLEAR: model_.clear(); break;
    case sdk::ACT_LFO: if (i == -1 && has(2)) model_.setFx(keyOf(a[0]), a[1]); break;
    default: break;
  }
}

void World::process(const sdk::Out& out, int n) {
  for (int off = 0; off < n; off += Sound::BLOCK) {
    const int m = std::min(Sound::BLOCK, n - off);
    sound_.step(model_, m / sr_);
    sound_.render(out.L + off, out.R + off, out.sL + off, out.sR + off, m);
  }
}

int World::mirror(double* buf, int cap) const {
  if (!buf || cap < MIRROR_N) return 0;
  std::fill(buf, buf + MIRROR_N, 0.0);
  const Fx& f = model_.fx();
  buf[0] = model_.count(); buf[1] = model_.pinnedCount(); buf[2] = model_.orbitT();
  const double fx[NFX] = {f.shimmer, f.decay, f.tone, f.mix, f.cathedral, f.gloria, f.vibrato, f.orbits};
  for (int k = 0; k < NFX; k++) buf[3 + k] = fx[k];
  buf[11] = sound_.level();
  for (int j = 0; j < model_.count(); j++) {
    const Angel& an = model_.angel(j);
    double* p = buf + MIRROR_HEAD + MIRROR_ANGEL * j;
    p[0] = an.id; p[1] = an.k; p[2] = an.v; p[3] = model_.vEff(an); p[4] = an.pinned; p[5] = an.lvl;
    p[6] = an.singers; p[7] = an.detune; p[8] = an.vib; p[9] = an.breath; p[10] = an.reg;
  }
  return MIRROR_N;
}

int World::heard(double* buf, int cap) {
  if (!buf || cap < 1) return 0;
  const int k = sound_.takeEvents(buf + 1, cap - 1);
  if (!k) return 0;
  buf[0] = k;
  return 1 + 2 * k;
}

std::string World::snapshot() const {
  using sdk::Json;
  Json o = Json::object();
  o.set("model", Json::parse(model_.snapshot()));         // ангелы-закреплённые, зерно и fx (внутри модели, ключ "fx")
  return o.dump();
}

void World::restore(const std::string& js) {
  using sdk::Json;
  bool ok = false;
  const Json j = Json::parse(js, &ok);
  model_.reset(1);
  if (!ok || j.type != Json::Obj) return;
  if (const Json* m = j.get("model")) model_.restore(m->dump());
  else model_.restore(js);                                // прежний формат: снимок модели прямо в корне ({angels, fx})
}

}  // namespace paradisus
