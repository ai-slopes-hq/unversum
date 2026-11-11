// Генеративный узор «кубик»: части, общие для Cymatica (dice) и Hymenaeus (roll) — число ударов по Евклиду, длина мелодии,
// не делящая число ударов, блуждание по списку нот лада. Порядок вызовов Rng — как был в Cymatica::dice (паритет с прежним JS).
//
// ЛОВУШКИ:
//   - каждая функция берёт Rng по ссылке и тратит строго столько значений, сколько сказано; менять порядок вызовов нельзя —
//     у Cymatica сид кубика даёт один и тот же узор.
//   - span — ноты по возрастанию (MIDI или полутоны), sn ≥ 1; выход walkSpan — значения из span.
#pragma once
#include <algorithm>
#include <cmath>

#include "rng.hpp"

namespace sdk {

// Число ударов k на n шагов: 3…n−2 (при n < 5 зажато 1…n−1 и выше нуля). Тратит одно значение Rng.
inline int diceHits(Rng& R, int n) {
  const int lo = std::min(3, std::max(1, n - 1)), cnt = std::max(1, n - 4);
  return lo + static_cast<int>(std::floor(R.next() * cnt));
}

// Длина мелодии 3…8, не делящая n: ноты ложатся на удары по-новому каждый круг. Тратит одно значение Rng.
inline int coprimeLen(Rng& R, int n) {
  int m = 3 + static_cast<int>(std::floor(R.next() * 6));
  while (n % m == 0) m++;
  return m;
}

// Блуждание по span на m нот: начало — случайная нота span, шаг ±1 или ±2 индекса, у краёв прижимается. Тратит 1 + m значений Rng.
inline void walkSpan(const int* span, int sn, int m, Rng& R, int* out) {
  const double start = span[static_cast<int>(std::floor(R.next() * sn))];
  int i = 0;
  for (int q = 1; q < sn; q++) if (std::fabs(span[q] - start) < std::fabs(span[i] - start)) i = q;
  static const int D[4] = {-2, -1, 1, 2};
  for (int q = 0; q < m; q++) {
    out[q] = span[i];
    const int d = D[static_cast<int>(std::floor(R.next() * 4))];
    i = std::min(sn - 1, std::max(0, i + d));
  }
}

}  // namespace sdk
