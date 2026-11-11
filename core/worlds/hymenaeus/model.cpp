// Hymenaeus — модель, см. model.hpp.
#include "model.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "../../sdk/json.hpp"

namespace hymenaeus {

namespace {
constexpr double CLOUD_DEF[NCK] = {0.5, 0.4, 0.4, 0.5, 0, 0.3, 0.6, 0.35};   // POS SIZE DENS PITCH FREEZE ECHO CLEVEL SEND
constexpr double RING_DEF[NRK] = {0.5, 0.5, 0.5, 0.8, 0};             // STRUCT BRIGHT DAMP LVL POLY
constexpr int LO_DEF[NR] = {47, 59}, HI_DEF[NR] = {67, 79};             // резонатор B2…G4 (тоника A3 в середине), струна B3…G5 (тоника A4); ~1,7 октавы

bool finite(double v) { return std::isfinite(v); }
double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

void fixRange(int& a, int& b) {                         // уже целые ноты: порядок, граница 24…96, ширина ≥ 5
  a = std::min(std::max(a, NOTE_MIN), NOTE_MAX); b = std::min(std::max(b, NOTE_MIN), NOTE_MAX);
  if (a > b) std::swap(a, b);
  if (b - a < RANGE_MIN_W) {                            // узкий — раздвигаем вверх, у потолка — вниз
    b = a + RANGE_MIN_W;
    if (b > NOTE_MAX) { b = NOTE_MAX; a = b - RANGE_MIN_W; }
  }
}
void copyNotes(const Thread& from, Thread& to) {        // всё, кроме курсора
  for (int i = 0; i < PEARL_MAX; i++) to.p[i] = from.p[i];
  to.len = from.len; to.dejavu = from.dejavu; to.lo = from.lo; to.hi = from.hi;
}
// слот/нить из недоверенного JSON поверх th (поля, которых нет или которые не те, остаются); курсор не трогает
void loadNotes(const sdk::Json& e, Thread& th) {
  using sdk::Json;
  if (e.type != Json::Obj) return;
  const auto num = [&](const char* key, double& out) { const Json* v = e.get(key); if (v && v->type == Json::Num && finite(v->n)) out = v->n; };
  double x = th.len; num("len", x); { const long v = std::lround(x); th.len = v < 1 ? 1 : v > PEARL_MAX ? PEARL_MAX : static_cast<int>(v); }
  x = th.dejavu; num("dejavu", x); th.dejavu = clamp01(x);
  double lo = th.lo, hi = th.hi; num("lo", lo); num("hi", hi);
  int a = static_cast<int>(std::lround(std::min(std::max(lo, double(NOTE_MIN)), double(NOTE_MAX))));
  int b = static_cast<int>(std::lround(std::min(std::max(hi, double(NOTE_MIN)), double(NOTE_MAX))));
  fixRange(a, b); th.lo = a; th.hi = b;
  if (const Json* ps = e.get("p")) if (ps->type == Json::Arr)
    for (int i = 0; i < PEARL_MAX && i < static_cast<int>(ps->a.size()); i++) {
      const Json& q = ps->a[i];
      if (q.type != Json::Arr || q.a.size() < 2) continue;
      if (q.a[0].type == Json::Num && !std::isnan(q.a[0].n)) th.p[i].pitch = clamp01(q.a[0].n);
      if (q.a[1].type == Json::Num) th.p[i].rest = q.a[1].n != 0;
    }
}
sdk::Json notesJson(const Thread& th) {
  using sdk::Json;
  Json e = Json::object();
  e.set("len", Json::num(th.len));
  e.set("dejavu", Json::num(th.dejavu));
  e.set("lo", Json::num(th.lo));
  e.set("hi", Json::num(th.hi));
  Json ps = Json::array();
  for (int i = 0; i < PEARL_MAX; i++) {
    Json q = Json::array(); q.push(Json::num(th.p[i].pitch)); q.push(Json::num(th.p[i].rest ? 1 : 0)); ps.push(q);
  }
  e.set("p", ps);
  return e;
}
}  // namespace

namespace {
// Лад как ступени: абсолютная ступень g = 7·октава + номер ступени 0…6 (0 — тоника, 2 — терция, 4 — квинта).
struct Deg {
  int root; const int* st;
  static int fdiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
  int noteOf(int g) const { const int o = fdiv(g, 7); return 33 + root + 12 * o + st[g - 7 * o]; }
  int degOf(int note) const {                           // нота вне лада — ступень не выше неё
    const int s = note - 33 - root, o = fdiv(s, 12), pc = s - 12 * o;
    int k = 0; for (int j = 0; j < 7; j++) if (st[j] <= pc) k = j;
    return 7 * o + k;
  }
};
int mod7(int g) { return ((g % 7) + 7) % 7; }
bool chordTone(int g) { const int k = mod7(g); return k == 0 || k == 2 || k == 4; }
bool consonant(int d) { const int a = std::abs(d) % 7; return a == 0 || a == 2 || a == 4; }   // терция, квинта, октава (с составными)

// Шанс паузы по месту жемчужины: сильные места (0 и середина петли, чётные) — реже, слабые (нечётные) — чаще; в среднем ~19…22%.
double restP(int i, int len) {
  const int k = i % len;
  if (k == 0) return 0.03;
  if (len > 1 && k == len / 2) return 0.06;
  return (k & 1) ? 0.32 : 0.14;
}
constexpr double LEAP_P = 0.08, CONS_P = 0.7;            // шанс скачка 3…5 ступеней; шанс аккорда с нитью 0 у нити 1
}  // namespace

// Бросок жемчужины i нити th (lead — нить 0 того же слота для нити 1, иначе nullptr). Мелодия идёт по ступеням лада хоста
// (мажор от root, если лад выключен), высота хранится как прежде 0…1 в lo…hi (нота → (нота − lo)/(hi − lo), hzOf возвращает ту же ноту).
//  - жемчужина 0 — тоника/квинта/терция (I .5, V .3, III .2) ближе всего к тонике диапазона (ступень root, ближайшая к середине lo…hi);
//  - последняя в петле (len − 1) — I (.6) или V (.4), ближайшая к предыдущей и к жемчужине 0: петля замыкается;
//  - остальные — шаг от предыдущей: ±1 (44%), ±2 (42%), повтор (6%), скачок 3…5 (8%); тянет к тонике, после скачка — назад,
//    на чётных местах 35% сдвига на ближайший звук аккорда; у краёв lo…hi — отражение;
//  - нить 1 с шансом 70% берёт терцию, квинту или октаву (±2, ±4, ±7 ступеней) от жемчужины нити 0 с тем же номером (по модулю её длины),
//    ближайшую к своей предыдущей; если у нити 0 пауза — свой ход. Паузу бросает restP; у нити 1 она реже при паузе нити 0.
void Model::toss(Thread& th, int i, const Thread* lead) {
  const Deg D{scale_.root, Scale::stepsOf(scale_.idx)};
  int gLo = D.degOf(th.lo); if (D.noteOf(gLo) < th.lo) gLo++;
  int gHi = std::max(D.degOf(th.hi), gLo);
  const auto cl = [&](int g) { return std::min(std::max(g, gLo), gHi); };
  const auto gOf = [&](const Thread& t, int k) { return D.degOf(static_cast<int>(std::lround(t.lo + t.p[k].pitch * (t.hi - t.lo)))); };
  const int gC = D.degOf((th.lo + th.hi) / 2);
  int gT = 7 * Deg::fdiv(gC + 3, 7);                    // тоника, ближайшая к середине диапазона
  while (gT < gLo) gT += 7;
  while (gT > gHi) gT -= 7;
  if (gT < gLo) gT = gC;
  const int len = th.len;
  const bool first = i == 0, last = !first && i == len - 1;
  int gl = 0; bool leadPlays = false, leadRests = false;
  if (lead) { const int q = i % lead->len; leadRests = lead->p[q].rest; leadPlays = !leadRests; if (leadPlays) gl = gOf(*lead, q); }
  const int prev = first ? gT : cl(gOf(th, i > 0 ? i - 1 : len - 1));
  int g = prev;

  if (first || last) {                                  // якорь: звук аккорда
    static const int ROLE[3] = {0, 4, 2};
    const double r = rng_.next();
    const int pick = first ? (r < 0.5 ? 0 : r < 0.8 ? 1 : 2) : (r < 0.6 ? 0 : 1);
    const int g0 = first ? gT : cl(gOf(th, 0));
    bool found = false; double best = 1e9;
    for (int a = 0; a < 3 && !found; a++) {
      const int role = ROLE[(pick + a) % 3];
      if (!first && role == 2) continue;                // замыкаем на I или V
      for (int c = gLo; c <= gHi; c++) {
        if (mod7(c) != role) continue;
        double sc = first ? std::abs(c - gT) : 2.0 * std::abs(c - prev) + 0.5 * std::abs(c - g0);
        if (leadPlays && !consonant(c - gl)) sc += 50;  // нить 1: звук, созвучный нити 0, если есть
        if (sc < best) { best = sc; g = c; found = true; }
      }
    }
    if (!found) g = gT;
  } else if (leadPlays && rng_.next() < CONS_P) {       // нить 1: терция, квинта или октава от нити 0
    static const int OFF[6] = {2, -2, 4, -4, 7, -7};
    int cand[6], n = 0;
    for (int o : OFF) { const int c = gl + o; if (c >= gLo && c <= gHi) cand[n++] = c; }
    if (n == 0) g = prev;
    else {
      for (int a = 1; a < n; a++) {                     // вставками по близости к prev (устойчиво, без аллокаций)
        const int v = cand[a]; int b = a - 1;
        while (b >= 0 && std::abs(cand[b] - prev) > std::abs(v - prev)) { cand[b + 1] = cand[b]; b--; }
        cand[b + 1] = v;
      }
      const double r = rng_.next();
      g = cand[(r < 0.85 || n == 1) ? 0 : 1];
    }
  } else {                                              // шаг по ступеням
    double up = std::min(std::max(0.5 + 0.07 * (gT - prev), 0.2), 0.8);
    if (i >= 2) { const int back = prev - cl(gOf(th, i - 2)); if (std::abs(back) >= 3) up = back > 0 ? 0.15 : 0.85; }
    const int dir = rng_.next() < up ? 1 : -1;
    const double r = rng_.next();
    const int mag = r < 0.06 ? 0 : r < 0.50 ? 1 : r < 1 - LEAP_P ? 2 : (r < 0.96 ? 3 : r < 0.985 ? 4 : 5);
    g = prev + dir * mag;
    if (g > gHi) g = 2 * gHi - g;
    if (g < gLo) g = 2 * gLo - g;
    g = cl(g);
    if ((i & 1) == 0 && !chordTone(g) && rng_.next() < 0.35) {
      const int a = g - 1, b = g + 1;
      const bool oa = a >= gLo && chordTone(a) && std::abs(a - prev) <= 5, ob = b <= gHi && chordTone(b) && std::abs(b - prev) <= 5;
      if (oa && ob) g = std::abs(a - prev) <= std::abs(b - prev) ? a : b;
      else if (oa) g = a;
      else if (ob) g = b;
    }
  }

  th.p[i].pitch = clamp01(static_cast<double>(D.noteOf(g) - th.lo) / (th.hi - th.lo));
  double rp = restP(i, len);
  if (lead) rp *= leadRests ? 0.3 : 1.1;
  th.p[i].rest = rng_.next() < rp;
}

void Model::reset(uint32_t seed) {
  rng_ = sdk::Rng(seed);
  trng_ = sdk::Rng(seed ^ 0x9E3779B9u);
  for (int t = 0; t < NR; t++) {
    Thread& th = thr_[t];
    th.len = PEARL_DEF; th.cur = 0; th.dejavu = 0.7; th.lo = LO_DEF[t]; th.hi = HI_DEF[t];
  }
  for (int r = 0; r < NR; r++) for (int k = 0; k < NRK; k++) ring_[r][k] = RING_DEF[k];
  for (int k = 0; k < NCK; k++) cloud_[k] = CLOUD_DEF[k];
  for (int t = 0; t < NR; t++) for (int i = 0; i < PEARL_MAX; i++) toss(thr_[t], i, t == 1 ? &thr_[0] : nullptr);
  curLoc_ = 0; mix_ = 0.5;
  for (int l = 0; l < NLOC; l++) for (int t = 0; t < NR; t++) {   // слот 0 — копия нитей, 1–3 — другие мелодии из того же rng
    Thread& s = loc_[l][t];
    if (l == 0) { copyNotes(thr_[t], s); continue; }
    s = Thread(); s.lo = LO_DEF[t]; s.hi = HI_DEF[t];
    for (int i = 0; i < PEARL_MAX; i++) toss(s, i, t == 1 ? &loc_[l][0] : nullptr);
  }
}

void Model::setLoc(int slot) {
  if (slot < 0 || slot >= NLOC || slot == curLoc_) return;
  for (int t = 0; t < NR; t++) {
    copyNotes(thr_[t], loc_[curLoc_][t]);
    copyNotes(loc_[slot][t], thr_[t]);
    thr_[t].cur %= thr_[t].len;                         // курсор не сбрасывается, только вписывается в новую длину
  }
  curLoc_ = slot;
}

void Model::setMix(double v) { if (!std::isnan(v)) mix_ = clamp01(v); }

void Model::rollTimbre() {
  const auto draw = [&]() { return 0.1 + 0.8 * trng_.next(); };   // подальше от краёв ручек
  for (int r = 0; r < NR; r++) for (int k : {int(STRUCT), int(BRIGHT), int(DAMP)}) ring_[r][k] = draw();
  for (int k : {int(POS), int(SIZE), int(DENS), int(PITCH)}) cloud_[k] = draw();
}

void Model::setDejaVu(int t, double v) { if (t >= 0 && t < NR && !std::isnan(v)) thr_[t].dejavu = clamp01(v); }

void Model::setLen(int t, double n) {
  if (t < 0 || t >= NR || !finite(n)) return;
  Thread& th = thr_[t];
  const long v = std::lround(n);
  const int len = v < 1 ? 1 : v > PEARL_MAX ? PEARL_MAX : static_cast<int>(v);
  const bool changed = len != th.len;
  th.len = len;
  th.cur %= th.len;
  if (changed) {                                        // новая длина — новые концы петли: жемчужина 0 (I/III/V) и последняя (I/V) бросаются заново, даже при dejavu 1
    const Thread* lead = t == 1 ? &thr_[0] : nullptr;
    toss(th, 0, lead);
    if (th.len > 1) toss(th, th.len - 1, lead);
  }
}

void Model::setRange(int t, double lo, double hi) {
  if (t < 0 || t >= NR || std::isnan(lo) || std::isnan(hi)) return;
  const auto note = [](double v) { return static_cast<int>(std::lround(std::min(std::max(v, double(NOTE_MIN)), double(NOTE_MAX)))); };
  int a = note(lo), b = note(hi);
  fixRange(a, b);
  thr_[t].lo = a; thr_[t].hi = b;
}

void Model::togglePearl(int t, int i) { if (t >= 0 && t < NR && i >= 0 && i < PEARL_MAX) thr_[t].p[i].rest = !thr_[t].p[i].rest; }
void Model::setRest(int t, int i, bool r) { if (t >= 0 && t < NR && i >= 0 && i < PEARL_MAX) thr_[t].p[i].rest = r; }
void Model::setPearlPitch(int t, int i, double p) { if (t >= 0 && t < NR && i >= 0 && i < PEARL_MAX && !std::isnan(p)) thr_[t].p[i].pitch = clamp01(p); }

void Model::setRing(int ring, int key, double v) {
  if (ring < 0 || ring >= NR || key < 0 || key >= NRK || std::isnan(v)) return;
  ring_[ring][key] = clamp01(v);
}

void Model::setCloud(int key, double v) {
  if (key < 0 || key >= NCK || std::isnan(v)) return;
  cloud_[key] = clamp01(v);
}

void Model::roll(int t) {
  for (int k = 0; k < NR; k++) if (t < 0 || t == k) for (int i = 0; i < PEARL_MAX; i++) toss(thr_[k], i, k == 1 ? &thr_[0] : nullptr);   // сначала нить 0, затем 1 — она ложится на новую
}

void Model::clearAll() {
  const auto blank = [](Thread& th, int t) { th = Thread(); th.dejavu = 1.0; th.lo = LO_DEF[t]; th.hi = HI_DEF[t]; for (int i = 0; i < PEARL_MAX; i++) th.p[i].rest = true; };
  for (int t = 0; t < NR; t++) { blank(thr_[t], t); for (int l = 0; l < NLOC; l++) blank(loc_[l][t], t); }
  for (int r = 0; r < NR; r++) for (int k = 0; k < NRK; k++) ring_[r][k] = RING_DEF[k];
  for (int k = 0; k < NCK; k++) cloud_[k] = CLOUD_DEF[k];
  cloud_[FREEZE] = 0;
  curLoc_ = 0; mix_ = 0.5;
}

bool Model::sounding() const {
  for (int t = 0; t < NR; t++) for (int i = 0; i < thr_[t].len; i++) if (!thr_[t].p[i].rest) return true;
  return false;
}

Step Model::next(int t) {
  if (t < 0 || t >= NR) return Step{false, 0, 0, 0, 0};
  Thread& th = thr_[t];
  th.cur %= th.len;
  const int i = th.cur;
  if (rng_.next() >= th.dejavu) toss(th, i, t == 1 ? &thr_[0] : nullptr);            // dejavu 1: never; 0: always
  th.cur = (i + 1) % th.len;
  const Pearl& p = th.p[i];
  return Step{!p.rest, t, hzOf(t, p.pitch), 0.8, i};
}

double Model::hzOf(int t, double pitch) const {
  const Thread& th = thr_[(t >= 0 && t < NR) ? t : 0];
  const double p = clamp01(std::isnan(pitch) ? 0.5 : pitch);
  double semi = std::round(th.lo + p * (th.hi - th.lo)) - 33;   // ноты Scale — от A1 (MIDI 33)
  if (scale_.on) {                                      // snapSemi не уходит ниже ноты 0: поднимаем на целые октавы (класс высоты тот же), снапим, опускаем
    const double shift = semi < 12 ? 12 * std::ceil((12 - semi) / 12) : 0;
    semi = scale_.snapSemi(semi + shift) - shift;
  }
  return 55.0 * std::pow(2.0, semi / 12);
}

std::string Model::snapshot() const {
  using sdk::Json;
  Json o = Json::object();
  o.set("rng", Json::num(rng_.state));
  o.set("trng", Json::num(trng_.state));
  o.set("curLoc", Json::num(curLoc_));
  o.set("mix", Json::num(mix_));
  Json ts = Json::array();
  for (int t = 0; t < NR; t++) {
    Json e = notesJson(thr_[t]);
    e.set("cur", Json::num(thr_[t].cur));
    ts.push(e);
  }
  o.set("thr", ts);
  Json ls = Json::array();
  for (int l = 0; l < NLOC; l++) {
    Json s = Json::array();
    for (int t = 0; t < NR; t++) s.push(notesJson(l == curLoc_ ? thr_[t] : loc_[l][t]));   // активный слот — по живым нитям
    ls.push(s);
  }
  o.set("loc", ls);
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
  const Json* ts = j.get("thr");
  if (!ts || ts->type != Json::Arr) return;             // старый одно-нитевой формат не поддерживается — дефолт
  double t = 1;
  if (const Json* v = j.get("rng")) if (v->type == Json::Num && finite(v->n)) t = v->n;
  rng_ = sdk::Rng(static_cast<uint32_t>(t < 0 ? 0 : t > 4294967295.0 ? 4294967295.0 : t));
  trng_ = sdk::Rng(static_cast<uint32_t>(t) ^ 0x9E3779B9u);
  if (const Json* v = j.get("trng")) if (v->type == Json::Num && finite(v->n)) trng_ = sdk::Rng(static_cast<uint32_t>(v->n < 0 ? 0 : v->n > 4294967295.0 ? 4294967295.0 : v->n));
  if (const Json* v = j.get("curLoc")) if (v->type == Json::Num && finite(v->n) && v->n >= 0 && v->n < NLOC) curLoc_ = static_cast<int>(v->n);
  if (const Json* v = j.get("mix")) if (v->type == Json::Num) setMix(v->n);
  if (const Json* ls = j.get("loc")) if (ls->type == Json::Arr)     // слоты; активный перезапишут живые нити ниже
    for (int l = 0; l < NLOC && l < static_cast<int>(ls->a.size()); l++) {
      const Json& s = ls->a[l];
      if (s.type != Json::Arr) continue;
      for (int k = 0; k < NR && k < static_cast<int>(s.a.size()); k++) loadNotes(s.a[k], loc_[l][k]);
    }
  for (int k = 0; k < NR; k++) copyNotes(loc_[curLoc_][k], thr_[k]);   // слот 0 после reset — прежние ноты; иначе из loc
  for (int k = 0; k < NR && k < static_cast<int>(ts->a.size()); k++) {
    const Json& e = ts->a[k];
    if (e.type != Json::Obj) continue;
    loadNotes(e, thr_[k]);
    double x = 0; if (const Json* v = e.get("cur")) if (v->type == Json::Num && finite(v->n)) x = v->n;
    thr_[k].cur = 0;
    thr_[k].cur = (x >= 0) ? static_cast<int>(std::fmod(x, thr_[k].len)) : 0;
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
