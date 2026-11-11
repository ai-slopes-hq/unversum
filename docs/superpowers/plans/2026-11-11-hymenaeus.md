# Hymenaeus (XIV) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Нативный мир Hymenaeus (XIV, слот 14): нить жемчужин (Marbles-lite) → три кольца (порт Rings) → облака (свой гранулятор) → общий ревербер.

**Architecture:** Мир SDK в `core/worlds/hymenaeus/` (`model`, `rings/`, `cloud`, `sound`, `world`) по образцу Horologium; регистрация в `core/bridge/api.cpp`. Вид, жесты и плашка `fx` — Swift в `ios/app/T2D/Hymenaeus/`. Параметры меняются только методами модели, жесты и плашка пишут в одни и те же `action`.

**Tech Stack:** C++17 (`core/sdk/*`), Swift + CoreGraphics, XCTest, `core/tests/*` с макросом `CHECK`.

**Spec:** `docs/superpowers/specs/2026-11-11-hymenaeus-design.md`. Макет: `mockups/hymenaeus.html`. Исходники Rings (MIT): `/Users/noise/code/mutableinstruments/rings`.

## Global Constraints

- Только нативный мир: в вебе его нет. Номер XIV, слот 14, русского текста в интерфейсе нет (названия латынью/по-английски).
- Модель — без DOM, Web Audio, `Math.random`; случайность только `sdk::Rng` от seed из `prepare`. `state` фасада правят только методы модели.
- Блок есть в `core/sdk/` — брать его, формулу не копировать. Своего `hall()` нет — только посыл в общий ревербер (`rev()`).
- Rings: три голоса, 24–32 моды (не 64), без `String`/`FM`/полифонии 4; Clouds: буфер 2–4 с, без вокодера и pitch shifter; Marbles: без XY и T-генератора.
- Производительность: холст не плотнее 2× (`DPR`), никакого `shadowBlur`, градиенты не каждый кадр, в кадр никаких полноэкранных проходов, неслышимое отцеплено; блок ≤ 128 сэмплов.
- Светлый мир: чернила `44,38,30` на слоновой кости `#f2ede2`, золото `168,124,48`; шрифты IM Fell English (поле) и EB Garamond (подписи, плашки); жирного нет; подписи на поле только пока тянешь.
- Коммиты на английском, с телом-списком; правка `core/` требует пересобранного `t2d.wasm` (`core/build.sh`), на пуше — новый `CACHE` в `sw.js` = `?v=` в `index.html`.
- MI-код — MIT: заголовок с авторством и текстом лицензии в каждом портированном файле.

## Review Focus

- Длина нити 1 или смена `len` на ходу (индекс шага за пределами): шаг не выходит за массив, играет без щелчков.
- Все жемчужины — паузы (и déjà vu = 1): тишина, без деления на ноль и без зависшего звона.
- `freeze` на пустом или тихом буфере: без NaN и без шума из ниоткуда.
- Транспорт: bpm ≤ 0, скачок сетки MIDI-часов, остановка посреди такта: не больше одного шага на сэмпл-границу, нет лавины ударов за блок, нет зависших нот.
- `restore()` с мусорным JSON, NaN/inf и значениями вне диапазона в `action`: мир остаётся в допустимом состоянии; кольцо с damping = 0 не уходит в самоколебание.

---

## Этап 1. Модель и ядро без звука

### Task 1: Модель — нить, déjà vu, лад, кольца

Риск: обычный — чистая модель без памяти и потоков.

**Files:**
- Create: `core/worlds/hymenaeus/model.hpp`, `core/worlds/hymenaeus/model.cpp`, `core/tests/hymenaeus_test.cpp`
- Modify: `core/build.sh` (сборка и запуск `hymenaeus-test`)

**Interfaces:**
- Produces (`namespace hymenaeus`):
  - `constexpr int NR = 3, PEARL_MAX = 16, PEARL_DEF = 11, NRK = 4;` `enum RingKey { STRUCT, BRIGHT, DAMP, LVL };` `enum CloudKey { POS, SIZE, DENS, PITCH, FREEZE, ECHO, CLEVEL, NCK };`
  - `struct Pearl { double pitch = 0.5; bool rest = false; };` (pitch 0…1)
  - `struct Step { bool hit; int ring; double hz; double vel; int pearl; };`
  - `class Model { explicit Model(const Scale&); void reset(uint32_t seed = 1); void setDejaVu(double); void setLen(int); void togglePearl(int); void setPearlPitch(int, double); void setRest(int, bool); void roll(); void clear(); void setRing(int ring, int key, double v); void setCloud(int key, double v); Step next(); int ringFor(double pitch) const; std::string snapshot() const; void restore(const std::string&); bool sounding() const; accessors dejavu(), len(), pearl(i), ring(i,key), cloud(key), cursor(); }`
  - `next()` двигает курсор по нити и возвращает шаг: жемчужина выбирает кольцо через `ringFor` (полосы высоты 0…⅓, ⅓…⅔, ⅔…1 → кольца 0 (большое, низкое), 1, 2 (малое)), `hz` — через квантайзер в лад хоста (`Scale`, тоника 55 Гц × лад).
- Consumes: `Scale` из `core/host/scale.hpp`, `sdk::Rng` из `core/sdk/rng.hpp`, JSON-помощники `core/sdk/json.hpp`.

- [ ] **Step 1: Failing tests в `core/tests/hymenaeus_test.cpp`** (формат — как `horoModelLen` в `sdk_test.cpp`): `modelSeedDeterministic` (два `Model` с seed 5: 40 вызовов `next()` совпадают); `modelDejaVuOneRepeats` (после `setDejaVu(1)` последовательность `hz` из `len` шагов повторяется на следующем круге); `modelDejaVuZeroVaries` (при 0 за 8 кругов не все круги равны); `modelRestsSilent` (`togglePearl` на всех жемчужинах → `next().hit == false` на 3 круга); `modelRingBands` (`ringFor(0.1)==0`, `ringFor(0.5)==1`, `ringFor(0.9)==2`); `modelClamps` (`setLen(0)→1`, `setLen(99)→16`, NaN игнорируется, `setRing(…, NAN)`/`±inf` зажаты в 0…1); `modelSnapshotRoundTrip`; `modelRestoreGarbage` (`restore("{")`, `restore("[]")`, пустая строка: состояние допустимо, `next()` не падает).
- [ ] **Step 2: Собрать и убедиться, что падает** — `core/build.sh` (ошибка компиляции: нет `model.hpp`).
- [ ] **Step 3: Реализовать `Model`** в `model.hpp/.cpp`. Подробность: петля déjà vu — буфер из `len` значений высоты; на каждом шаге с вероятностью `1 − dejavu` значение заменяется новым из `rng_`, иначе берётся прежнее (как в Marbles `random_sequence`); паузы — по флагу `rest`.
- [ ] **Step 4: Прогнать** — `core/build/hymenaeus-test`, ожидание: все `CHECK` проходят, код возврата 0.
- [ ] **Step 5: Commit** — `core: Hymenaeus model — thread, deja vu, ring bands`.

> **Приёмка этапа 1** — автору нечего слушать; достаточно зелёных тестов, этап принимается вместе с этапом 2.

---

## Этап 2. Звук

### Task 2: Кольца — порт Rings (модальный резонатор и plucker)

Риск: высокий — realtime-аудио, ручные буферы и таблицы, риск неустойчивости фильтров.

**Files:**
- Create: `core/worlds/hymenaeus/rings/resonator.hpp`, `rings/plucker.hpp`, `rings/tables.hpp`, `rings/LICENSE-MI.txt`
- Modify: `core/tests/hymenaeus_test.cpp`
- Источник: `rings/dsp/resonator.{h,cc}`, `plucker.h`, нужные таблицы из `rings/resources.cc` (stiffness, svf shift), SVF/Follower — из `stmlib/dsp/filter.h`, без остальной stmlib.

**Interfaces:**
- Produces: `rings::Resonator { void prepare(double sr); void setFreq(double hz); void setStructure(double); void setBrightness(double); void setDamping(double); float process(float in); bool active() const; }` (макс. 28 мод, все буферы — члены, без аллокаций в `process`); `rings::Plucker { void trigger(double pos, double vel); float next(); }` (burst в резонатор, `pos` 0…1 — место удара).
- Consumes: `sdk::Rng` для шума удара.

- [ ] **Step 1: Failing tests:** `ringsRingsDown` (импульс → RMS затухает: хвост через 2 с в 100 раз тише пика при damping 0.5); `ringsDampingMonotone` (больше damping → короче хвост); `ringsBrightnessCentroid` (brightness 0.9 даёт спектральный центроид выше, чем 0.1, по DFT 2048); `ringsStable` (damping 0, structure 0/1, brightness 0/1, hz 20/8000, 10 с шума на входе: нет NaN/inf, пик < 4.0); `ringsIdleCheap` (`active()==false` после затухания и `process` не считает моды); `ringsNoAlloc` (размер объекта фиксирован, `static_assert` на `sizeof` ≤ 64 КБ).
- [ ] **Step 2: Собрать — падает** (нет заголовков).
- [ ] **Step 3: Портировать** `Resonator` и `Plucker` в float, namespace `rings`, зависимости stmlib заменить встроенными копиями SVF; число мод — константа 28; damping зажать так, чтобы t60 ≤ 20 с (против самоколебания). В шапке каждого файла — авторство Émilie Gillet и текст MIT.
- [ ] **Step 4: Прогнать тесты** — все проходят.
- [ ] **Step 5: Commit** — `core: Hymenaeus rings — port of MI Rings modal resonator and plucker`.

### Task 3: Облака — гранулятор

Риск: высокий — realtime-аудио, кольцевой буфер, ручная память.

**Files:**
- Create: `core/worlds/hymenaeus/cloud.hpp`, `core/worlds/hymenaeus/cloud.cpp`
- Modify: `core/tests/hymenaeus_test.cpp`

**Interfaces:**
- Produces: `hymenaeus::Cloud { void prepare(double sr, uint32_t seed); void write(float l, float r); void set(double pos, double size, double dens, double pitch, bool freeze, double echo); void process(float& l, float& r); bool sleeping() const; }`. Буфер — 3 с стерео, фиксированный `std::array`; до 24 зёрен одновременно, окно Хэннинга по таблице, питч зерна ±12 пт; `echo` — обратная связь в буфер; `freeze` — запись стоит, читается замороженное. Использовать `sdk::Rng`, `sdk::OnePole` по месту.

- [ ] **Step 1: Failing tests:** `cloudSilentIn` (тишина на входе и пустой буфер → выход 0, `sleeping()==true`); `cloudEchoesInput` (импульс → через `pos·3 с` выход ненулевой); `cloudFreezeHolds` (после `freeze` и тишины на входе выход держится > 2 с); `cloudFreezeEmpty` (freeze на пустом буфере → ровно 0, без NaN); `cloudNoNan` (случайные `set` со значениями 0, 1, NaN→зажато, 100 кадров по 128 × 200 блоков); `cloudDensityBounded` (dens = 1 не даёт > 24 живых зёрен и пик < 4.0); `cloudSleepsAgain` (после затухания снова `sleeping()`).
- [ ] **Step 2: Собрать — падает.**
- [ ] **Step 3: Реализовать** `Cloud`; `set` зажимает все аргументы в допустимые диапазоны, NaN игнорирует.
- [ ] **Step 4: Прогнать тесты** — все проходят.
- [ ] **Step 5: Commit** — `core: Hymenaeus cloud — grain buffer with freeze and echo`.

### Task 4: Мир, звук и регистрация в ядре

Риск: высокий — realtime-аудио, публичный C-ABI мост, слот в общем модуле wasm.

**Files:**
- Create: `core/worlds/hymenaeus/sound.hpp`, `sound.cpp`, `world.hpp`, `world.cpp`
- Modify: `core/bridge/api.cpp` (`static hymenaeus::World hym{…}` и `{14, &hym}` в `kSlots`), `core/build.sh` (все списки .cpp: `sdk-test`, `mix-cli`, `t2d.wasm`, `hymenaeus-test`), `core/tests/hymenaeus_test.cpp`
- Rebuild: `t2d.wasm` (`core/build.sh`)

**Interfaces:**
- Produces (`hymenaeus::World : sdk::World`): `enum Act { RING, CLOUD, PEARL, STRIKE, THREAD, DICE, CLEAR, RUN, BPM };`
  - `RING` i = кольцо, a = [RingKey, v]; `CLOUD` a = [CloudKey, v]; `PEARL` i = жемчужина, a = [0 pitch | 1 rest, v]; `STRIKE` i = кольцо, a = [pos, vel]; `THREAD` a = [0 dejavu | 1 len, v]; `RUN` a = [on]; `BPM` a = [bpm]; `ACT_LFO` (i == −1) a = [key, v]: 0…3·NR−1 — кольцо key/NRK, ключ key%NRK; затем CloudKey.
  - `mirror`: `[run, bpm, dejavu, len, cursor, затем на жемчужину PEARL_MAX × (pitch, rest), на кольцо NR × (struct, bright, damp, lvl, energy), cloud NCK значений, cloudEnergy]`; константа `MIRROR_N` в `world.hpp`.
  - `heard`: `[ударов, номера колец…, номер жемчужины]` с прошлого раза.
  - `rev() == sdk::Rev::hall`; шаги — `sdk::StepClock` по темпу или сетке MIDI (`attach`).
- Consumes: `Model` (Task 1), `rings::Resonator/Plucker` (Task 2), `Cloud` (Task 3), `sdk::Clock`, `sdk::StepClock`.

- [ ] **Step 1: Failing tests:** `worldSilentWhenStopped` (без RUN и без STRIKE: `sounding()==false`, выход ровно 0); `worldStepsInTime` (bpm 120, RUN: за 4 с ровно 32 шага ±1 по `heard`); `worldStrikeSounds` (`STRIKE` кольца 1 → пик выхода в блоке > 0.01 и затухание до тишины); `worldNoStepFlood` (bpm 0, bpm −5, bpm 1e9, скачок сетки: ≤ 1 шаг на блок); `worldStopNoStuck` (RUN выкл. посреди такта → через 25 с `sounding()==false`); `worldSnapshotRoundTrip`; `worldGarbageActions` (все `Act` с NaN/inf/индексами вне диапазона: нет падений, нет NaN в выходе); `worldBudget` (блок 128 при трёх звенящих кольцах и плотных облаках < 25 % времени блока на 48 кГц — как `snapshotBudget`); `worldSlot14` (`t2d_init` кладёт мир в слот 14, `t2d_mirror(14,…)` отдаёт `MIRROR_N` чисел).
- [ ] **Step 2: Собрать — падает.**
- [ ] **Step 3: Реализовать** `World`/`Sound`: на шаг модели — `plucker.trigger` в выбранное кольцо с частотой `hz` и лёгкой громкостью; сумма колец уходит и в сухой выход, и в `cloud.write`; облако на выход; посыл в ревербер — как у Horologium (сглаженный, `OnePole`). Неслышимое кольцо (`!active()`) не считается; облако спит, пока `sleeping()`.
- [ ] **Step 4: Зарегистрировать** в `api.cpp` и `build.sh` (слот 14, миры и тесты), пересобрать `core/build.sh` (включая `t2d.wasm`).
- [ ] **Step 5: Прогнать** — `core/build/hymenaeus-test`, `core/build/sdk-test`, `core/build/mix-cli`: всё зелёное, существующие тесты не сломаны.
- [ ] **Step 6: Commit** — `core: Hymenaeus world — thread, rings and clouds in slot 14` (в коммите же `t2d.wasm`).

---

### Task 4b: Режимы колец — sympathetic strings и полифония 1 / 2 / 4 (автор, 11.11)

Риск: высокий — звук и состояние ядра. Ревью: `sonnet` после задачи.

Автор хочет «жёлтый режим» Rings со своей полифонией и автоматизацию `structure` ради характерного глайда. Решения: режим **свой у каждого кольца** (modal | strings), полифония **1 / 2 / 4 на кольцо** переключателем; в строковом режиме `structure` двигает аккорд и строй струн (развёртка = глайд), LFO на `structure` уже есть (ключ `ring·STRUCT`).

**Files:**
- Create: `core/worlds/hymenaeus/rings/string.hpp` (порт `rings/dsp/string.cc/h`, без `FM`), `rings/chords.hpp` (таблица аккордов из `part.cc`)
- Modify: `model.{hpp,cpp}` (RingKey `MODE`, `POLY`; snapshot; зажим), `sound.{hpp,cpp}` (голоса по кругу, струнный путь по `part.cc`: sympathetic strings), `world.{hpp,cpp}` (ключи действий, зеркало: +2 числа на кольцо → `MIRROR_N`), `core/tests/hymenaeus_test.cpp`; `core/build.sh` (wasm пересобрать), `ios/app/T2D/Hymenaeus/HymMirror.swift` (`ringN`, `mirrorN`)
- Лицензия MI — тот же `rings/LICENSE-MI.txt`

**Interfaces:**
- Produces: `RingKey { STRUCT, BRIGHT, DAMP, LVL, MODE, POLY }` (`NRK = 6`; MODE 0 = modal, 1 = strings; POLY 0…1 → 1 / 2 / 4); зеркало кольца: `struct, bright, damp, lvl, mode, poly, energy` (7 чисел).
- Consumes: Task 2–4 (`Resonator`, `Plucker`, `Sound::strike`).

- [ ] **Step 1: Failing tests** — `modelRingModePoly` (зажим, snapshot round-trip, мусор), `soundStringsSound` (в режиме strings удар звучит, пик < 4, затухает), `soundStructureGlide` (развёртка `structure` 0→1 за 2 с в strings: спектральный центр движется монотонно без щелчков — нет скачков > порога между блоками), `soundPolyVoices` (poly 4: четыре удара подряд звучат одновременно, пятый забирает самый старый), `soundModeSwitchLive` (смена режима на звенящем кольце — без NaN и щелчка), `worldMirrorSize`.
- [ ] **Step 2: Run** — падает.
- [ ] **Step 3: Реализовать** порт `String` и таблицу аккордов (по `part.cc` ProcessInternal для `RESONATOR_MODEL_SYMPATHETIC_STRING`), голоса кольца по кругу, MODE/POLY в модели, действие `RING` с ключами 4–5, LFO-ключи расширить до `NR·NRK`.
- [ ] **Step 4: Run** — всё зелёно; `core/build.sh`; sdk-test, mix-cli.
- [ ] **Step 5: Commit** (с `t2d.wasm`); Swift: `ringN = 7`, `mirrorN = 5 + 32 + NR·7 + NCK + 1 = 67`, тест зеркала.

## Этап 3. Вид и жесты (Swift)

### Task 5: Каркас — регистрация, зеркало, вид по макету, тап-удар

Риск: обычный — UI.

**Files:**
- Create: `ios/app/T2D/Hymenaeus/HymenaeusView.swift`, `HymMirror.swift`, `HymDraw.swift`, `HymGeo.swift`; `ios/app/T2DTests/HymenaeusTests.swift`
- Modify: `ios/app/T2D/Worlds.swift` (строка WorldInfo: slot 14, «Hymenaeus», roman `XIV`, тема слоновая кость/чернила/золото, ячейка карты 4×4), `ios/app/T2D/Map/Emblems.swift` (`case 14` — эмблема: жемчужная нить с кольцом), `ios/app/T2D/App.swift` (если нужен `makeView`)

**Interfaces:**
- Produces: `struct HymMirror { init(_ m: [Double]) ; let run: Bool; let bpm, dejavu: Double; let len, cursor: Int; let pearls: [(pitch: Double, rest: Bool)]; let rings: [RingState]; let cloud: [Double]; … }` — разбор по `MIRROR_N` из Task 4; `enum Act: Int { ring, cloud, pearl, strike, thread, dice, clear, run, bpm }` с теми же номерами, что в `world.hpp`; `HymGeo` — прямоугольники зон (облака, кольца, нить, солнце) от масштаба `S = min(W/390, H/844)` по макету.
- Consumes: C-ABI `t2d_app_mirror/t2d_app_heard/t2d_app_action` (как `HorologiumView.swift`).

- [ ] **Step 1: Failing tests** в `HymenaeusTests.swift`: `testMirrorParse` (вектор длины `MIRROR_N` → поля на своих местах; короче — не падает, нули); `testGeoZonesDisjoint` (зоны облаков, колец, нити не пересекаются на 390×844 и 430×932); `testActNumbersMatchCore` (значения `Act` равны константам ядра, прочитанным через мост/константный список).
- [ ] **Step 2: Запустить** — `cd ios/app && xcodegen && xcodebuild test -project T2D.xcodeproj -scheme T2D -destination 'platform=iOS Simulator,name=<устройство>' -only-testing:T2DTests/HymenaeusTests` — падает (нет типов).
- [ ] **Step 3: Реализовать** регистрацию, зеркало и вид по макету (`mockups/hymenaeus.html`: солнце-четверть, облака с рисинками и штриховкой, кольца-ободки, нить с жемчужинами, нити звука) и тап по кольцу → `Act.strike`; тап по солнцу → карта. Живое: текущая жемчужина светится, звёзды-ноты по `heard`. Без `shadowBlur`, градиенты кэшировать.
- [ ] **Step 4: Прогнать тесты — зелёные; сценарий скриншотов агентом `shots`** (вердикт текстом).
- [ ] **Step 5: Commit** — `ios: Hymenaeus world shell — view, mirror, map card, tap strike`.

> **Приёмка этапа 2 (звук)** — после Task 5 автор слушает мир на iPhone (нить играет сама, тап бьёт кольца, облака отзываются). Дальше — только после его слова.

### Task 6: Жесты по осям

Риск: обычный — UI.

**Files:**
- Create: `ios/app/T2D/Hymenaeus/HymTouch.swift`
- Modify: `HymenaeusView.swift`, `HymDraw.swift` (подписи при перетаскивании), `HymenaeusTests.swift`

**Interfaces:**
- Produces: `enum HymGesture { case cloudDrag(x: Double, y: Double), cloudHold, echoDrag(x: Double), ringStrike(ring: Int, pos: Double), ringAxes(ring: Int, bright: Double, damp: Double), ringPinch(ring: Int, structure: Double), claspDrag(Double), pearlTap(Int), pearlPitch(Int, Double) }` и `func HymTouch.classify(at: CGPoint, geo: HymGeo, …) -> HymGesture?`; каждый жест → один `core.action(Act…)` из таблицы Task 4.

- [ ] **Step 1: Failing tests:** `testCloudDragMapsAxes` (X → `CloudKey.POS`, Y → `SIZE`, значения 0…1, зажаты); `testHoldFreezes`; `testRingAxes` (вертикаль → `BRIGHT`, горизонталь → `DAMP`); `testPinchStructure`; `testClaspDragDejaVu`; `testPearlTapTogglesRest`; `testPearlVerticalPitch`; `testDragOutsideZonesIgnored`; `testNaNPointIgnored`.
- [ ] **Step 2: Запустить `HymenaeusTests` — падают.**
- [ ] **Step 3: Реализовать** `classify` и подключение жестов; подписи (`positio · granum`, `echo`, `glacies · freeze`, `anuli`, `margaritae`, `déjà vu`) видны только пока жест идёт.
- [ ] **Step 4: Тесты зелёные; `shots` — сценарий с перетаскиванием облака, кольца и застёжки.**
- [ ] **Step 5: Commit** — `ios: Hymenaeus axis gestures for clouds, rings and thread`.

### Task 7: Верхняя строка и плашка `fx`

Риск: обычный — UI.

**Files:**
- Create: `ios/app/T2D/Hymenaeus/HymSheets.swift`
- Modify: `HymenaeusView.swift`, `HymenaeusTests.swift`

**Interfaces:**
- Produces: `fxSpec`/`ringSpec`/`cloudSpec` (формат `SheetSpec` как в `HoroSheets.swift`): ползунки structure/brightness/damping/level на кольцо, pos/size/dens/pitch/echo/level + freeze в облаках, déjà vu и длина нити; LFO по `ACT_LFO` (`obj: -1`, ключи как в Task 4). Кнопки верхней строки: `lad`, `steps` (длина 4…16), `fx`, `dice`, `undo`, `trash` — `dice → Act.dice`, `trash → Act.clear`.

- [ ] **Step 1: Failing tests:** `testSpecKeysMatchCore` (каждый ключ плашки есть в `CloudKey/RingKey`/`THREAD`); `testSliderWritesSameAction` (ползунок и жест с тем же значением дают одинаковый `Act` и `a`); `testStepsClamp` (4…16); `testUndoRestoresDice` (dice → undo возвращает прежние жемчужины).
- [ ] **Step 2: Запустить — падают.**
- [ ] **Step 3: Реализовать** плашку и кнопки; жест → зеркало → ползунок (и наоборот) читают одно состояние из `HymMirror`.
- [ ] **Step 4: Тесты зелёные; `shots` — плашка `fx` и верхняя строка.**
- [ ] **Step 5: Commit** — `ios: Hymenaeus fx plate and top bar`.

### Task 8: Документация и лицензия MI

Риск: обычный — документация.

**Files:**
- Create: `docs/worlds/hymenaeus.md`
- Modify: `docs/worlds/README.md` (строка XIV), `CLAUDE.md` (таблица миров), `README.md`, `docs/tokens.md` (строка в конце задачи), `ROADMAP.md` по необходимости
- Уже лежит: `core/worlds/hymenaeus/rings/LICENSE-MI.txt` (Task 2)

- [ ] **Step 1: Написать `docs/worlds/hymenaeus.md`:** жесты, звук, функции, тесты (по образцу `docs/worlds/horologium.md`); отметить, что экран лицензий релиза пока не создан и MIT-текст лежит в `LICENSE-MI.txt` (релиз отложен).
- [ ] **Step 2: Обновить таблицы** в `docs/worlds/README.md`, `CLAUDE.md`, `README.md`.
- [ ] **Step 3: Строка в `docs/tokens.md`** (`python3 tools/tokens.py --line`).
- [ ] **Step 4: Commit** — `docs: Hymenaeus world notes and tables`.

---

## Self-Review (план против спеки)

- Покрытие: модель, кольца, облака, ядро/слот — Task 1–4; экран, жесты, плашка, верхняя строка — Task 5–7; документация, лицензия MI — Task 8; веб, режимы Rings/Clouds, многоканальный выход — вне объёма.
- Отступление от спеки: в Rings переносятся только модальный `Resonator` и `Plucker` (`String` — для струнных моделей, в одном голосе модального режима не нужен); экран лицензий не создаётся (его нет в приложении, релиз отложен), текст MIT лежит в `LICENSE-MI.txt` и в шапках файлов.
- Согласованность имён: `RingKey {STRUCT, BRIGHT, DAMP, LVL}`, `CloudKey {POS, SIZE, DENS, PITCH, FREEZE, ECHO, CLEVEL}`, `Act` и `MIRROR_N` определены в Task 1/4 и используются в Task 5–7 теми же именами.
