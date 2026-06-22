# Phase 18: Performance Validation

Goal: Measure whether the architecture supports large-scale AI.

Harness is code-complete (CVars + console commands); the measurements themselves are a PIE pass
(PENDING USER). See `Docs/Guides/Phase15-19_Editor_Work_And_Verification.md` §18.

- [ ] Test 100 soldiers — `Ash.Perf.SpawnSoldiers 100` (PENDING USER measurement)
- [ ] Test 300 soldiers — `Ash.Perf.SpawnSoldiers 300` (PENDING USER measurement)
- [ ] Test 500 soldiers — `Ash.Perf.SpawnSoldiers 500` (PENDING USER measurement)
- [ ] Test 1000 soldiers — `Ash.Perf.SpawnSoldiers 1000` (PENDING USER measurement)
- [ ] Measure FPS — `stat unit` / `Ash.Perf.Report` (PENDING USER)
- [ ] Measure Game Thread cost — `stat unit` (Game ms) (PENDING USER)
- [ ] Measure Mass processor cost — `stat mass` / `stat MassProcessor` (PENDING USER)
- [ ] Measure active Actor proxy count — `Ash.Perf.Report` (ActiveProxies) (PENDING USER)
- [x] Compare AI LOD on/off — toggle `Ash.Mass.LOD 0/1` (wired into the LOD processor)
- [x] Compare time slicing on/off — toggle `Ash.Mass.TimeSlice 0/1` (wired into the LOD processor)
- [ ] Document bottlenecks — fill in the DONE doc after the measurement pass (PENDING USER)
- [x] Create `Done/DONE_performance_validation.md` (results table pre-staged; numbers PENDING USER)