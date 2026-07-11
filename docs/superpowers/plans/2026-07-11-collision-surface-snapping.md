# Collision Surface Snapping Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add persistent `C`-toggle, cursor-directed static collision surface snapping to Move operations.

**Architecture:** Keep world queries in the editor boundary, contact math in a pure solver, and final movement in the existing snapshot-based Op/Sink pipeline. Cache source bounds once per operation and preserve existing undo/selection semantics.

**Tech Stack:** Unreal Engine 5.7 C++, Slate input preprocessing, editor collision queries, UE Automation Tests.

## Global Constraints

- `C` toggles only during active Move and persists for the editor session.
- Static `WorldStatic` targets only; complex trace with simple fallback.
- Group rigid movement; collision bounds with visual-bounds fallback.
- No rotation alignment, dynamic targets, exact shape sweeps, toolbar UI, saved setting, or configurable gap.
- Numeric input remains exact.

---

### Task 1: Pure contact solver

**Files:**
- Create: `BlenderXform/Source/BlenderXform/Private/BlenderXformSurfaceSnap.h`
- Create: `BlenderXform/Source/BlenderXform/Private/BlenderXformSurfaceSnap.cpp`
- Create: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformSurfaceSnapTest.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Public/BlenderXformMath.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformMath.cpp`

- [ ] Write solver tests for free/axis/plane/grid/fallback cases.
- [ ] Build and confirm tests fail because the solver API is missing.
- [ ] Extract reusable constrain-and-grid Move helper.
- [ ] Implement minimum-projection support and allowed-subspace contact correction.
- [ ] Build and run Math/SurfaceSnap tests.

### Task 2: Op integration and HUD state

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformOp.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformOp.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformOpTest.cpp`

- [ ] Add failing tests for applied hit, miss HUD, numeric bypass, and non-Move behavior.
- [ ] Extend the sink test double with cached surface bounds.
- [ ] Pass small surface-state data through `UpdateFromScreen` and invoke the solver only for mouse Move.
- [ ] Add `[surface]` / `[surface:none]` formatting.
- [ ] Build and run Op tests.

### Task 3: Selection bounds and static-world tracing

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformEditorSink.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformEditorSink.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformEditorSinkTest.cpp`

- [ ] Add bounds-selection helper tests where practical.
- [ ] Cache aggregate collision bounds with per-actor visual fallback during selection capture.
- [ ] Implement complex-then-simple static trace with selection/hidden filtering and normal orientation.
- [ ] Expose cached bounds through `IXApplySink`.
- [ ] Build and run EditorSink tests.

### Task 4: Persistent input toggle

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformInputProcessorTest.cpp`

- [ ] Add failing policy tests for plain `C`, Move-only, idle, and repeat behavior.
- [ ] Add session toggle and first-real-mouse-move state.
- [ ] Deproject a cursor ray, request a static surface trace, and pass state/hit to Op.
- [ ] Keep ordinary Move fallback on miss and reset state when the plugin is disabled.
- [ ] Build and run Input tests.

### Task 5: Documentation and verification

**Files:**
- Modify: `README.md`

- [ ] Document persistent `C` surface snapping and its interaction with constraints/grid/numeric input.
- [ ] Build `G1_ProjectEditor Win64 Development`.
- [ ] Run all `BlenderXform` automation tests with `-EnablePlugins=BlenderXform -NullRHI`.
- [ ] Inspect git diff and verify scope against the approved design.
