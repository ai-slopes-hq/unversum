// Hymenaeus (XIV) — модель: жемчужная нить (Marbles-lite), параметры колец и облаков. Без звука и без времени:
// ход нити — раз в шаг, шаги тикает мир (world.hpp). Случайность — только sdk::Rng от зерна.
//
// Нить — до 16 жемчужин (высота 0…1, пауза), играют первые len по кругу. Застёжка déjà vu 0…1: на каждом шаге с вероятностью
// 1 − dejavu жемчужина под курсором перебрасывается (новая высота, пауза с шансом REST_P), иначе остаётся — 1 даёт точный повтор.
// Высота → полутон (3 октавы от A2) → лад хоста (Scale) → Гц; кольцо выбирает полоса высоты: низкие — большое кольцо 0, высокие — малое 2.
#pragma once
#include <string>

#include "../../host/scale.hpp"
#include "../../sdk/rng.hpp"

namespace hymenaeus {

constexpr int NR = 3, PEARL_MAX = 16, PEARL_DEF = 11, NRK = 4;
enum RingKey { STRUCT, BRIGHT, DAMP, LVL };
enum CloudKey { POS, SIZE, DENS, PITCH, FREEZE, ECHO, CLEVEL, NCK };
constexpr double REST_P = 0.15;                         // шанс паузы у перебрасываемой жемчужины

struct Pearl { double pitch = 0.5; bool rest = false; };
struct Step { bool hit; int ring; double hz; double vel; int pearl; };

class Model {
 public:
  explicit Model(const Scale& scale) : scale_(scale) { reset(); }
  void reset(uint32_t seed = 1);                        // всё по умолчанию, нить заново бросается из зерна

  void setDejaVu(double v);
  void setLen(int n) { setLen(static_cast<double>(n)); }
  void setLen(double n);                                // округляется, 1…16; NaN и ±inf игнорируются
  void togglePearl(int i);
  void setRest(int i, bool r);
  void setPearlPitch(int i, double p);
  void setRing(int ring, int key, double v);
  void setCloud(int key, double v);
  void roll();                                          // кубик: вся нить бросается заново
  void clear();                                         // вся нить — паузы

  Step next();                                          // шаг нити: курсор двигается, жемчужина может перебросится
  int ringFor(double pitch) const;                      // полоса высоты → кольцо 0…2
  bool sounding() const;                                // есть ли среди первых len жемчужин звучащая

  std::string snapshot() const;
  void restore(const std::string& js);                  // мусор и выход за диапазон зажимаются, не падают

  double dejavu() const { return dejavu_; }
  int len() const { return len_; }
  int cursor() const { return cur_; }
  Pearl pearl(int i) const { return (i >= 0 && i < PEARL_MAX) ? p_[i] : Pearl(); }
  double ring(int r, int key) const { return (r >= 0 && r < NR && key >= 0 && key < NRK) ? ring_[r][key] : 0; }
  double cloud(int key) const { return (key >= 0 && key < NCK) ? cloud_[key] : 0; }

 private:
  void toss(int i);                                     // бросить жемчужину i
  const Scale& scale_;
  Pearl p_[PEARL_MAX];
  int len_ = PEARL_DEF, cur_ = 0;
  double dejavu_ = 0.7;
  double ring_[NR][NRK] = {};
  double cloud_[NCK] = {};
  sdk::Rng rng_{1};
};

}  // namespace hymenaeus
