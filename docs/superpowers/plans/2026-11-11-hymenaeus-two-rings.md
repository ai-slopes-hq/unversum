# Hymenaeus: два кольца, две нити, локации, mix — план

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** переделать Hymenaeus (слот 14, только iOS) на два кольца с двумя нитями, четыре локации нот, mix dry/wet, два кубика и новый вид C; корзина очищает мир до конца.

**Architecture:** вся логика в ядре (`core/worlds/hymenaeus/`), Swift только рисует и шлёт `Act`. Модель хранит `thr_[2]`, `loc_[4]`, mix; снимок включает всё, кроме облаков-звука. Вид — по эскизу `mockups/hymenaeus-v2.html?v=C`.

**Tech Stack:** C++ ядро → `t2d.wasm` (`core/build.sh`), Swift/CoreGraphics (`ios/app`, `xcodegen`), тесты `core/tests/hymenaeus_test.cpp`, `ios/app/T2DTests/`.

**Spec:** `docs/superpowers/specs/2026-11-11-hymenaeus-two-rings-design.md`

## Global Constraints

- `NR = 2`: кольцо 0 — modal, кольцо 1 — strings; `RingKey` = STRUCT, BRIGHT, DAMP, LVL, POLY; `NRK = 5`.
- Нить: `Thread { p[16], len, cur, dejavu, lo, hi }`; границы `lo/hi` 24…96, ширина ≥ 5; по умолчанию 45…69 и 57…81.
- Локаций 4; хранят `{ p[16], len, dejavu, lo, hi }` обеих нитей; смена не трогает кольца, облака, mix.
- Mix по умолчанию 0.5; сухое `min(1, 2(1−mix))`, влажное `min(1, 2·mix)`; при 0.5 звук как раньше.
- LFO-ключи: кольцо `i`, ключ `k` → `i·5 + k` (0…9); облака → `10 + k`.
- Act: `THREAD a=[key,v]` с нитью в `i` (0 dejavu, 1 len, 2 lo, 3 hi); `PEARL a=[нить, 0 высота|1 пауза, v]`; `DICE a=[нить]` (пусто или −1 — обе); новые `TIMBRE`, `LOC`, `MIX`.
- Старые снимки не поддерживаются (restore → дефолт). Модель — без DOM/Web Audio/`Math.random` (только `rng`).
- Палитра Paradisus: чернила `94,69,32`, фон `#f5efe1`, золото `176,131,44`; без `shadowBlur`, подписи только во время действия.
- Новый файл Swift — `xcodegen` в `ios/app`; правка `core/` — вместе с `t2d.wasm`. Коммиты по-английски, с телом-списком.
- Изменение относительно спеки (запрос автора 11.11): **`CLEAR` очищает мир до конца**, а не только нить (см. Task 3).

## Review Focus

- Нить с `len = 1` и нить с `len = 16` шагают от общих часов без дрейфа курсоров друг у друга.
- `lo/hi` из мусора (NaN, `lo > hi`, ширина < 5, за 24…96) зажимаются, не падают.
- `restore` старого одно-нитевого снимка и пустой строки даёт рабочий дефолт.
- `LOC` с номером вне 0…3 игнорируется; быстрые `LOC` подряд не теряют правок активной локации.
- `mix` NaN/вне 0…1 зажимается; mix=0 и 1 не дают NaN в выходе.
- `CLEAR`, потом undo — точное возвращение прежнего мира; `CLEAR` дважды — не плодит шагов undo.

---

### Task 1: Модель — два кольца, две нити, диапазон

**Риск:** высокий — ядро, ручная работа с массивами C++, публичный формат снимка.

**Files:**
- Modify: `core/worlds/hymenaeus/model.hpp`, `model.cpp`
- Test: `core/tests/hymenaeus_test.cpp`

**Interfaces:**
- Produces: `constexpr int NR=2, NRK=5, NLOC=4`; `struct Thread {Pearl p[PEARL_MAX]; int len, cur; double dejavu; int lo, hi;}`; `Model::thread(int t) const -> const Thread&`; `Step next(int t)` (поле `ring` = `t`, `hz` из `lo/hi`); `void setRange(int t, double lo, double hi)`; `void roll(int t = -1)`; `setLen/setDejaVu/togglePearl/setRest/setPearlPitch` получают первым аргументом `int t`; `double hzOf(int t, double pitch) const`; `bool sounding() const`.

- [ ] **Step 1: Тесты (падают).** В `hymenaeus_test.cpp` заменить/добавить: `modelTwoThreadsIndependent` (len 3 и 5, 15 шагов `next(0)`/`next(1)` — курсоры `cur` 0 и 1 равны `15%3`, `15%5`), `modelRangeClamp` (`setRange(0, 10, 200)` → 24…96; `setRange(0, 60, 62)` → ширина 5; NaN игнорируется; `lo > hi` меняет местами), `modelPitchWithinRange` (1000 шагов: `hz` нити 0 в пределах нот `lo..hi` после снапа, т.е. MIDI-номер ∈ [lo−1, hi+1] для лада chromatic), `modelDeterministic` (два `Model` с seed 7 дают равные снимки), `modelRingsCount` (`NR == 2`, `NRK == 5`), `modelDefaultRanges` (45…69, 57…81).
- [ ] **Step 2:** `core/build.sh && core/build/hymenaeus-test` — FAIL (не компилируется).
- [ ] **Step 3:** Реализовать `Thread`, `thr_[NR]`, убрать `MODE`, `modeOf`, `ringFor`; `hzOf(t,p) = scale.snap(lo + p·(hi−lo))`; `reset(seed)` бросает обе нити из одного `rng_`. Снимок пока `thr[2]`, `rings[2×5]`, `cloud[8]`, `rng` (локации и mix — Task 2).
- [ ] **Step 4:** тесты — PASS.
- [ ] **Step 5:** `git add core && git commit -m "core: Hymenaeus two threads, two rings, pitch range"` (wasm — в конце Task 4).

### Task 2: Модель — локации, mix, TIMBRE, снимок

**Риск:** высокий — формат снимка, зажимы недоверенного JSON.

**Files:** Modify `core/worlds/hymenaeus/model.hpp/.cpp`; Test `core/tests/hymenaeus_test.cpp`.

**Interfaces:**
- Consumes: Task 1.
- Produces: `void setLoc(int slot)`; `int loc() const`; `void setMix(double v)`; `double mix() const` (по умолчанию 0.5); `void rollTimbre()` (STRUCT, BRIGHT, DAMP колец и POS, SIZE, DENS, PITCH облаков в безопасных пределах — те же, что у ручек; уровни, mix, send, freeze не меняет); `snapshot()` с `loc[4]`, `curLoc`, `mix`, `thr`, `rings`, `cloud`, `rng`.

- [ ] **Step 1: Тесты:** `modelLocSwapKeepsSound` (после `setLoc(1)` кольца, облака, mix равны прежним; ноты нитей равны `loc_[1]`), `modelLocKeepsEdits` (правка жемчужины в слоте 0, `setLoc(1)`, `setLoc(0)` — правка на месте; курсоры `cur` не сброшены), `modelLocOutOfRange` (`setLoc(-1)`, `setLoc(4)` ничего не меняют), `modelLocsDiffer` (после `reset` слоты 1–3 различны), `modelDiceKeepsTimbre` (`roll()` не меняет `ring_/cloud_/mix`), `modelTimbreKeepsNotes` (`rollTimbre()` не меняет ноты, LVL, SEND, FREEZE, mix; сам тембр меняется; значения в границах), `modelMixClamp` (NaN игнорируется, 2 → 1, −1 → 0), `modelSnapshotRoundTrip` (снимок → restore → равный снимок, включая локации), `modelRestoreGarbage` (пустая строка, `{}`, старый формат с ключом `p`, числа вместо массивов — дефолт без падения).
- [ ] **Step 2:** запуск — FAIL.
- [ ] **Step 3:** Реализовать; `reset` заполняет слоты 1–3 разными мелодиями из `rng_`; `setLoc` сохраняет активные нити в старый слот и подменяет нити из нового.
- [ ] **Step 4:** PASS.
- [ ] **Step 5:** commit `core: Hymenaeus locations, mix, timbre dice, new snapshot`.

### Task 3: Полная очистка мира (CLEAR) и undo

**Риск:** высокий — состояние мира, снимки undo, асинхронность кольца.

**Files:** Modify `core/worlds/hymenaeus/model.hpp/.cpp`; Test `core/tests/hymenaeus_test.cpp`, `ios/app/T2DTests/UndoTests.swift` или `HymenaeusTests.swift`.

**Interfaces:**
- Produces: `void Model::clearAll()` — обе нити активной локации паузы, `dejavu/len/lo/hi` дефолт, слоты 1–3 пустые (паузы), кольца и облака (кроме `FREEZE` в 0) — значения по умолчанию, mix 0.5, `cur = 0`, `curLoc = 0`; `rng_` не трогается. `sounding()` после него `false`. (`clear()` из Task 1 — нить-только — удаляется.)

- [ ] **Step 1: Тесты ядра:** `modelClearAllIsEmpty` (после правок: `setRing`, `setCloud`, `setMix`, `setLoc(2)`, ноты → `clearAll()`; все жемчужины всех слотов паузы, `!sounding()`, кольца/облака/mix равны `Model` после `reset` без нот); `modelClearUndoRestores` (`s = snapshot()`; `clearAll()`; `restore(s)`; `snapshot() == s`); `modelClearTwice` (второй `clearAll()` не меняет снимок); `worldClearSilences` (в `World`: запустить, `CLEAR`, за 2 с блоков `process` выход ≈ 0 после затухания звона — пик последней 0.5 с < 1e-4).
- [ ] **Step 2:** FAIL.
- [ ] **Step 3:** Реализовать `clearAll`; `World::action CLEAR` вызывает его и сбрасывает ручные удары `nman_` и `heard`.
- [ ] **Step 4:** PASS.
- [ ] **Step 5: Тест Swift** `testHymenaeusClearThenUndoFull` в `HymenaeusTests.swift` через `Host`: мир с правками → `recycleAll([slot])`/корзина → зеркало пустое (все паузы, кольца дефолт) → `undoStep()` → зеркало равно исходному. `testClearDoesNotAddTwoUndoSteps` — два подряд CLEAR дают `undo.depth` +1 (второй не меняет снимок, шаг не добавляется).
- [ ] **Step 6:** commit `core: Hymenaeus clear wipes the whole world`.

### Task 4: Мир — Act, mix в звуке, зеркало, wasm

**Риск:** высокий — realtime-аудио, C-ABI, аллокации в блоке.

**Files:** Modify `core/worlds/hymenaeus/world.hpp/.cpp`, `sound.hpp/.cpp`, `core/build.sh` вывод `t2d.wasm`; Test `core/tests/hymenaeus_test.cpp`.

**Interfaces:**
- Produces: `enum Act { RING, CLOUD, PEARL, STRIKE, THREAD, DICE, CLEAR, RUN, BPM, TIMBRE, LOC, MIX }`; `MIRROR_N` пересчитан: `run, bpm, loc, mix, cursor×2, len×2, dejavu×2, lo×2, hi×2` + `2 × PEARL_MAX × (высота, пауза)` + `NR × (struct, bright, damp, lvl, poly, энергия)` + `NCK` + энергия облаков; порядок записать в заголовок `world.hpp`; `heard`: `[k, (кольцо, жемчужина)×k]` без изменений.

- [ ] **Step 1: Тесты:** `worldActRouting` (каждый Act меняет нужное в модели; `THREAD i=1 a=[2, 50]` ставит `lo` нити 1), `worldDiceBothOrOne` (`DICE a=[0]` меняет только нить 0), `worldMixSignal` (без облаков mix=0 ≈ чистая тишина облаков; mix=1 → сухое = 0; mix=0.5 — пик и RMS равны эталону, снятому с коммита до Task 1 на фиксированном seed и записанному константой), `worldMixNoNaN`, `worldNoAllocInProcess` (существующий приём теста), `worldMirrorSize` (`MIRROR_N == расчётное`), `worldLfoKeys` (`ACT_LFO` key 5 → кольцо 1, ключ 0; key 10 → облако POS).
- [ ] **Step 2:** FAIL.
- [ ] **Step 3:** Реализовать: циклы по `NR`; события нитей несут номер кольца = номер нити; в `sound.cpp` коэффициенты dry/wet перед сумматором; `MIX` пишет `model_.setMix`.
- [ ] **Step 4:** `core/build.sh` — PASS; `core/build.sh` пересобирает `t2d.wasm`.
- [ ] **Step 5:** commit `core: Hymenaeus acts, mix, mirror; rebuild t2d.wasm` (оба файла — требование guard-хука).

### Task 5: Swift — зеркало, геометрия, сверка Act

**Риск:** обычный — UI-данные под тестами сверки.

**Files:** Modify `ios/app/T2D/Hymenaeus/HymMirror.swift`, `HymGeo.swift`, `HymSheets.swift`; Test `ios/app/T2DTests/HymenaeusTests.swift`.

**Interfaces:**
- Consumes: `MIRROR_N`, порядок Task 4.
- Produces: `HymMirror.nr = 2`, `mirrorN`, поля `loc, mix, cursor[2], len[2], dejavu[2], lo[2], hi[2], pearls[2][16]`; `HymAct` дополнен `timbre, loc, mix`; `HymGeo`: `staveY(thread)`, `ringCenter(i)`, `ringR(i)`, `lensRect`, `mixTrack`, `sealRect(loc)`; `HymSheets.cloudLfoBase = 10`, `ringNK = 5`.

- [ ] **Step 1: Тесты:** сверка `ActNumbers` с `world.hpp` (существующая) расширена тремя новыми; `testMirrorLayout` (заполненный буфер → поля); `testGeoNoOverlap` (стан, кольца, линза, mix, печати не пересекаются на 390×844 и 320×568); `testLfoKeys` (кольцо `i·5+k`, облака `10+k`).
- [ ] **Step 2:** FAIL. **Step 3:** реализовать. **Step 4:** PASS (`xcodebuild test` схемой проекта, как в `docs/testing.md`).
- [ ] **Step 5:** commit `ios: Hymenaeus mirror and geometry for two rings`.

### Task 6: Swift — вид C, жесты, плашки, кнопки

**Риск:** обычный — UI; проверка глазами автора на iPhone.

**Files:** Modify `HymDraw.swift`, `HymTouch.swift`, `HymSheets.swift`, `HymenaeusView.swift`; Test `HymenaeusTests.swift`.

**Interfaces:**
- Consumes: Task 5.
- Produces: `HymTouch.hit(_ p: CGPoint) -> HymHit` с вариантами `.pearl(thread,i)`, `.clasp(thread)`, `.stave(thread)`, `.seal(loc)`, `.ring(i)`, `.cloud`, `.mix`, `.none`; в `HymenaeusView`: `dice()` (ноты), `timbre()` (тембр), `clear()`; плашка кольца: structure, brightness, damping, level, voces, двусторонний слайдер диапазона нити (подписи — ноты MIDI), длина нити; fx-веер: облака, mix.

- [ ] **Step 1: Тесты:** `testHitZones` (точки из `HymGeo` попадают в нужные `HymHit`), `testPitchGestureSendsAct` (вертикаль по жемчужине → `PEARL a=[нить,0,v]`), `testRangeSliderClamps` (ручки не сближаются меньше 5, границы 24…96), `testSealTapSendsLoc`, `testDiceButtons` (нотный кубик → `DICE`, тембровый → `TIMBRE`), `testClearButtonCallsClear`.
- [ ] **Step 2–4:** FAIL → реализовать вид по эскизу (палитра Paradisus, hairline, подписи только во время жеста, градиенты не каждый кадр) → PASS. Новый файл — `xcodegen`.
- [ ] **Step 5:** `shots` — сценарий скриншота мира 14; вердикт текстом, сверка с `mockups/hymenaeus-v2.html?v=C`.
- [ ] **Step 6:** commit `ios: Hymenaeus view C, gestures, plates`.

### Task 7: Доки

**Риск:** обычный — документация.

**Files:** Modify `docs/worlds/hymenaeus.md`, `docs/worlds/README.md`, `CLAUDE.md` (строка мира XIV), `docs/tokens.md`, спека (пометка про CLEAR).

- [ ] **Step 1:** обновить описание мира (два кольца, нити, локации, mix, кубики, CLEAR очищает всё), таблицу Act и ключей LFO, список известных ограничений; в спеке строка `CLEAR` исправлена на «очищает мир до конца».
- [ ] **Step 2:** строка в `docs/tokens.md` (`python3 tools/tokens.py --line`).
- [ ] **Step 3:** `node --check` не нужен (веб не менялся); commit `docs: Hymenaeus two rings`.

## Self-Review

- Покрытие спеки: ядро (T1–T4), вид и жесты (T5–T6), тесты (в задачах), документация (T7); дополнение автора про полную очистку (T3).
- Типы согласованы: `Thread`, `NLOC`, `setLoc/loc`, `mix()`, Act-коды и порядок зеркала определены в T1/T2/T4 и использованы в T5/T6.
- Открытое: что именно значит «очищает до конца» — принято: нити всех слотов, кольца, облака, mix в дефолт (это допущение, автор не уточнял).
