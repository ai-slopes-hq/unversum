#include "model.hpp"

#include <algorithm>
#include <cmath>

#include "../../sdk/json.hpp"

namespace sidera {

namespace {
constexpr double PI = 3.14159265358979323846;
double clampd(double x, double lo, double hi) { return x < lo ? lo : x > hi ? hi : x; }
// зеркальное отражение x в [lo, hi]: конечное, без цикла; hi ≤ lo — точка lo
double fold(double x, double lo, double hi) {
  if (!(hi > lo)) return lo;
  if (x >= lo && x <= hi) return x;                     // уже в рамке — без округления (шаг 0 не дрожит)
  const double w = hi - lo;
  double t = std::fmod(x - lo, 2 * w);
  if (t < 0) t += 2 * w;
  return lo + (t <= w ? t : 2 * w - t);
}
double dist(int kind, double u) {
  u = clampd(u, 0.001, 0.999);
  switch (kind) {
    case LOGISTIC: return 0.25 * std::log(u / (1 - u));
    case UNIFORM: return 2 * u - 1;
    default: return 0.1 * std::tan(PI * (u - 0.5));    // Коши
  }
}
constexpr double DEF[NVK] = {0, 5, 0.25, 0.3, 0.5, 0, CAUCHY, 1};
constexpr double DEF_NOTE[NV] = {0, 7, 19}, DEF_N[NV] = {5, 8, 12}, DEF_LVL[NV] = {0.5, 0.4, 0.3}, DEF_PAN[NV] = {-0.5, 0, 0.5};
}  // namespace

void Model::initPoint(int v, int i) {
  const int n = static_cast<int>(prm_[v][N]);
  Point p;
  p.hd = 0.5;
  p.ha = 0.6 * std::sin(2 * PI * (i + 0.5) / n);
  p.d = p.hd; p.a = p.ha;
  pts_[v][i] = p;
}

void Model::fit(Point& p, int v) const {
  const double wd = prm_[v][WD], wa = prm_[v][WA];
  const double dlo = std::max(D_MIN, p.hd - wd / 2), dhi = std::min(1.0, p.hd + wd / 2);
  const double alo = std::max(-1.0, p.ha - wa / 2), ahi = std::min(1.0, p.ha + wa / 2);
  p.d = clampd(dhi >= dlo ? p.d : p.hd, std::min(dlo, dhi), std::max(dlo, dhi));
  p.a = clampd(p.a, std::min(alo, ahi), std::max(alo, ahi));
}

void Model::reset(uint32_t seed) {
  step_ = 0.15;
  fx_[DRIFT] = 0.25; fx_[SPACE] = 0.3; fx_[TONE] = 0.35;
  rng_ = sdk::Rng(seed);
  for (int v = 0; v < NV; v++) {
    for (int k = 0; k < NVK; k++) prm_[v][k] = DEF[k];
    prm_[v][NOTE] = DEF_NOTE[v]; prm_[v][N] = DEF_N[v]; prm_[v][LVL] = DEF_LVL[v]; prm_[v][PAN] = DEF_PAN[v];
    vrng_[v] = sdk::Rng(seed + 1000u * static_cast<uint32_t>(v));
    for (int i = 0; i < NPT_MAX; i++) initPoint(v, i);
  }
}

void Model::setVoice(int v, int key, double val) {
  if (v < 0 || v >= NV || key < 0 || key >= NVK || std::isnan(val)) return;
  switch (key) {
    case NOTE: prm_[v][key] = clampd(std::round(val), 0, 60); break;
    case N: {
      const int old = static_cast<int>(prm_[v][N]);
      const int n = static_cast<int>(clampd(std::round(val), NPT_MIN, NPT_MAX));
      prm_[v][N] = n;
      for (int i = old; i < n; i++) initPoint(v, i);   // новые звёзды — по умолчанию для нового N; прежние не трогаются
      break;
    }
    case PAN: prm_[v][key] = clampd(val, -1, 1); break;
    case DIST: prm_[v][key] = clampd(std::round(val), 0, NDIST - 1); break;
    case VSTEP: prm_[v][key] = clampd(val, 0, 2); break;
    default: prm_[v][key] = clampd(val, 0, 1); break;  // WD WA LVL
  }
  if (key == WD || key == WA) for (int i = 0; i < NPT_MAX; i++) fit(pts_[v][i], v);
}

void Model::moveHome(int v, int pt, double hd, double ha) {
  if (v < 0 || v >= NV || pt < 0 || pt >= NPT_MAX || std::isnan(hd) || std::isnan(ha)) return;
  Point& p = pts_[v][pt];
  p.hd = clampd(hd, D_MIN, 1); p.ha = clampd(ha, -1, 1);
  fit(p, v);
}

void Model::setStep(double s) { if (!std::isnan(s)) step_ = clampd(s, 0, 1); }

void Model::roll(int v) {
  for (int k = 0; k < NV; k++) {
    if (v >= 0 && k != v) continue;
    if (prm_[k][LVL] <= 0.001) prm_[k][LVL] = DEF_LVL[k];             // после «очистить» кубик возвращает звук; ручной уровень не трогаем
    vrng_[k] = sdk::Rng(static_cast<uint32_t>(rng_.next() * 4294967296.0));
    for (int i = 0; i < NPT_MAX; i++) { pts_[k][i].d = pts_[k][i].hd; pts_[k][i].a = pts_[k][i].ha; fit(pts_[k][i], k); }
  }
}

void Model::setFx(int key, double val) { if (key >= 0 && key < NFX && !std::isnan(val)) fx_[key] = clampd(val, 0, 1); }

void Model::kick(int v, int pt) {
  if (v < 0 || v >= NV || pt < -1 || pt >= NPT_MAX) return;
  const int n = static_cast<int>(prm_[v][N]);
  for (int i = 0; i < n; i++) {
    if (pt >= 0 && i != pt) continue;
    Point& p = pts_[v][i];
    p.hd = clampd(p.hd + 0.16 * (2 * vrng_[v].next() - 1), D_MIN, 1);        // рамка уходит: созвездие заметно меняется
    p.ha = clampd(p.ha + 0.5 * (2 * vrng_[v].next() - 1), -1, 1);
    const double wd = prm_[v][WD], wa = prm_[v][WA];
    const double dlo = std::max(D_MIN, p.hd - wd / 2), dhi = std::min(1.0, p.hd + wd / 2);
    const double alo = std::max(-1.0, p.ha - wa / 2), ahi = std::min(1.0, p.ha + wa / 2);
    p.d = dlo + (std::max(dlo, dhi) - dlo) * vrng_[v].next();                // и сама точка — в случайное место рамки
    p.a = alo + (std::max(alo, ahi) - alo) * vrng_[v].next();
    fit(p, v);
  }
}

void Model::silence() { for (int v = 0; v < NV; v++) prm_[v][LVL] = 0; }

void Model::walk(int v) {
  if (v < 0 || v >= NV) return;
  const int n = static_cast<int>(prm_[v][N]);
  const int kind = static_cast<int>(prm_[v][DIST]);
  const double s = step_ * prm_[v][VSTEP], wd = prm_[v][WD], wa = prm_[v][WA];
  for (int i = 0; i < n; i++) {
    Point& p = pts_[v][i];
    const double ud = vrng_[v].next(), ua = vrng_[v].next();
    p.d = p.d + s * 0.5 * dist(kind, ud);
    p.a = p.a + s * dist(kind, ua);
    const double dlo = std::max(D_MIN, p.hd - wd / 2), dhi = std::min(1.0, p.hd + wd / 2);
    const double alo = std::max(-1.0, p.ha - wa / 2), ahi = std::min(1.0, p.ha + wa / 2);
    p.d = fold(p.d, dlo, std::max(dlo, dhi));
    p.a = fold(p.a, alo, std::max(alo, ahi));
    if (fx_[DRIFT] > 0) {                                                       // вторичное блуждание: сама рамка (центр) медленно плывёт, зеркала на краях поля
      p.hd = fold(p.hd + 0.03 * fx_[DRIFT] * (2 * vrng_[v].next() - 1), D_MIN, 1);
      p.ha = fold(p.ha + 0.05 * fx_[DRIFT] * (2 * vrng_[v].next() - 1), -1, 1);
      fit(p, v);
    }
  }
}

double Model::hzOf(int v) const {
  double semi = voice(v, NOTE);
  if (scale_.on) {                                      // snapSemi не уходит ниже ноты 0: поднимаем на целые октавы, снапим, опускаем
    const double shift = semi < 12 ? 12 * std::ceil((12 - semi) / 12) : 0;
    semi = scale_.snapSemi(semi + shift) - shift;
  }
  return 55.0 * std::pow(2.0, semi / 12);
}

bool Model::sounding() const {
  for (int v = 0; v < NV; v++) if (prm_[v][LVL] > 0.001) return true;
  return false;
}

std::string Model::snapshot() const {
  using sdk::Json;
  Json o = Json::object();
  o.set("rng", Json::num(rng_.state));
  o.set("step", Json::num(step_));
  Json fxs = Json::array();
  for (int k = 0; k < NFX; k++) fxs.push(Json::num(fx_[k]));
  o.set("fx", fxs);
  Json vs = Json::array();
  for (int v = 0; v < NV; v++) {
    Json e = Json::object();
    e.set("rng", Json::num(vrng_[v].state));
    Json p = Json::array();
    for (int k = 0; k < NVK; k++) p.push(Json::num(prm_[v][k]));
    e.set("p", p);
    Json ps = Json::array();
    for (int i = 0; i < NPT_MAX; i++) {
      Json q = Json::array();
      q.push(Json::num(pts_[v][i].d)); q.push(Json::num(pts_[v][i].a)); q.push(Json::num(pts_[v][i].hd)); q.push(Json::num(pts_[v][i].ha));
      ps.push(q);
    }
    e.set("pts", ps);
    vs.push(e);
  }
  o.set("voices", vs);
  return o.dump();
}

void Model::restore(const std::string& js) {
  using sdk::Json;
  bool ok = false;
  const Json j = Json::parse(js, &ok);
  reset(1);
  if (!ok || j.type != Json::Obj) return;
  auto seedOf = [](const Json* x, uint32_t d) { return (x && x->type == Json::Num && std::isfinite(x->n)) ? static_cast<uint32_t>(clampd(x->n, 0, 4294967295.0)) : d; };
  rng_ = sdk::Rng(seedOf(j.get("rng"), 1));
  if (const Json* s = j.get("step")) if (s->type == Json::Num) setStep(s->n);
  if (const Json* f = j.get("fx")) if (f->type == Json::Arr)
    for (int k = 0; k < NFX && k < static_cast<int>(f->a.size()); k++) if (f->a[k].type == Json::Num) setFx(k, f->a[k].n);
  const Json* vs = j.get("voices");
  if (!vs || vs->type != Json::Arr) return;
  for (int v = 0; v < NV && v < static_cast<int>(vs->a.size()); v++) {
    const Json& e = vs->a[v];
    if (e.type != Json::Obj) continue;
    vrng_[v] = sdk::Rng(seedOf(e.get("rng"), 1 + 1000u * static_cast<uint32_t>(v)));
    if (const Json* p = e.get("p")) if (p->type == Json::Arr)
      for (int k = 0; k < NVK && k < static_cast<int>(p->a.size()); k++) if (p->a[k].type == Json::Num && k != N) setVoice(v, k, p->a[k].n);
    if (const Json* p = e.get("p")) if (p->type == Json::Arr && static_cast<int>(p->a.size()) > N && p->a[N].type == Json::Num) setVoice(v, N, p->a[N].n);
    if (const Json* ps = e.get("pts")) if (ps->type == Json::Arr)
      for (int i = 0; i < NPT_MAX && i < static_cast<int>(ps->a.size()); i++) {
        const Json& q = ps->a[i];
        if (q.type != Json::Arr || q.a.size() < 4) continue;
        double f[4]; bool good = true;
        for (int c = 0; c < 4; c++) { good = good && q.a[c].type == Json::Num && std::isfinite(q.a[c].n); f[c] = q.a[c].n; }
        if (!good) continue;
        Point& pp = pts_[v][i];
        pp.hd = clampd(f[2], D_MIN, 1); pp.ha = clampd(f[3], -1, 1); pp.d = clampd(f[0], D_MIN, 1); pp.a = clampd(f[1], -1, 1);
        fit(pp, v);
      }
  }
}

}  // namespace sidera
