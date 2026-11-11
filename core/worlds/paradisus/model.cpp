// Paradisus — модель, см. model.hpp.
#include "model.hpp"

#include <algorithm>
#include <cmath>

#include "../../sdk/json.hpp"

namespace paradisus {

namespace {
constexpr double TAU = 6.283185307179586;
// форманты сопрано (F1–F4: частота Гц, громкость дБ, ширина Гц) — хор как в соборе; гласные a e i o u (worlds/paradisus/model.js)
struct Form { double f, db, bw; };
constexpr Form FORM[NVOW][4] = {
  {{800, 0, 80}, {1150, -6, 90}, {2900, -32, 120}, {3900, -20, 130}},    // a
  {{350, 0, 60}, {2000, -20, 100}, {2800, -15, 120}, {3600, -40, 150}},  // e
  {{270, 0, 60}, {2140, -12, 90}, {2950, -26, 100}, {3900, -26, 120}},   // i
  {{450, 0, 70}, {800, -11, 80}, {2830, -22, 100}, {3800, -22, 130}},    // o
  {{325, 0, 50}, {700, -16, 60}, {2700, -35, 170}, {3800, -40, 180}},    // u
};
const char* const FX_NAME[NFX] = {"shimmer", "decay", "tone", "mix", "cathedral", "gloria", "vibrato", "orbits"};

bool finite(double v) { return std::isfinite(v); }
double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
double frac(double x) { return x - std::floor(x); }
double wrap5(double v) { v = std::fmod(v, 5.0); if (v < 0) v += 5.0; return v >= 5.0 ? 0.0 : v; }   // NaN сюда не приходит
int clampK(double k) { return k < 0 ? 0 : k > NSPH - 1 ? NSPH - 1 : static_cast<int>(std::lround(k)); }
double cleanV(double v) { return finite(v) ? wrap5(v) : 0.0; }
}  // namespace

void Model::reset(uint32_t seed) {
  rng_ = sdk::Rng(seed);
  n_ = 0; nextId_ = -1; pinId_ = 0; nChimes_ = 0;
  fx_ = Fx();
  orbitT_ = 0; time_ = 0;
}

double Model::tonic() const { return 110.0 * std::pow(2.0, (scale_.on ? scale_.root : 0) / 12.0); }
double Model::hzOf(const Angel& a) const {
  static const double R[3] = {0.5, 1, 2};
  return tonic() * harm(a.k) * R[a.reg < 0 ? 0 : a.reg > 2 ? 2 : a.reg];
}
double Model::vEff(const Angel& a) const {
  if (!a.pinned) return a.v;
  const double ph = a.seed / 4294967296.0 * TAU;
  return wrap5(a.v + 0.45 * fx_.vibrato * std::sin(time_ * 0.25 * a.dr + ph));
}
double Model::planetAt(int k) const { return frac(orbitT_ * harm(k) + k * 0.13); }

void Model::formants(double v, double f[4], double bw[4], double g[4]) const {
  v = cleanV(v);
  const int i = static_cast<int>(std::floor(v)) % NVOW, j = (i + 1) % NVOW;
  const double t = v - std::floor(v);
  for (int q = 0; q < 4; q++) {
    const Form &A = FORM[i][q], &B = FORM[j][q];
    f[q] = A.f * std::pow(B.f / A.f, t);                // частоты по логарифму, громкость и ширина — линейно
    g[q] = A.db + (B.db - A.db) * t;
    bw[q] = A.bw + (B.bw - A.bw) * t;
  }
}

Angel* Model::findMut(int id) { for (int i = 0; i < n_; i++) if (a_[i].id == id) return &a_[i]; return nullptr; }
const Angel* Model::find(int id) const { for (int i = 0; i < n_; i++) if (a_[i].id == id) return &a_[i]; return nullptr; }
int Model::pinnedCount() const { int c = 0; for (int i = 0; i < n_; i++) c += a_[i].pinned; return c; }
void Model::removeAt(int i) { for (int j = i; j + 1 < n_; j++) a_[j] = a_[j + 1]; n_--; }

Angel* Model::add(int id, int k, double v, bool pin) {
  if (n_ >= MAX_ANGEL) return nullptr;
  Angel a;
  a.k = clampK(k); a.v = cleanV(v); a.pinned = pin; a.id = id;
  a.seed = static_cast<uint32_t>(rng_.next() * 4294967296.0);
  a.dr = 0.7 + rng_.next() * 0.6;
  a_[n_] = a;
  return &a_[n_++];
}

void Model::touch(int id, int k, double v) {
  if (Angel* a = findMut(id)) { a->k = clampK(k); a->v = cleanV(v); return; }
  add(id, k, v, false);
}

void Model::release(int id) {
  for (int i = 0; i < n_; i++) if (a_[i].id == id) { if (!a_[i].pinned) removeAt(i); return; }
}

bool Model::togglePin(int id) {
  for (int i = 0; i < n_; i++) {
    if (a_[i].id != id) continue;
    if (a_[i].pinned) { removeAt(i); return false; }
    if (pinnedCount() >= MAX_PIN) return false;
    a_[i].pinned = true;
    a_[i].id = pinId_ = nextId_--;                      // касание и закреплённый ангел дальше живут порознь
    return true;
  }
  return false;
}

void Model::setAngel(int id, int key, double val) {
  Angel* a = findMut(id);
  if (!a || !finite(val)) return;
  const double x = clamp01(val);
  switch (key) {
    case SINGERS: a->singers = std::min(5, std::max(1, static_cast<int>(std::ceil(x * 5)))); break;
    case DETUNE: a->detune = x; break;
    case VIB: a->vib = x; break;
    case BREATH: a->breath = x; break;
    case REG: a->reg = std::min(2, static_cast<int>(std::floor(x * 3))); break;
    default: break;
  }
}

void Model::setSung(int id, bool on) { if (Angel* a = findMut(id)) a->sung = on; }

void Model::setFx(int key, double val) {
  if (key < 0 || key >= NFX || !finite(val)) return;
  double* p[NFX] = {&fx_.shimmer, &fx_.decay, &fx_.tone, &fx_.mix, &fx_.cathedral, &fx_.gloria, &fx_.vibrato, &fx_.orbits};
  *p[key] = clamp01(val);
}

void Model::clear() { n_ = 0; nChimes_ = 0; }

void Model::dice() {                                    // три ангела на сферах аккорда 4·5·6 — мажорное трезвучие в чистом строе
  clear();
  static const int CH[5][3] = {{4, 5, 6}, {3, 4, 5}, {4, 6, 8}, {5, 6, 8}, {2, 3, 5}};
  const int* ch = CH[static_cast<int>(rng_.next() * 5) % 5];
  const double v0 = rng_.next() * 5;                    // гласные — вразброс по кругу, ангелы не толпятся
  for (int i = 0; i < 3; i++) { const double v = std::fmod(v0 + i * 1.7 + rng_.next() * 0.4, 5.0); add(nextId_--, 9 - ch[i], v, true); }
}

double Model::roseHz() const {
  for (int i = 0; i < n_; i++) if (a_[i].sung) return hzOf(a_[i]);
  return tonic() * 2;
}

void Model::tick(double dt) {
  if (!(dt > 0) || !finite(dt)) return;
  time_ += dt;
  const double k = 1 - std::exp(-dt / 0.3);
  for (int i = 0; i < n_; i++) a_[i].lvl += ((a_[i].sung ? 1 : 0) - a_[i].lvl) * k;
  if (fx_.orbits > 0.01) {                              // планета сферы прошла над закреплённым поющим ангелом — chime
    double prev[MAX_ANGEL];
    for (int i = 0; i < n_; i++) prev[i] = planetAt(a_[i].k);
    orbitT_ += dt / (70 / (0.3 + 1.7 * fx_.orbits));
    for (int i = 0; i < n_; i++) {
      const Angel& a = a_[i];
      if (!a.pinned) continue;
      const double p0 = prev[i], p1 = planetAt(a.k), u = a.v / 5, step = frac(p1 - p0);
      if (step < 0.5 && frac(u - p0) < step && a.sung && nChimes_ < MAX_CHIME) chimes_[nChimes_++] = Chime{a.id, hzOf(a)};
    }
  }
}

int Model::takeChimes(Chime* out, int cap) {
  const int n = std::min(nChimes_, cap);
  for (int i = 0; i < n; i++) out[i] = chimes_[i];
  nChimes_ = 0;
  return n;
}

std::string Model::snapshot() const {
  using sdk::Json;
  Json o = Json::object();
  o.set("rng", Json::num(rng_.state));
  Json as = Json::array();
  for (int i = 0; i < n_; i++) {
    const Angel& a = a_[i];
    if (!a.pinned) continue;
    Json e = Json::object();
    e.set("k", Json::num(a.k)); e.set("v", Json::num(a.v)); e.set("singers", Json::num(a.singers));
    e.set("detune", Json::num(a.detune)); e.set("vib", Json::num(a.vib)); e.set("breath", Json::num(a.breath)); e.set("reg", Json::num(a.reg));
    as.push(e);
  }
  o.set("angels", as);
  Json f = Json::object();
  const double v[NFX] = {fx_.shimmer, fx_.decay, fx_.tone, fx_.mix, fx_.cathedral, fx_.gloria, fx_.vibrato, fx_.orbits};
  for (int i = 0; i < NFX; i++) f.set(FX_NAME[i], Json::num(v[i]));
  o.set("fx", f);
  return o.dump();
}

void Model::restore(const std::string& js) {
  using sdk::Json;
  bool ok = false;
  const Json j = Json::parse(js, &ok);
  reset(1);
  if (!ok || j.type != Json::Obj) return;
  uint32_t saved = 1;
  if (const Json* v = j.get("rng")) if (v->type == Json::Num && finite(v->n)) saved = static_cast<uint32_t>(v->n < 0 ? 0 : v->n > 4294967295.0 ? 4294967295.0 : v->n);
  rng_ = sdk::Rng(saved);
  if (const Json* f = j.get("fx")) if (f->type == Json::Obj)
    for (int i = 0; i < NFX; i++) if (const Json* v = f->get(FX_NAME[i])) if (v->type == Json::Num && finite(v->n)) setFx(i, v->n);
  if (const Json* as = j.get("angels")) if (as->type == Json::Arr)
    for (const Json& e : as->a) {
      if (e.type != Json::Obj || pinnedCount() >= MAX_PIN) continue;
      const auto num = [&](const char* key, double def) { const Json* v = e.get(key); return (v && v->type == Json::Num && finite(v->n)) ? v->n : def; };
      Angel* a = add(nextId_--, clampK(num("k", 0)), num("v", 0), true);
      if (!a) break;
      a->singers = std::min(5, std::max(1, static_cast<int>(std::lround(num("singers", 4)))));
      a->detune = clamp01(num("detune", 0.5)); a->vib = clamp01(num("vib", 0.5)); a->breath = clamp01(num("breath", 0.25));
      a->reg = std::min(2, std::max(0, static_cast<int>(std::lround(num("reg", 1)))));
    }
  rng_ = sdk::Rng(saved);                               // зерна ангелов вынуты из потока — снимок после restore тот же
}

}  // namespace paradisus
