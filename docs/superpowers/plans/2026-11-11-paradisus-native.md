# Paradisus native Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Вернуть мир VI · Paradisus в нативное iOS-приложение (слот 6) с новым звуком: хор гласных + шиммер.

**Architecture:** Новый блок `sdk::Shimmer` (core/sdk). Мир `paradisus::World` в `core/worlds/paradisus/` по образцу Hymenaeus (модель → звук → мир → зеркало). Вид на Swift в `ios/app/T2D/Paradisus/` (зеркало, геометрия, рисунок, жесты, плашки). Веб остаётся в архиве.

**Tech Stack:** C++ ядро (`core/`, wasm + нативные тесты `core/build.sh`), Swift/CoreGraphics (xcodegen + xcodebuild), JS-оригинал `worlds/paradisus/` как референс.

**Spec:** `docs/superpowers/specs/2026-11-11-paradisus-native-design.md`

## Global Constraints

- Слот 6; в `core/bridge/api.cpp` — `static paradisus::World par(host.scale());` и `{6, &par}` в `kSlots` (рядом со стр. 39 и 71).
- Случайность только через `sdk::Rng`; модель без DOM, времени, `Math.random`; свой `hall()` у мира нет — только посыл в общий ревербер (`out.sL/sR`, `rev()`).
- Без аллокаций в `process`; без `std::vector` push в аудиопотоке.
- Каждый коммит с `core/*.cpp|hpp` включает пересобранный `t2d.wasm` (`core/build.sh`; guard-хук). Новые `.cpp` — во все четыре места `core/build.sh`: sdk-test, hymenaeus-test (по аналогии paradisus-test), mix-cli, t2d.wasm. `ios/app/project.yml`: `T2DTests.sources` += `T2D/Paradisus`.
- Порядок `enum Act` менять нельзя после фиксации; Swift `ActNumbers` его зеркалит.
- Swift: холст не плотнее 2×; без `shadowBlur`; градиенты через `glowAt`, не каждый кадр; без полноэкранных проходов; подписи только во время жеста; шрифты IM Fell English / EB Garamond; светлая тема `INK` 94,69,32 на `#f5efe1`, золото `GOLD`.
- Коммиты на английском, тело списком, хвост `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`. Чужие `notebooks/` не коммитить.
- Перед выкладкой веба (пуш): `sw.js` `CACHE` = `?v=` в `index.html`, число больше, чем в `origin/native` (сейчас 231).
- Шиммер: потолок обратной связи ≤ 0.55 жёстко, независимо от ручек.

## Review Focus

1. Перегруз: все ручки шиммера на максимуме + все 4 закреплённых ангела + Gloria + россыпь + собор — пик выхода мира < 1.0, нет NaN/Inf (Task 1, Task 3).
2. Нулевые/крайние входы шиммера: тишина, DC-смещение, impulse, `sr` 44100/48000, размер блока 1 и 512 (Task 1).
3. Лад выключен / экзотический (`scale_` off, мало ступеней): `hzOf` не падает, высота в слышимом диапазоне (Task 2).
4. Снимок: `restore` пустой/битой строки, старый формат — мир остаётся рабочим (Task 3).
5. Жесты: двойной тап по пятому ангелу при 4 закреплённых; тап за пределами сфер; жест, прерванный системой (Task 5).

---

### Task 1: `sdk::Shimmer`

**Риск:** высокий (ревью sonnet).

**Files:**
- Create: `core/sdk/shimmer.hpp`
- Modify: `core/tests/sdk_test.cpp` (тесты + вызов в `main` стр. ~5047), `docs/sdk-inventory.md` (§6)

**Interfaces:**
- Produces: `namespace sdk { class Shimmer { public: void prepare(double sr, int maxBlock); void reset(); void set(float amount, float decay, float tone, float mix); void process(const float* inL, const float* inR, float* outL, float* outR, int n); }; }` — `amount` 0..1 (доля сдвинутого сигнала в петле), `decay` 0..1 → обратная связь 0..0.55 (hard cap `kMaxFb = 0.55f`), `tone` 0..1 → ФНЧ петли 2..8 кГц, `mix` 0..1 сухой/влажный. Сдвиг +12 полутонов (два окна Ханна, 50 % перекрытие, окно ≈ 60 мс). Выход — только влажный сигнал × `mix` (сухой остаётся у вызывающего). Все буферы выделены в `prepare`.

- [ ] **Step 1: Failing tests в `sdk_test.cpp`:** `shimmerImpulseDecays` (импульс 1.0, decay=1, 10 с → хвост < 1e-3 и пик < 1.0); `shimmerPeakBounded` (белый шум ±1 на входе, все ручки = 1, 20 с → |out| ≤ 1.0, нет NaN/Inf); `shimmerOctave` (синус 440 Гц, decay=0, amount=1, mix=1 → доминирующая частота выхода 880 Гц ±1 %, через zero-cross по хвосту 1 с); `shimmerSilenceAndDc` (тишина → тишина; DC 0.5 не накапливается: |mean| < 0.01 после 5 с); `shimmerBlockSizes` (блоки 1 и 512 дают одинаковый выход ±1e-6).
- [ ] **Step 2:** `core/build.sh` → FAIL (нет заголовка).
- [ ] **Step 3: Implement `sdk::Shimmer` в `core/sdk/shimmer.hpp`** по Interfaces; в петле: ФНЧ (однополюсный), блокировка постоянной составляющей, `tanh` мягкое ограничение; ограничение выхода жёстким клипом ±1 как последняя страховка.
- [ ] **Step 4:** `core/build.sh && core/build/sdk-test` → PASS (`0 failed`).
- [ ] **Step 5:** строка о блоке в `docs/sdk-inventory.md` §6; commit `core: sdk Shimmer (octave-up, capped feedback)`.

### Task 2: Модель Paradisus

**Риск:** обычный.

**Files:**
- Create: `core/worlds/paradisus/model.hpp`, `core/worlds/paradisus/model.cpp`, `core/tests/paradisus_test.cpp`
- Modify: `core/build.sh` (добавить `paradisus_test` и модель во все 4 места, где перечислены `.cpp`)

**Interfaces:**
- Consumes: `sdk::Rng`, `Scale` хоста (`snapSemi`), референс `worlds/paradisus/model.js` (114 строк) и `tools/models/paradisus.test.mjs`.
- Produces: `namespace paradisus { constexpr int NSPH = 9, MAX_PIN = 4, NVOW = 5; struct Angel { int k; double v; bool pinned; int singers; double detune, vib, breath, reg, lvl; uint32_t seed; double dr; bool sung; }; struct Fx { double shimmer, decay, tone, mix, cathedral; }; class Model { public: explicit Model(const Scale&); void reset(uint32_t seed); int harm(int k) const; double hzOf(const Angel&) const; void formants(double v, double f[4], double bw[4], double g[4]) const; /* acts, ниже */ void touch(int id, int k, double v); void release(int id); bool togglePin(int id); void setFx(int key, double val); std::string snapshot() const; void restore(const std::string&); }; }`. `harm(k) = 9 − k`. Таблица `FORM` и интерполяция между гласными — перенос из `model.js`.

- [ ] **Step 1: Тесты в `paradisus_test.cpp`** (переносом из `.mjs`, CHECK-схема как `hymenaeus_test.cpp`): `harmMapping` (harm(0)=9, harm(8)=1); `formantsInterpolate` (v=0 → формант a; v между гласными — между таблицей); `pinLimit` (пятое закрепление отклонено, `togglePin` у закреплённого снимает); `hzOfScaleOff` (лад выключен → частота конечна и в 20..8000 Гц для всех k, v); `snapshotRoundTrip`; `restoreGarbage` (пустая и битая строка → модель в дефолте, без падения).
- [ ] **Step 2:** `core/build.sh` → FAIL.
- [ ] **Step 3: Implement `paradisus::Model`** по Interfaces; значения таблицы `FORM`, диапазоны ручек и дефолты — точно из `worlds/paradisus/model.js`.
- [ ] **Step 4:** `core/build.sh && core/build/paradisus-test` → `0 failed`.
- [ ] **Step 5:** commit `core: Paradisus model` (с `t2d.wasm`).

### Task 3: Звук и мир (ядро)

**Риск:** высокий (ревью sonnet).

**Files:**
- Create: `core/worlds/paradisus/sound.hpp`, `sound.cpp`, `world.hpp`, `world.cpp`
- Modify: `core/bridge/api.cpp` (include стр. ~24, `static paradisus::World par(host.scale());` стр. ~39, `{6, &par}` в `kSlots` стр. ~71), `core/build.sh`, `core/tests/paradisus_test.cpp`

**Interfaces:**
- Consumes: Task 1 `sdk::Shimmer`, Task 2 `Model`, `sdk::Saw`, `sdk::BpBank<4>`, `sdk::Rng`, `sdk::World`/`sdk::Out`.
- Produces: `paradisus::World : sdk::World` с `explicit World(const Scale&)`, `prepare`, `action(int kind, int i, const double* a, int n)`, `process(const sdk::Out&, int n)`, `rev() -> sdk::Rev::cathedral`, `mirror/heard/snapshot/restore/sounding`. `enum Act { TOUCH, RELEASE, PIN, VOWEL, ROSE, FX };` — порядок фиксирован. `constexpr int MIRROR_N` и раскладка зеркала — в шапке `world.hpp` (ангелы: `k, v, pinned, level`; `fx`; счётчики вспышек `chime`/Gloria через `heard`).

- [ ] **Step 1: Failing tests в `paradisus_test.cpp`:** `worldPeakBounded` (4 закреплённых ангела с `singers` max, `fx` все = 1, россыпь 10 раз, 30 с блоками 128 → пик `out.L/R` < 1.0, нет NaN/Inf); `worldSilentWhenIdle` (без касаний 2 с → RMS < 1e-4); `worldVowelChangesSpectrum` (гласная a vs i → разный спектральный центроид ≥ 10 %); `worldDeterministic` (одно зерно → одинаковый выход); `bridgeParadisus` (через `t2d_*` слот 6 доступен, `t2d_mirror` длины `MIRROR_N`); `worldSnapshotRoundTrip`.
- [ ] **Step 2:** `core/build.sh` → FAIL.
- [ ] **Step 3: Implement** `paradisus::Sound` (цепочка спеки §2: пилы → форманты → тело → дыхание/воздух → громкость, шины: сухой 0.6, шиммер, собор; Gloria и россыпь — расписание в сэмплах, 12 зёрен через 35 мс) и `paradisus::World` (действия, зеркало, снимок через `sdk::Json`, ключи `model`, `fx`).
- [ ] **Step 4:** `core/build.sh && core/build/paradisus-test && core/build/sdk-test` → `0 failed`.
- [ ] **Step 5:** commit `core: Paradisus world (chorus + shimmer), slot 6` (с `t2d.wasm`).

### Task 4: Зеркало и геометрия (Swift)

**Риск:** обычный.

**Files:**
- Create: `ios/app/T2D/Paradisus/ParMirror.swift`, `ParGeo.swift`, `ios/app/T2DTests/ParadisusTests.swift`
- Modify: `ios/app/project.yml` (`T2DTests.sources` += `T2D/Paradisus`)

**Interfaces:**
- Consumes: Task 3 раскладка зеркала и `Act` из шапки `world.hpp`; образец `HymMirror.swift`, `HymGeo.swift`; `worlds/paradisus/view.js` `geo()`.
- Produces: `enum Par { static let mirrorN: Int; static func angel(_ m: [Double], _ i: Int) -> ParAngel; ... }`, `enum ParAct: Int { case touch, release, pin, vowel, rose, fx }`, `struct ParGeo { init(size: CGSize, safeBot: CGFloat); var sph: [(rx: CGFloat, ry: CGFloat)]; var rose: CGPoint; func sphereAt(_ p: CGPoint) -> Int?; func angelRect(_ i: Int) -> CGRect }` (девять эллипсов, центр чуть выше середины, `k=0` внутри, `k=8` снаружи).

- [ ] **Step 1: Tests:** `testMirrorLayout` (`Par.mirrorN` == `MIRROR_N` из шапки), `testActNumbersMatchCore` (порядок `ParAct` == `enum Act`), `testSphereAtNearest`, `testGeoClearOfBottomBar` (320×568, 375×667, 393×852: корзина/нижняя панель не пересекает розу и сферы).
- [ ] **Step 2:** `xcodegen generate` + `xcodebuild test` → FAIL.
- [ ] **Step 3: Implement** `ParMirror.swift`, `ParGeo.swift` (геометрия — `view.js` `geo()`, `sphereAt` — ближайший эллипс).
- [ ] **Step 4:** тесты → PASS.
- [ ] **Step 5:** commit `ios: Paradisus mirror and geometry`.

### Task 5: Вид, жесты, плашки, регистрация

**Риск:** обычный.

**Files:**
- Create: `ios/app/T2D/Paradisus/ParDraw.swift`, `ParTouch.swift`, `ParSheets.swift`, `ParadisusView.swift`
- Modify: `ios/app/T2D/Worlds.swift` (WorldInfo slot 6, `title: "Paradisus"`, `roman: "VI"`, тема — `INK` 94,69,32 / `#f5efe1`, `view: true`), `ios/app/T2D/App.swift` (`makeView`: `case ParadisusView.slotN`), `ios/app/T2DTests/ParadisusTests.swift`

**Interfaces:**
- Consumes: Task 4 (`Par`, `ParAct`, `ParGeo`), образец `HymenaeusView`, `view.js`.
- Produces: `final class ParadisusView: World { static let slotN = 6 }`; `enum ParHit { case angel(Int), sphere(Int), rose, pinned(Int), none }`; `ParTouch.hit(_ p: CGPoint, geo: ParGeo, m: [Double]) -> ParHit`; плашка ангела и веер fx (шиммер: `amount`, `decay`, `tone`, `mix`; `cathedral`).

- [ ] **Step 1: Tests:** `testHitZones`, `testHoldSendsTouchRelease` (тап-держим → `TOUCH`, отпускание → `RELEASE`), `testAlongDragChangesVowel` (протяжка вдоль → `VOWEL`), `testAcrossDragChangesSphere` (поперёк → `TOUCH` с новым `k`), `testDoubleTapPins`, `testFifthPinRejectedShowsNoCrash` (пятое закрепление — `PIN` отправлен, состояние не ломается), `testRoseTapSendsRose`, `testInterruptedGestureReleases` (отмена касания системой → `RELEASE`).
- [ ] **Step 2:** FAIL.
- [ ] **Step 3: Implement** вид, жесты (чистые функции в `ParTouch`), плашки, регистрацию; клетка карты — `(2,3)` (сейчас занята выключенным Goetia — **ruling:** Paradisus занимает её, пока автор не назвал другую; Goetia в `Worlds.all` не возвращается).
- [ ] **Step 4:** `xcodebuild test` → PASS; сцена `-shot` для мира 6 (по образцу `hymenaeus`), `ios/app/shot.sh paradisus` — смотрит агент `shots`.
- [ ] **Step 5:** commit `ios: Paradisus view C, gestures, plates`.

### Task 6: Доки

**Риск:** обычный.

**Files:**
- Modify: `docs/worlds/paradisus.md`, `docs/worlds/README.md`, `CLAUDE.md` (таблица миров, строка VI), `README.md`, `docs/sdk-inventory.md`, `docs/tokens.md`

- [ ] **Step 1:** описать нативный Paradisus: звук (хор + шиммер, потолок обратной связи), жесты, Act-таблица, раскладка зеркала, известные ограничения (звук на слух не проверен, жесты на iPhone не смотрены), статус «только нативный iOS; в вебе архив».
- [ ] **Step 2:** строка в `docs/tokens.md` (`python3 tools/tokens.py --line`).
- [ ] **Step 3:** commit `docs: Paradisus native`.

---

## Self-Review

- Покрытие спеки: модель (T2), звук хора и Gloria/россыпь (T3), шиммер (T1), вид и жесты (T4–T5), доки (T6), риск перегруза (Review Focus 1 → T1/T3 тесты). Слот 6 в `Worlds.swift` проверяется в T5.
- Типы: `Shimmer::set(amount, decay, tone, mix)` ↔ `Fx{shimmer,decay,tone,mix,cathedral}` (T2) ↔ плашка fx (T5). `Act` порядок зафиксирован в T3 и зеркалится в `ParAct` (T4, тест).
- Открытое: клетка карты для Paradisus (принято `(2,3)`); точные значения `FORM`/дефолтов берутся из `model.js`, не придумываются.
