// Sidera (XIV) — мир SDK: три голоса GENDYN (model.hpp, voice.hpp) → Knee → сухой выход и посыл в общий ревербер (hall).
// Мир только нативный (iOS), слот 14. Шаг блужданий — «луна»; корзина гасит голоса.
//
// action(kind, i, a, n):
//   VOICE i = голос, a = [VKey, v];  STAR i = голос, a = [точка, hd, ha] (центр рамки: hd 0.02…1, ha −1…1);
//   DICE a = [голос] (пусто или −1 — все): точки в центры рамок, блуждание сеется заново (вне диапазона — ничего);
//   CLEAR — LVL всех голосов в 0 (точки целы);  MOON a = [step 0…1];  RUN a = [on] — пока не включено, мир молчит (как Hymenaeus: звук начинается со входа в мир);
//   KICK i = голос, a = [точка] (−1 — все): встряска звезды;  FX a = [FxKey, v 0…1] (DRIFT, SPACE, TONE).
//   ACT_LFO (i == −1) a = [key, v], v 0…1: key 0 — шаг луны; 1 + 4·голос + {0 LVL, 1 PAN (0…1 → −1…1), 2 WD, 3 WA}; 13 + {0 DRIFT, 1 SPACE, 2 TONE}.
//   Act-номера зеркалит Swift (SidAct): порядок enum Act менять нельзя.
// Зеркало (MIRROR_N = 222 числа), порядок:
//   [0] энергия выхода 0…1  [1] step  [2] send (доля посыла в ревербер = SPACE)
//   по голосу v с 3 + 72·v: [NOTE, N, WD, WA, LVL, PAN, DIST, энергия голоса] (8), затем NPT_MAX × (d, a, hd, ha) (64)
//   в конце с 219: [DRIFT, SPACE, TONE]
// heard: ничего (вспышек нет).
#pragma once
#include "../../sdk/world.hpp"
#include "model.hpp"
#include "voice.hpp"

namespace sidera {

enum Act { VOICE, STAR, DICE, CLEAR, MOON, RUN, KICK, FX };
constexpr int MIRROR_HEAD = 3, MIRROR_VOICE = 8 + NPT_MAX * 4, MIRROR_FX = MIRROR_HEAD + NV * MIRROR_VOICE, MIRROR_N = MIRROR_FX + NFX;   // 3 + 3·72 + 3 = 222

class World : public sdk::World {
 public:
  explicit World(const Scale& scale) : model_(scale) {}
  void prepare(double sr, uint32_t seed) override;
  void action(int kind, int i, const double* a, int n) override;
  void process(const sdk::Out& out, int n) override;
  sdk::Rev rev() const override { return sdk::Rev::hall; }
  int mirror(double* buf, int cap) const override;
  int heard(double*, int) override { return 0; }
  std::string snapshot() const override;
  void restore(const std::string& js) override;
  bool sounding() const override { return run_ && model_.sounding(); }

 private:
  Model model_;
  Voice voice_[NV];
  double sr_ = 48000, env_ = 0;
  bool run_ = false;                                    // мир молчит, пока его не включили (RUN); в снимке
};

}  // namespace sidera
