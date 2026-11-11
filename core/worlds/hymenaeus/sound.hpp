// Hymenaeus — звук: три кольца (Plucker → Resonator из rings/) в сумму и облака (Cloud). Всё в блоках; мир (world.hpp) решает, когда бить.
//   кольцо i: щипок (strike) возбуждает резонатор; odd-моды — в левый канал, even — в правый; уровень кольца lvl.
//   сумма колец идёт в облака (пишется в буфер) и сухим выходом; облака — отдельной долей clevel. Выход — мягкое колено (потолок 1).
// Сумма колец ×RING_GAIN: пик резонатора при щипке vel 0.8 — 0.03…0.7 в зависимости от высоты.
#pragma once
#include <cstdint>

#include "../../sdk/knee.hpp"
#include "cloud.hpp"
#include "model.hpp"
#include "rings/plucker.hpp"
#include "rings/resonator.hpp"

namespace hymenaeus {

class Sound {
 public:
  static constexpr int BLOCK = 128;
  static constexpr double RING_GAIN = 3.0;
  void prepare(double sr, uint32_t seed);
  void setRing(int i, double structure, double bright, double damp, double lvl);   // зажимается в резонаторе; без изменений — ничего не пересчитывает
  void setCloud(const double* c);                                                  // NCK значений CloudKey
  void strike(int ring, double hz, double pos, double vel);
  // n ≤ BLOCK сэмплов: пишет L, R (сухо) и sL, sR (посыл) — присваивает, не прибавляет
  void render(float* L, float* R, float* sL, float* sR, int n);
  bool sounding() const;
  double ringEnergy(int i) const { return (i >= 0 && i < NR) ? env_[i] : 0; }
  double cloudEnergy() const { return cenv_; }

 private:
  rings::Resonator res_[NR];
  rings::Plucker pl_[NR];
  Cloud cloud_;
  double bright_[NR] = {}, lvl_[NR] = {}, send_ = 0.35, clevel_ = 0.6, env_[NR] = {}, cenv_ = 0;
  int pluckLeft_[NR] = {};                               // сколько сэмплов ещё гонять щипок (хвост гребёнки)
  double sr_ = 48000;
  sdk::Knee knee_{1.0, 0.7, 0.3};
};

}  // namespace hymenaeus
