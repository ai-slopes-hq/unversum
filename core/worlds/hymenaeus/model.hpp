// Hymenaeus (XIV) — модель: жемчужная нить (Marbles-lite), параметры колец и облаков. Без звука и без времени:
// ход нити — раз в шаг, шаги тикает мир (world.hpp). Случайность — только sdk::Rng от зерна.
//
// Нить — до 16 жемчужин (высота 0…1, пауза), играют первые len по кругу. Застёжка déjà vu 0…1: на каждом шаге с вероятностью
// 1 − dejavu жемчужина под курсором перебрасывается (музыкальный генератор, см. Model::toss: шаги по ступеням лада, тоника в начале,
// замыкание петли, ритм пауз по месту, нить 1 в терцию/квинту/октаву к нити 0), иначе остаётся — 1 даёт точный повтор.
// Пальцем жемчужины не правятся: меняются только кубиком (roll — нить заполняется последовательно) и застёжкой déjà vu;
// единственное исключение — смена длины петли (setLen): концы (жемчужина 0 и последняя) бросаются заново, чтобы петля замкнулась.
// Диапазон lo…hi по умолчанию ~1,7 октавы вокруг тоники нити (резонатор 47…67, струна 59…79); пальцем не правится (PEARL/RANGE-акты и
// setPearlPitch/setRange остались для тестов и снимков).
// Четыре локации (слоты): в каждой для каждой нити свои жемчужины, длина, déjà vu и диапазон; кольца, облака и mix общие и при смене
// локации не меняются. Курсоры нитей при смене не сбрасываются. Две нити, нить t бьёт кольцо t (0 — modal, 1 — strings); у каждой свои длина, курсор, déjà vu и диапазон высоты lo…hi (ноты MIDI,
// 24…96, ширина ≥ 5). Высота 0…1 → lo + p·(hi − lo) → лад хоста (Scale) → Гц (генератор кладёт ноты лада точно: p = (нота − lo)/(hi − lo)). Случайность — общая, от одного rng.
#pragma once
#include <string>

#include "../../host/scale.hpp"
#include "../../sdk/rng.hpp"

namespace hymenaeus {

constexpr int NR = 2, PEARL_MAX = 16, PEARL_DEF = 11, NRK = 5, NLOC = 4;
constexpr int NOTE_MIN = 24, NOTE_MAX = 96, RANGE_MIN_W = 5;
enum RingKey { STRUCT, BRIGHT, DAMP, LVL, POLY };       // POLY 0…1 → 1 / 2 / 4 голоса
enum CloudKey { POS, SIZE, DENS, PITCH, FREEZE, ECHO, CLEVEL, SEND, NCK };   // SEND — посыл в общий ревербер
inline int polyOf(double v) { return v < 1.0 / 3 ? 1 : v < 2.0 / 3 ? 2 : 4; }

struct Pearl { double pitch = 0.5; bool rest = false; };
struct Thread {
  Pearl p[PEARL_MAX];
  int len = PEARL_DEF, cur = 0;
  double dejavu = 0.7;
  int lo = 45, hi = 69;
};
struct Step { bool hit; int ring; double hz; double vel; int pearl; };

class Model {
 public:
  explicit Model(const Scale& scale) : scale_(scale) { reset(); }
  void reset(uint32_t seed = 1);                        // всё по умолчанию, нить заново бросается из зерна

  // t — номер нити 0…NR−1; вне диапазона — ничего не делает
  void setDejaVu(int t, double v);
  void setLen(int t, int n) { setLen(t, static_cast<double>(n)); }
  void setLen(int t, double n);                         // округляется, 1…16; NaN и ±inf игнорируются; при смене длины концы петли (жемчужина 0 и последняя) бросаются заново, даже при dejavu 1
  void setRange(int t, double lo, double hi);           // ноты MIDI: округление, зажим 24…96, lo > hi — меняются, ширина ≥ 5; NaN — игнор
  void togglePearl(int t, int i);
  void setRest(int t, int i, bool r);
  void setPearlPitch(int t, int i, double p);
  void setRing(int ring, int key, double v);
  void setCloud(int key, double v);
  void roll(int t = -1);                                // кубик: нить t (−1 — обе) бросается заново
  void setLoc(int slot);                                // локация 0…NLOC−1: активные нити уходят в старый слот, из нового приходят другие; вне диапазона — ничего
  void setMix(double v);                                // 0…1 (0.5 — как раньше): сухое ↔ облака; NaN игнорируется
  void rollTimbre();                                    // STRUCT, BRIGHT, DAMP колец и POS, SIZE, DENS, PITCH облаков — случайно; ноты и уровни не трогает
  void clearAll();                                      // корзина: все ноты всех слотов — паузы, нити/кольца/облака (кроме FREEZE → 0)/mix — по умолчанию, cur и curLoc 0; rng не трогается

  Step next(int t);                                     // шаг нити t: курсор двигается, жемчужина может перебросится; ring = t
  double hzOf(int t, double pitch) const;               // высота 0…1 → Гц: lo…hi нити t, лад хоста
  bool sounding() const;                                // есть ли среди первых len жемчужин какой-нибудь нити звучащая

  std::string snapshot() const;
  void restore(const std::string& js);                  // мусор и выход за диапазон зажимаются, не падают

  int loc() const { return curLoc_; }
  double mix() const { return mix_; }
  const Thread& thread(int t) const { return thr_[(t >= 0 && t < NR) ? t : 0]; }
  double ring(int r, int key) const { return (r >= 0 && r < NR && key >= 0 && key < NRK) ? ring_[r][key] : 0; }
  double cloud(int key) const { return (key >= 0 && key < NCK) ? cloud_[key] : 0; }

 private:
  void toss(Thread& th, int i, const Thread* lead);     // бросить жемчужину i нити th музыкальным генератором (см. model.cpp); lead — нить 0 для нити 1
  const Scale& scale_;
  Thread thr_[NR];
  double ring_[NR][NRK] = {};
  double cloud_[NCK] = {};
  Thread loc_[NLOC][NR];                                // слоты; слот curLoc_ устарел — правда в thr_ (ноты, len, dejavu, lo, hi; cur не хранится)
  int curLoc_ = 0;
  double mix_ = 0.5;
  sdk::Rng rng_{1};
  sdk::Rng trng_{1};                                    // отдельный поток для кости timbre: мелодии déjà vu не зависят от бросков
};

}  // namespace hymenaeus
