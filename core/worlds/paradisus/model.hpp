// Paradisus (VI) — модель: ангелы на девяти сферах, общие fx, планеты на сферах (полиритм). Порт worlds/paradisus/model.js.
// Без звука, DOM и системного времени: время — tick(dt) в секундах, случайность — только sdk::Rng от зерна.
//
// Ангел — голос хора на сфере k (0 — Primum Mobile … 8 — Luna; гармоника harm(k) = 9 − k) и гласной v (0…5 по кругу a e i o u).
// Тронул сферу — поёт, пока держишь; закреплённый (togglePin) остаётся; закреплённых не больше MAX_PIN.
// Идентификаторы: id ≥ 0 — номер касания от хоста (touch создаёт или двигает), id < 0 — выдаёт модель (кубик, restore, закрепление:
// при закреплении ангел получает новый отрицательный id, его возвращает pinId()). Вид и плашки ангела обращаются по id.
// Высота: тоника A2 (110 Гц; лад включён — от его тоники root) × harm(k) × регистр (0.5 / 1 / 2). В JS высота не снапится в лад — так же здесь.
//
// Действия мира (Task 3 кладёт их в Act): TOUCH(id,k,v), RELEASE(id), TOGGLE_PIN(id), SET_ANGEL(id,key,val), SET_FX(key,val),
// ROSE, DICE, CLEAR — один в один с методами ниже.
#pragma once
#include <cstdint>
#include <string>

#include "../../host/scale.hpp"
#include "../../sdk/rng.hpp"

namespace paradisus {

constexpr int NSPH = 9, MAX_PIN = 4, NVOW = 5, MAX_ANGEL = 16, MAX_CHIME = 16;
enum FxKey { SHIMMER, DECAY, TONE, MIX, CATHEDRAL, GLORIA, VIBRATO, ORBITS, NFX };   // SHIMMER — amount шиммера; CATHEDRAL — посыл в собор ядра
enum AngelKey { SINGERS, DETUNE, VIB, BREATH, REG, NAKEY };   // ручки плашки ангела (0…1; SINGERS → 1…5 певчих, REG → 0/1/2)

struct Angel {
  int k = 0; double v = 0; bool pinned = false; int singers = 4;
  double detune = 0.5, vib = 0.5, breath = 0.25;
  int reg = 1;                                          // 0 / 1 / 2 → ×0.5 / ×1 / ×2
  double lvl = 0;                                       // плавная громкость 0…1 (для вида): тянется к 1, пока голос собран (sung)
  uint32_t seed = 0;                                    // фаза дрейфа гласной: seed / 2^32 · 2π
  double dr = 1;                                        // скорость дрейфа 0.7…1.3
  bool sung = false;                                    // голос собран и звучит (ставит мир: setSung)
  int id = 0;
};
struct Fx { double shimmer = 0.35, decay = 0.5, tone = 0.5, mix = 0.5, cathedral = 0.5, gloria = 0.2, vibrato = 0.4, orbits = 0.35; };
struct Chime { int id; double hz; };

class Model {
 public:
  explicit Model(const Scale& scale) : scale_(scale) { reset(); }
  void reset(uint32_t seed = 1);                        // всё по умолчанию, ангелов нет

  int harm(int k) const { return 9 - k; }
  double tonic() const;                                 // 110 Гц; лад включён — × 2^(root/12)
  double hzOf(const Angel& a) const;
  double vEff(const Angel& a) const;                    // закреплённый плывёт по гласным на ±0.45·vibrato, остальные — v
  double planetAt(int k) const;                         // 0…1 по кругу: harm(k) оборотов за круг орбиты
  // гласная v (0…5, по кругу; вне — заворачивается) → 4 форманта: частота Гц (по логарифму между гласными), ширина Гц, громкость в дБ
  void formants(double v, double f[4], double bw[4], double g[4]) const;

  // Ангелы
  void touch(int id, int k, double v);                  // id ≥ 0: нет такого — новый поющий ангел; есть — сфера k (поперёк) и гласная v (вдоль). k зажимается 0…8, v заворачивается в 0…5
  void release(int id);                                 // палец поднят: незакреплённый исчезает, закреплённому — ничего
  bool togglePin(int id);                               // незакреплённый → закреплён (true, новый id в pinId()); пятый — отказ (false); закреплённый → снят и исчез (false)
  int pinId() const { return pinId_; }                  // id последнего закреплённого ангела
  void setAngel(int id, int key, double val);           // AngelKey; val 0…1 зажимается, NaN игнорируется
  void setSung(int id, bool on);                        // склейка с миром: голос ангела собран / снят
  void setFx(int key, double val);                      // FxKey; 0…1, NaN игнорируется
  void dice();                                          // кубик: все ангелы заменяются тремя закреплёнными на аккорде 4·5·6 (или 3·4·5, 4·6·8, 5·6·8, 2·3·5)
  void clear();                                         // корзина: ангелов нет (fx остаются)
  double roseHz() const;                                // тап по розе: первый поющий ангел, иначе октава над тоникой (мир разбрасывает 12 зёрен через 35 мс)

  // Время: lvl ангелов, орбиты и chime. Планета сферы прошла над закреплённым поющим ангелом — chime (забрать takeChimes)
  void tick(double dt);
  int takeChimes(Chime* out, int cap);
  double orbitT() const { return orbitT_; }
  void setOrbit(double x) { orbitT_ = x; }

  int count() const { return n_; }
  const Angel& angel(int i) const { return a_[(i >= 0 && i < n_) ? i : 0]; }   // порядок — как создавались; count() == 0 — не вызывать
  const Angel* find(int id) const;
  int pinnedCount() const;
  const Fx& fx() const { return fx_; }
  bool sounding() const { return n_ > 0; }

  std::string snapshot() const;                         // {"rng":…,"angels":[закреплённые: k v singers detune vib breath reg],"fx":{…}}
  void restore(const std::string& js);                  // мусор и выход за диапазон зажимаются, не падают; пустая/битая строка — дефолт

 private:
  Angel* findMut(int id);
  Angel* add(int id, int k, double v, bool pin);
  void removeAt(int i);
  const Scale& scale_;
  Angel a_[MAX_ANGEL];
  int n_ = 0, nextId_ = -1, pinId_ = 0;
  Fx fx_;
  double orbitT_ = 0, time_ = 0;
  Chime chimes_[MAX_CHIME];
  int nChimes_ = 0;
  sdk::Rng rng_{1};
};

}  // namespace paradisus
