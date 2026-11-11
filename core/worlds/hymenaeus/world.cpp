// Hymenaeus — мир SDK, см. world.hpp.
#include "world.hpp"

#include <algorithm>
#include <cmath>

#include "../../sdk/json.hpp"

namespace hymenaeus {

namespace {
bool whole(double v) { return std::isfinite(v); }
int keyOf(double v) { return whole(v) ? static_cast<int>(std::max(-1e6, std::min(1e6, v))) : -1; }
}  // namespace

void World::prepare(double sr, uint32_t seed) {
  sr_ = sr > 0 ? sr : 48000;
  model_.reset(seed);
  sound_.prepare(sr_, seed);
  steps_.reset(); nman_ = 0; nh_ = 0; run_ = false; bpm_ = 90; now_ = 0;
}

void World::action(int kind, int i, const double* a, int n) {
  const auto has = [&](int k) { return a && n >= k; };
  switch (kind) {
    case RING: if (has(2)) model_.setRing(i, keyOf(a[0]), a[1]); break;
    case CLOUD: if (has(2)) model_.setCloud(keyOf(a[0]), a[1]); break;
    case PEARL: {                                              // i = жемчужина, a = [нить, 0 высота | 1 пауза, v]
      if (!has(3) || i < 0 || i >= PEARL_MAX) break;
      const int t = keyOf(a[0]);
      if (t < 0 || t >= NR) break;
      if (keyOf(a[1]) == 0) model_.setPearlPitch(t, i, a[2]);
      else if (keyOf(a[1]) == 1 && !std::isnan(a[2])) model_.setRest(t, i, a[2] != 0);
      break;
    }
    case STRIKE: {
      if (i < 0 || i >= NR) break;
      const double pos = (has(1) && !std::isnan(a[0])) ? std::min(std::max(a[0], 0.0), 1.0) : 0.3;
      const double vel = (has(2) && !std::isnan(a[1])) ? std::min(std::max(a[1], 0.0), 1.0) : 1.0;
      queue(man_, nman_, MAN_MAX, Ev{0, i, -1, model_.hzOf(i, 0.5), pos, vel});
      break;
    }
    case THREAD: {                                             // i = нить, a = [0 déjà vu | 1 длина | 2 lo | 3 hi, v]
      if (!has(2) || i < 0 || i >= NR) break;
      const double v = a[1];
      switch (keyOf(a[0])) {
        case 0: model_.setDejaVu(i, v); break;
        case 1: model_.setLen(i, v); break;
        case 2: if (std::isfinite(v)) model_.setRange(i, v, model_.thread(i).hi); break;
        case 3: if (std::isfinite(v)) model_.setRange(i, model_.thread(i).lo, v); break;
        default: break;
      }
      break;
    }
    case DICE:                                                 // a = [нить]; пусто или −1 — обе; вне диапазона, NaN и ±inf — ничего
      if (!has(1)) model_.roll();
      else if (std::isfinite(a[0])) { const int t = keyOf(a[0]); if (t == -1) model_.roll(); else if (t >= 0 && t < NR) model_.roll(t); }
      break;
    case TIMBRE: model_.rollTimbre(); break;
    case LOC: if (has(1) && std::isfinite(a[0])) model_.setLoc(keyOf(a[0])); break;
    case MIX: if (has(1)) model_.setMix(a[0]); break;
    case CLEAR: model_.clearAll(); sound_.silence(); nman_ = 0; nh_ = 0; break;
    case RUN: if (has(1) && !std::isnan(a[0])) { const bool on = a[0] != 0; if (on && !run_) steps_.reset(); run_ = on; } break;
    case BPM: if (has(1) && !std::isnan(a[0]) && a[0] > 0) bpm_ = std::min(std::max(a[0], 20.0), 300.0); break;
    case sdk::ACT_LFO: {
      if (!has(2) || i != -1) break;
      const int key = keyOf(a[0]);
      if (key >= 0 && key < NR * NRK) model_.setRing(key / NRK, key % NRK, a[1]);
      else if (key >= NR * NRK && key < NR * NRK + NCK) model_.setCloud(key - NR * NRK, a[1]);
      break;
    }
    default: break;
  }
}

void World::process(const sdk::Out& out, int n) {
  if (n <= 0) return;
  if (n > Sound::BLOCK) n = Sound::BLOCK;
  const double now = clk_ ? clk_->now : now_, end = now + n / sr_;
  now_ = end;
  for (int i = 0; i < NR; i++)
    sound_.setRing(i, model_.ring(i, STRUCT), model_.ring(i, BRIGHT), model_.ring(i, DAMP), model_.ring(i, LVL), i == 1 ? 1.0 : 0.0, model_.ring(i, POLY));
  double cl[NCK]; for (int k = 0; k < NCK; k++) cl[k] = model_.cloud(k);
  sound_.setCloud(cl); sound_.setMix(model_.mix());

  Ev ev[EV_CAP]; int ne = 0;
  for (int j = 0; j < nman_; j++) queue(ev, ne, EV_CAP, man_[j]);
  nman_ = 0;
  if (run_) {
    static const sdk::Grid none;
    int seq = 0;
    steps_.run(now, end, clk_ ? clk_->grid : none, 2,
               [&](long) { return 30.0 / bpm_; },
               [&](long) { return 0.0; },
               [&](long, double t) {
                 if (seq >= MAX_EV) return;               // лавина шагов (безумная сетка) — лишние отбрасываем
                 seq++;
                 for (int th = 0; th < NR; th++) {          // обе нити шагают от общих часов
                   const Step s = model_.next(th);
                   if (!s.hit) continue;
                   const int off = std::min(std::max(static_cast<int>((t - now) * sr_), 0), n - 1);
                   queue(ev, ne, EV_CAP, Ev{off, s.ring, s.pearl, s.hz, 0.3, s.vel});
                 }
               });
  }
  std::stable_sort(ev, ev + ne, [](const Ev& a, const Ev& b) { return a.off < b.off; });

  int cur = 0;
  for (int j = 0; j < ne; j++) {
    if (ev[j].off > cur) { sound_.render(out.L + cur, out.R + cur, out.sL + cur, out.sR + cur, ev[j].off - cur); cur = ev[j].off; }
    sound_.strike(ev[j].ring, ev[j].hz, ev[j].pos, ev[j].vel);
    pushHeard(ev[j].ring, ev[j].pearl);
  }
  if (cur < n) sound_.render(out.L + cur, out.R + cur, out.sL + cur, out.sR + cur, n - cur);
}

int World::mirror(double* buf, int cap) const {
  if (!buf || cap < MIRROR_N) return 0;
  double* p = buf;
  *p++ = run_; *p++ = bpm_; *p++ = model_.loc(); *p++ = model_.mix();
  for (int t = 0; t < NR; t++) *p++ = model_.thread(t).cur;
  for (int t = 0; t < NR; t++) *p++ = model_.thread(t).len;
  for (int t = 0; t < NR; t++) *p++ = model_.thread(t).dejavu;
  for (int t = 0; t < NR; t++) *p++ = model_.thread(t).lo;
  for (int t = 0; t < NR; t++) *p++ = model_.thread(t).hi;
  for (int t = 0; t < NR; t++) for (int i = 0; i < PEARL_MAX; i++) { const Pearl q = model_.thread(t).p[i]; *p++ = q.pitch; *p++ = q.rest; }
  for (int r = 0; r < NR; r++) {
    *p++ = model_.ring(r, STRUCT); *p++ = model_.ring(r, BRIGHT); *p++ = model_.ring(r, DAMP); *p++ = model_.ring(r, LVL);
    *p++ = model_.ring(r, POLY); *p++ = sound_.ringEnergy(r);
  }
  for (int k = 0; k < NCK; k++) *p++ = model_.cloud(k);
  *p++ = sound_.cloudEnergy();
  return MIRROR_N;
}

int World::heard(double* buf, int cap) {
  if (!nh_ || !buf || cap < 1 + 2 * nh_) return 0;
  buf[0] = nh_;
  for (int j = 0; j < nh_; j++) { buf[1 + 2 * j] = hr_[j]; buf[2 + 2 * j] = hp_[j]; }
  const int k = 1 + 2 * nh_;
  nh_ = 0;
  return k;
}

std::string World::snapshot() const {
  using sdk::Json;
  Json o = Json::object();
  o.set("model", Json::parse(model_.snapshot()));
  o.set("bpm", Json::num(bpm_));
  o.set("run", Json::boolean(run_));
  return o.dump();
}

void World::restore(const std::string& js) {
  using sdk::Json;
  bool ok = false;
  const Json j = Json::parse(js, &ok);
  model_.reset(1); bpm_ = 90; run_ = false; steps_.reset();
  if (!ok || j.type != Json::Obj) return;
  if (const Json* m = j.get("model")) model_.restore(m->dump());
  if (const Json* b = j.get("bpm")) if (b->type == Json::Num && std::isfinite(b->n) && b->n > 0) bpm_ = std::min(std::max(b->n, 20.0), 300.0);
  if (const Json* r = j.get("run")) if (r->type == Json::Bool) run_ = r->b;
}

bool World::sounding() const { return (run_ && model_.sounding()) || sound_.sounding(); }

}  // namespace hymenaeus
