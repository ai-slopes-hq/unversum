// Hymenaeus (XIV) — мир SDK: нить жемчужин (model.hpp) бьёт по трём кольцам (rings/), звон уходит в облака (cloud.hpp) и в общий ревербер.
// Шаги нити — восьмые от своего темпа (bpm 20…300, по умолчанию 90) или по сетке MIDI-часов хоста; не больше MAX_EV шагов за блок.
//
// action(kind, i, a, n):
//   RING i = кольцо, a = [RingKey (STRUCT BRIGHT DAMP LVL MODE POLY), v];  CLOUD a = [CloudKey, v];  PEARL i = жемчужина, a = [0 высота | 1 пауза, v];
//   STRIKE i = кольцо, a = [место удара 0…1, сила 0…1] (по умолчанию 1) — ручной удар;  THREAD a = [0 déjà vu | 1 длина, v];
//   DICE, CLEAR;  RUN a = [on];  BPM a = [bpm];
//   ACT_LFO (i == -1) a = [key, v]: key 0…NR·NRK−1 — кольцо key / NRK, ключ key % NRK; затем CloudKey (key − NR·NRK)
// Зеркало (MIRROR_N): run, bpm, dejavu, len, cursor; PEARL_MAX × (высота, пауза); NR × (struct, bright, damp, lvl, mode, poly, энергия); NCK облаков; энергия облаков.
// heard: [k, (кольцо, жемчужина) × k]; жемчужина −1 — ручной удар.
#pragma once
#include "../../sdk/transport.hpp"
#include "../../sdk/world.hpp"
#include "model.hpp"
#include "sound.hpp"

namespace hymenaeus {

enum Act { RING, CLOUD, PEARL, STRIKE, THREAD, DICE, CLEAR, RUN, BPM };
constexpr int MIRROR_HEAD = 5, MIRROR_RING = 7, MIRROR_N = MIRROR_HEAD + 2 * PEARL_MAX + NR * MIRROR_RING + NCK + 1;
constexpr int MAX_EV = 4, HEARD_MAX = 32;

class World : public sdk::World {
 public:
  explicit World(const Scale& scale) : model_(scale) {}
  void prepare(double sr, uint32_t seed) override;
  void action(int kind, int i, const double* a, int n) override;
  void process(const sdk::Out& out, int n) override;
  sdk::Rev rev() const override { return sdk::Rev::hall; }
  int mirror(double* buf, int cap) const override;
  int heard(double* buf, int cap) override;
  std::string snapshot() const override;
  void restore(const std::string& js) override;
  bool sounding() const override;
  void attach(const sdk::Clock* c) override { clk_ = c; }
  const Model& model() const { return model_; }

 private:
  struct Ev { int off, ring, pearl; double hz, pos, vel; };
  void queue(Ev* q, int& nq, int cap, const Ev& e) { if (nq < cap) q[nq++] = e; }
  void pushHeard(int ring, int pearl) { if (nh_ < HEARD_MAX) { hr_[nh_] = ring; hp_[nh_] = pearl; nh_++; } }
  Model model_;
  Sound sound_;
  sdk::Steps steps_;
  const sdk::Clock* clk_ = nullptr;
  Ev man_[8]; int nman_ = 0;                             // ручные удары, ждут следующего блока
  int hr_[HEARD_MAX] = {}, hp_[HEARD_MAX] = {}, nh_ = 0;
  bool run_ = false;
  double bpm_ = 90, sr_ = 48000, now_ = 0;
};

}  // namespace hymenaeus
