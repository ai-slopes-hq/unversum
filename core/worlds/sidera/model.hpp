// Sidera (XIV) — модель: три голоса GENDYN (динамический стохастический синтез Ксенакиса). Голос — N точек излома формы волны (звёзды),
// у каждой (d, a) — длительность отрезка и амплитуда, и центр рамки (hd, ha). Каждый период точки делают шаг по распределению Коши,
// логистическому или равномерному и отражаются зеркальными границами рамки. Без звука и без времени: период ведёт Voice (voice.hpp).
// Случайность — только sdk::Rng от зерна. Параметры меняются только методами модели.
//
// Высота голоса — нота лада хоста (NOTE, полутоны от A1 55 Гц, 0…60): период нормируется под sr / f0, блуждает только тембр.
#pragma once
#include <string>

#include "../../host/scale.hpp"
#include "../../sdk/rng.hpp"

namespace sidera {

constexpr int NV = 3, NPT_MIN = 3, NPT_MAX = 16;
constexpr double D_MIN = 0.02;                          // меньшая длительность отрезка (до нормировки)
enum Dist { CAUCHY, LOGISTIC, UNIFORM, NDIST };
enum VKey { NOTE, N, WD, WA, LVL, PAN, DIST, VSTEP, OCT, NVK };   // NOTE 0…60 (целые), N 3…16, WD WA LVL 0…1, PAN −1…1, DIST 0…2, VSTEP 0…2, OCT −2…2 (целые, октавы поверх лада)

enum FxKey { DRIFT, SPACE, TONE, CUTOFF, RESO, NFX };  // общие FX мира 0…1: DRIFT — вторичное блуждание рамок, SPACE — посыл в ревербер, TONE — яркость (фильтр голоса),
                                                        // CUTOFF (1 — открыт) и RESO — мастер-фильтр lowpass с резонансом, без огибающей

struct Point { double d = 0.5, a = 0, hd = 0.5, ha = 0; };   // d — 0.02…1, a — −1…1, hd/ha — центр рамки

class Model {
 public:
  explicit Model(const Scale& scale) : scale_(scale) { reset(); }
  void reset(uint32_t seed = 1);                        // всё по умолчанию; семена голосов seed + 1000·v

  // v — голос 0…NV−1; вне диапазона и NaN — ничего; ±inf зажимается; N, NOTE, DIST округляются
  void setVoice(int v, int key, double val);
  void moveHome(int v, int pt, double hd, double ha);   // центр рамки точки: hd 0.02…1, ha −1…1; точка (d, a) подтягивается внутрь рамки
  void setStep(double s);                               // общий масштаб шага (луна) 0…1
  void roll(int v = -1);                                // кубик: голос v (−1 — все): новые нота, N, рамки, панорама, закон, октава и положение звёзд, блуждание сеется заново; при −1 ещё луна и FX
  void kick(int v, int pt);                             // встряска: точка pt голоса v (−1 — все) прыгает в случайное место рамки, рамка чуть сдвигается
  void setFx(int key, double val);                      // FxKey, значение 0…1; вне диапазона и NaN — ничего
  double fx(int key) const { return (key >= 0 && key < NFX) ? fx_[key] : 0; }
  void silence();                                       // корзина: LVL всех голосов = 0, точки целы
  void walk(int v);                                     // один период блуждания: все N точек голоса v делают шаг

  double hzOf(int v) const;                             // нота голоса через лад хоста → Гц
  bool sounding() const;                                // у какого-нибудь голоса LVL > 0.001

  std::string snapshot() const;
  void restore(const std::string& js);                  // мусор и выход за диапазон зажимаются, не падают

  double step() const { return step_; }
  double voice(int v, int key) const { return (v >= 0 && v < NV && key >= 0 && key < NVK) ? prm_[v][key] : 0; }
  Point pt(int v, int i) const { return (v >= 0 && v < NV && i >= 0 && i < NPT_MAX) ? pts_[v][i] : Point(); }

 private:
  void initPoint(int v, int i);                         // умолчание точки i при текущем N голоса: hd 0.5, ha 0.6·sin(2π(i+0.5)/N)
  void fit(Point& p, int v) const;                      // подтянуть (d, a) внутрь рамки точки
  const Scale& scale_;
  double prm_[NV][NVK] = {};
  Point pts_[NV][NPT_MAX];
  double step_ = 0.15;
  double fx_[NFX] = {0.25, 0.3, 0.35, 1.0, 0.1};
  sdk::Rng rng_{1};                                     // мастер: семена голосов при roll
  sdk::Rng vrng_[NV] = {sdk::Rng(1), sdk::Rng(1), sdk::Rng(1)};
};

}  // namespace sidera
