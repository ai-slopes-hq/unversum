// Hymenaeus — звук: три кольца (Plucker → Resonator из rings/) в сумму и облака (Cloud). Всё в блоках; мир (world.hpp) решает, когда бить.
//   кольцо i: щипок (strike) возбуждает резонатор; odd-моды — в левый канал, even — в правый; уровень кольца lvl.
//   сумма колец идёт в облака (пишется в буфер) и сухим выходом; облака — отдельной долей clevel. Выход — мягкое колено (потолок 1).
// Режим кольца (MODE): modal — Resonator (щипок возбуждает банк мод); strings — sympathetic strings Rings: до 8 струн на кольцо,
//   главная струна (0) на ноте удара, остальные звенят от неё сочувственно, их строй ведёт structure (развёртка = глайд).
// Полифония кольца (POLY): 1 / 2 / 4 голоса по кругу, каждый следующий удар забирает самый старый; в strings голос = 8/poly струн.
// Смена режима и ручек на звенящем кольце ничего не обрывает: голос дозвенивает в том режиме, в котором его ударили.
// Сумма колец ×RING_GAIN: пик резонатора при щипке vel 0.8 — 0.03…0.7 в зависимости от высоты.
#pragma once
#include <cstdint>

#include "../../sdk/knee.hpp"
#include "cloud.hpp"
#include "model.hpp"
#include "rings/plucker.hpp"
#include "rings/resonator.hpp"
#include "rings/string.hpp"

namespace hymenaeus {

class Sound {
 public:
  static constexpr int BLOCK = 128;
  static constexpr double RING_GAIN = 3.0;
  void prepare(double sr, uint32_t seed);
  // зажимается; mode — доля 0…1 (modeOf), poly — доля 0…1 (polyOf); без изменений — ничего не пересчитывает
  void setRing(int i, double structure, double bright, double damp, double lvl, double mode = 0, double poly = 0);
  void setCloud(const double* c);                                                  // NCK значений CloudKey
  void strike(int ring, double hz, double pos, double vel);
  // n ≤ BLOCK сэмплов: пишет L, R (сухо) и sL, sR (посыл) — присваивает, не прибавляет
  void render(float* L, float* R, float* sL, float* sR, int n);
  void silence();                                        // оборвать всё звучащее (кольца, струны, облака) без выделений; параметры остаются
  bool sounding() const;
  double ringEnergy(int i) const { return (i >= 0 && i < NR) ? env_[i] : 0; }
  double cloudEnergy() const { return cenv_; }

 private:
  static constexpr int VOICES = 4, STRINGS = 8;
  void renderRing(int r, float* o, float* e, int n);     // суммирует голоса кольца r (modal и strings) в o / e
  void renderStrings(int r, const float exc[VOICES][BLOCK], float* o, float* e, int n);
  rings::Resonator res_[NR][VOICES];
  rings::Plucker pl_[NR][VOICES];
  rings::String str_[NR][STRINGS];                       // голос v, струна s — str_[r][v + s·polyS_[r]]
  Cloud cloud_;
  double structure_[NR] = {}, bright_[NR] = {}, damp_[NR] = {}, lvl_[NR] = {}, pos_[NR] = {}, send_ = 0.35, clevel_ = 0.6, env_[NR] = {}, cenv_ = 0;
  double note_[NR][VOICES] = {}, lfo_[NR][STRINGS] = {};   // нота голоса (полутона MIDI); фаза LFO положения сочувственных струн
  float dcx_[NR][VOICES] = {}, dcy_[NR][VOICES] = {};      // DC-блокер входа струн
  int pluckLeft_[NR][VOICES] = {};                       // сколько сэмплов ещё гонять щипок (хвост гребёнки)
  int vmode_[NR][VOICES] = {};                           // каким режимом голос ударили: 0 modal, 1 strings
  int mode_[NR] = {}, poly_[NR] = {}, polyS_[NR] = {}, next_[NR] = {};   // режим, полифония (1/2/4), полифония раскладки струн, следующий голос
  bool strActive_[NR] = {};
  double sr_ = 48000;
  sdk::Knee knee_{1.0, 0.7, 0.3};
};

}  // namespace hymenaeus
