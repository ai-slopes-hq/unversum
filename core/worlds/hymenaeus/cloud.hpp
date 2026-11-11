// Облака Hymenaeus — маленький гранулятор «по мотивам» Mutable Instruments Clouds (код Clouds не используется): буфер на 3 секунды
// стерео пишет вход, до MAX_GRAINS зёрен читают его с окном Ханна. Ручки (set, всё 0…1, NaN игнорируется, ±inf зажимается):
//   pos — откуда читать (0 — свежее, 1 — самое старое), size — длина зерна 30…560 мс, dens — зёрен в секунду 1…41,
//   pitch — высота зёрен ±12 пт (0.5 — без сдвига), freeze — запись стоит, читается замороженное, echo — обратная связь выхода в буфер.
// Порядок на каждый сэмпл: write(вход) → process(выход). Спит (sleeping), пока в буфере тишина и нет зёрен: тогда write и process
// почти ничего не стоят. Выход мягко ограничен (x / (1 + |x|/4) — меньше 4 по модулю), обратная связь — тоже, поэтому эхо не разгоняется.
// ЛОВУШКИ: объект ~1.2 МБ — держать статически или в куче, не на стеке (у wasm стек 64 КБ); sleeping() становится true не сразу —
// через длину буфера (3 с) тишины на входе; при echo > 0 хвост эха держит облако дольше.
#pragma once
#include <cstdint>

#include "../../sdk/rng.hpp"

namespace hymenaeus {

class Cloud {
 public:
  static constexpr int MAX_GRAINS = 24;
  static constexpr int MAX_FRAMES = 3 * 48000;           // 3 с при 48 кГц; на более высокой частоте буфер короче по времени
  void prepare(double sr, uint32_t seed);
  void set(double pos, double size, double dens, double pitch, bool freeze, double echo);
  void write(float l, float r);
  void process(float& l, float& r);
  bool sleeping() const { return !hasSound_ && ng_ == 0; }
  int grains() const { return ng_; }
  void silence() { ng_ = 0; written_ = 0; silentRun_ = 0; hasSound_ = false; fbL_ = fbR_ = 0; }   // зёрна и эхо забыты; буфер не чистим: чтение ограничено written_

 private:
  struct Grain { float dist, rate, age, inv, gl, gr; };   // dist — кадров позади головы записи
  void spawn();
  float tap(const float* b, float idx) const;

  float buf_[2][MAX_FRAMES] = {};
  float win_[513] = {};
  Grain g_[MAX_GRAINS] = {};
  int ng_ = 0, len_ = MAX_FRAMES, wp_ = 0, written_ = 0, silentRun_ = 0;
  bool hasSound_ = false, frozen_ = false;
  float sr_ = 48000, pos_ = 0.5f, size_ = 0.4f, dens_ = 0.4f, pitch_ = 0.5f, echo_ = 0.3f;
  float phase_ = 0, gain_ = 1, fbL_ = 0, fbR_ = 0;
  sdk::Rng rng_{1};
};

}  // namespace hymenaeus
