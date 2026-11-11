// Hymenaeus — модель, см. model.hpp.
#include "model.hpp"

#include <cmath>

#include "../../sdk/json.hpp"

namespace hymenaeus {

namespace {
constexpr double CLOUD_DEF[NCK] = {0.5, 0.4, 0.4, 0.5, 0, 0.3, 0.6, 0.35};   // POS SIZE DENS PITCH FREEZE ECHO CLEVEL SEND
constexpr double RING_DEF[NRK] = {0.5, 0.5, 0.5, 0.8};               // STRUCT BRIGHT DAMP LVL

bool finite(double v) { return std::isfinite(v); }
double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
}  // namespace

void Model::toss(int i) {
  p_[i].pitch = rng_.next();
  p_[i].rest = rng_.next() < REST_P;
}

void Model::reset(uint32_t seed) {
  rng_ = sdk::Rng(seed);
  len_ = PEARL_DEF; cur_ = 0; dejavu_ = 0.7;
  for (int r = 0; r < NR; r++) for (int k = 0; k < NRK; k++) ring_[r][k] = RING_DEF[k];
  for (int k = 0; k < NCK; k++) cloud_[k] = CLOUD_DEF[k];
  for (int i = 0; i < PEARL_MAX; i++) toss(i);
}

void Model::setDejaVu(double v) { if (!std::isnan(v)) dejavu_ = clamp01(v); }

void Model::setLen(double n) {
  if (!finite(n)) return;
  const long v = std::lround(n);
  len_ = v < 1 ? 1 : v > PEARL_MAX ? PEARL_MAX : static_cast<int>(v);
  cur_ %= len_;
}

void Model::togglePearl(int i) { if (i >= 0 && i < PEARL_MAX) p_[i].rest = !p_[i].rest; }
void Model::setRest(int i, bool r) { if (i >= 0 && i < PEARL_MAX) p_[i].rest = r; }
void Model::setPearlPitch(int i, double p) { if (i >= 0 && i < PEARL_MAX && !std::isnan(p)) p_[i].pitch = clamp01(p); }

void Model::setRing(int ring, int key, double v) {
  if (ring < 0 || ring >= NR || key < 0 || key >= NRK || std::isnan(v)) return;
  ring_[ring][key] = clamp01(v);
}

void Model::setCloud(int key, double v) {
  if (key < 0 || key >= NCK || std::isnan(v)) return;
  cloud_[key] = clamp01(v);
}

void Model::roll() { for (int i = 0; i < PEARL_MAX; i++) toss(i); }
void Model::clear() { for (int i = 0; i < PEARL_MAX; i++) p_[i].rest = true; }

int Model::ringFor(double pitch) const {
  if (std::isnan(pitch)) return 1;
  return pitch < 1.0 / 3 ? 0 : pitch < 2.0 / 3 ? 1 : 2;
}

bool Model::sounding() const {
  for (int i = 0; i < len_; i++) if (!p_[i].rest) return true;
  return false;
}

Step Model::next() {
  cur_ %= len_;
  const int i = cur_;
  if (rng_.next() >= dejavu_) toss(i);                  // dejavu 1: never; 0: always
  cur_ = (i + 1) % len_;
  const Pearl& p = p_[i];
  return Step{!p.rest, ringFor(p.pitch), hzOf(p.pitch), 0.8, i};
}

double Model::hzOf(double pitch) const {
  double semi = std::round(12 + clamp01(std::isnan(pitch) ? 0.5 : pitch) * 36);   // 3 октавы от A2
  if (scale_.on) semi = scale_.snapSemi(semi);
  return 55.0 * std::pow(2.0, semi / 12);
}

std::string Model::snapshot() const {
  using sdk::Json;
  Json o = Json::object();
  o.set("len", Json::num(len_));
  o.set("dejavu", Json::num(dejavu_));
  o.set("cur", Json::num(cur_));
  o.set("rng", Json::num(rng_.state));
  Json ps = Json::array();
  for (int i = 0; i < PEARL_MAX; i++) {
    Json e = Json::array(); e.push(Json::num(p_[i].pitch)); e.push(Json::num(p_[i].rest ? 1 : 0)); ps.push(e);
  }
  o.set("p", ps);
  Json rs = Json::array();
  for (int r = 0; r < NR; r++) { Json e = Json::array(); for (int k = 0; k < NRK; k++) e.push(Json::num(ring_[r][k])); rs.push(e); }
  o.set("rings", rs);
  Json cs = Json::array();
  for (int k = 0; k < NCK; k++) cs.push(Json::num(cloud_[k]));
  o.set("cloud", cs);
  return o.dump();
}

void Model::restore(const std::string& js) {
  using sdk::Json;
  bool ok = false;
  const Json j = Json::parse(js, &ok);
  reset(1);
  if (!ok || j.type != Json::Obj) return;
  const auto num = [&](const char* k, double& out) { const Json* v = j.get(k); if (v && v->type == Json::Num && finite(v->n)) out = v->n; };
  double t = 0;
  t = len_; num("len", t); setLen(t);
  t = dejavu_; num("dejavu", t); setDejaVu(t);
  t = 0; num("cur", t); cur_ = (finite(t) && t >= 0) ? static_cast<int>(std::fmod(t, len_)) : 0;
  t = 1; num("rng", t); rng_ = sdk::Rng(static_cast<uint32_t>(t < 0 ? 0 : t > 4294967295.0 ? 4294967295.0 : t));
  if (const Json* ps = j.get("p")) if (ps->type == Json::Arr)
    for (int i = 0; i < PEARL_MAX && i < static_cast<int>(ps->a.size()); i++) {
      const Json& e = ps->a[i];
      if (e.type != Json::Arr || e.a.size() < 2) continue;
      if (e.a[0].type == Json::Num) setPearlPitch(i, e.a[0].n);
      if (e.a[1].type == Json::Num) setRest(i, e.a[1].n != 0);
    }
  if (const Json* rs = j.get("rings")) if (rs->type == Json::Arr)
    for (int r = 0; r < NR && r < static_cast<int>(rs->a.size()); r++) {
      const Json& e = rs->a[r];
      if (e.type != Json::Arr) continue;
      for (int k = 0; k < NRK && k < static_cast<int>(e.a.size()); k++) if (e.a[k].type == Json::Num) setRing(r, k, e.a[k].n);
    }
  if (const Json* cs = j.get("cloud")) if (cs->type == Json::Arr)
    for (int k = 0; k < NCK && k < static_cast<int>(cs->a.size()); k++) if (cs->a[k].type == Json::Num) setCloud(k, cs->a[k].n);
}

}  // namespace hymenaeus
