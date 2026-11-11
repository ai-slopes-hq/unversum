// Paradisus (VI) — мир SDK, только нативный: хор ангелов на девяти сферах (model.hpp), звук — sound.hpp (пилы → форманты, шиммер, зёрна),
// посыл уходит в общий собор ядра (rev() = cathedral). Слот 6.
//
// action(kind, i, a, n):
//   TOUCH   i = id касания (≥ 0), a = [k, v] — сфера 0…8 (зажимается) и гласная 0…5 (заворачивается); нет такого ангела — родился и поёт, есть — двигается
//   RELEASE i = id — палец поднят: незакреплённый уходит (звук гаснет за ~3 с), закреплённому ничего
//   PIN     i = id — двойной тап: незакреплённый закрепляется (до 4; пятый — отказ, остаётся поющим, пока держишь), закреплённый снимается и уходит.
//           ВНИМАНИЕ: закреплённый получает НОВЫЙ id (отрицательный, из зеркала [0], id в записи ангела) — палец, державший старый id, больше ни к чему не привязан
//   VOWEL   i = id, a = [v] — только гласная, сфера остаётся (протяжка вдоль)
//   ROSE    — тап по розе: россыпь 12 зёрен через 35 мс на первом поющем ангеле (никого — октава над тоникой)
//   FX      a = [FxKey, v], v 0…1 (i не используется)   FxKey: 0 shimmer(amount) 1 decay 2 tone 3 mix 4 cathedral 5 gloria 6 vibrato 7 orbits
//   ANGEL   i = id, a = [AngelKey, v] — ручки плашки ангела, v 0…1   AngelKey: 0 singers 1 detune 2 vib 3 breath 4 reg
//   DICE    — кубик: три закреплённых ангела на аккорде;  CLEAR — корзина: ангелов нет (fx остаются)
//   sdk::ACT_LFO (i == −1) a = [key, v]: key 0…7 — тот же FxKey, значение 0…1.   action(−1, 0, [dt], 1) (тик моделей) игнорируется: время идёт в process.
//   Act-номера зеркалит Swift (ParAct): порядок TOUCH … FX менять нельзя, новые — только в конец.
// Зеркало (MIRROR_N = 188 чисел), порядок:
//   [0] число ангелов n (0…16)   [1] закреплённых   [2] orbitT (оборотов орбиты; планета сферы k: frac(orbitT·(9−k) + 0.13·k))
//   [3..10] fx в порядке FxKey (shimmer decay tone mix cathedral gloria vibrato orbits)   [11] уровень сухого выхода 0…1 (солнце дышит)
//   [12 + 11·j …] ангел j < n, по порядку создания: id, k, v (как поставили), vEff (где поёт: закреплённый плывёт по гласным), pinned (0/1),
//                  lvl (0…1, плавная громкость: тянется к 1, пока голос собран), singers (1…5), detune, vib, breath (0…1), reg (0 / 1 / 2 → ×0.5 / ×1 / ×2).
//   Ангелы за n — нули. MIRROR_HEAD = 12, MIRROR_ANGEL = 11.
// heard: [k, (тип, arg) × k]; тип 0 — chime (планета над закреплённым поющим ангелом; arg = id ангела), тип 1 — зерно Gloria / россыпи / chime
//   (вспышка; arg = панорама −0.8…0.8, момент — когда зерно реально зазвучало). Не больше 32 событий между опросами, лишние теряются.
#pragma once
#include "../../sdk/world.hpp"
#include "model.hpp"
#include "sound.hpp"

namespace paradisus {

enum Act { TOUCH, RELEASE, PIN, VOWEL, ROSE, FX, ANGEL, DICE, CLEAR };
constexpr int MIRROR_HEAD = 12, MIRROR_ANGEL = 11, MIRROR_N = MIRROR_HEAD + MAX_ANGEL * MIRROR_ANGEL;

class World : public sdk::World {
 public:
  explicit World(const Scale& scale) : model_(scale) {}
  void prepare(double sr, uint32_t seed) override;
  void action(int kind, int i, const double* a, int n) override;
  void process(const sdk::Out& out, int n) override;
  sdk::Rev rev() const override { return sdk::Rev::cathedral; }
  int mirror(double* buf, int cap) const override;
  int heard(double* buf, int cap) override;
  std::string snapshot() const override;
  void restore(const std::string& js) override;
  bool sounding() const override { return model_.sounding() || sound_.sounding(); }
  const Model& model() const { return model_; }
  Sound& sound() { return sound_; }

 private:
  Model model_;
  Sound sound_;
  double sr_ = 48000;
};

}  // namespace paradisus
