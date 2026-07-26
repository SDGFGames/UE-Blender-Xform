# Surface Normal-Align Snap Design

- **Date:** 2026-07-26
- **Status:** Implemented (Tasks A-D committed `f5d1ae4`..`00b9332`); automation gate passed (41 succeeded / 0 failed / 0 not run). Manual acceptance pending.
- **Engine:** Unreal Engine 5.7 editor plugin
- **Scope:** Add optional surface-normal alignment to the existing collision surface snap (`C`).
- **Relation to prior work:** Extends `2026-07-11-collision-surface-snapping-design.md`. Deferred-Backlog item "Surface Snap 自动旋转对齐法线"; pulled in by owner decision on 2026-07-26.

## Goal

During `G` and the Move phase of `Shift+D`, let the user press `Shift+C` to toggle normal-alignment on top of contact surface snapping. When armed and the cursor is over a static surface, the selection rotates rigidly so its **world Z (Up)** axis aligns with the surface normal at the hit point, while the existing contact placement still applies. The toggle is session-level and persists across moves/commits/cancels, like contact.

## Non-goals

- Per-actor normal tracking (each actor to its own surface). Stays rigid: one cursor trace, one rotation.
- Normal-align during Scale/Rotate. Move-only (contact is Move-only today).
- Aligning axes other than world Z (no Local / constrained-axis align this iteration).
- Coexisting with axis/plane move constraints. Align is suppressed when a constraint is set; contact still works.
- New plane numeric direction semantics.

## Keymap

| Key | Action |
|---|---|
| `C` short press | Toggle contact snap (unchanged): press + quick release with no key-repeat in between. |
| `C` hold | Activate normal-align (Move-only). While held, `bAlignHeld=true`; on release, align turns off. Contact is NOT toggled by a hold. |
| `Shift` (held during drag) | Precision (unchanged) - independent of the `C` hold. |

`C` does double duty, distinguished by **key-repeat**, not a timer: a quick tap produces no repeat events (contact toggle); a held press produces repeat events (align). This avoids the failure mode where a long press meant to align is misread as a short press and accidentally toggles contact off. UE key-repeat fires after the OS auto-repeat delay (~0.5s), so a deliberate hold reliably enters align mode.

- A short tap never activates align (no repeat => no hold).
- A hold never toggles contact (the press enters a candidate state; only a release with no prior repeat toggles contact).
- Align requires contact semantics, so while align is held the contact trace runs as usual; contact need not be separately armed for align to place the object (the align path issues its own trace).

During an active Move op the plugin already swallows unhandled keys, so there is no UE conflict.

## Interaction

1. Move op active, **Free** (no axis/plane), `C` held, cursor over a visible static surface:
   - The existing trace yields `Hit.Point` + `Hit.Normal`.
   - The existing `SolveMoveDelta` places the start bounds on the surface (translation only).
   - **New:** compute a rotation `Q` that maps world Up `(0,0,1)` to `Hit.Normal`, applied about pivot `Hit.Point`, rigidly to the whole selection.
2. An axis/plane constraint is set: align is suppressed (N3). Contact still applies as today; rotation unchanged.
3. Cursor misses the surface: rotation restores to the snapshot (N8); contact returns to ordinary Move (existing).
4. Drag (C still held): rotation follows the live normal each update (N7). Ctrl snap quantizes the align angle to `RotateSnapDeg` to reduce jitter on noisy collision.
5. `C` released: align turns off; the selection's rotation restores to the snapshot. Contact state is unchanged by the release (a hold never toggled it).
6. Confirm: rotation kept - one undo step with the move. Cancel: snapshot restored (rotation + position), no undo item.
7. Numeric Move bypasses align (as it bypasses contact today).

Align is a transient hold (not session-persistent like contact): it is only active while `C` is held during a Move op. This matches the precision (`Shift`) and force-snap (`Ctrl`) hold-modifiers.

## Math

Pure function `FBlenderXformMath::AlignUpToNormalQuat(const FVector& N)`:

- Let `Up = (0,0,1)`, `n = N.GetSafeNormal()`.
- `d = dot(Up, n)`.
- `d > 1 - 1e-6`: identity (already aligned).
- `d < -1 + 1e-6`: 180° about world X `(1,0,0)` (deterministic; see Known Limitations).
- Else: `axis = normalize(Up × n)`, `angle = acos(d)`. Return `FQuat(axis, angle)`.

Apply about pivot `P = Hit.Point` to each snapshot transform `T`:
- `T.Location = P + Q * (T.Location - P)`
- `T.Rotation = (Q * T.Rotation).GetNormalized()`

This mirrors the existing Rotate Apply path (`EditorSink.cpp` Rotate case), reused verbatim.

## FXApplied extension

`FXApplied` (BlenderXformOp.h) currently carries `MoveDelta` for Move. Add:
- `FQuat AlignQuat = FQuat::Identity;`
- `FVector AlignPivot = FVector::ZeroVector;`
- `bool bAlignApplied = false;`

The Move branch of `Recompute` sets these when contact+align+Free+hit are all true; the EditorSink `Apply` Move case composes the rotation about `AlignPivot` when `bAlignApplied`. Move and Rotate channels stay separate (Rotate still owns `RotAxis`/`RotDeg`); Move just optionally also carries an align rotation.

`AppliedNearlyEqual` (Op test helper) extended to compare the new fields.

## Edge cases

- **Wall (near-horizontal normal):** Z aligns to horizontal - the object tips onto the wall. Expected.
- **Normal flips between frames (concave/convex):** live rotation follows; Ctrl snap quantizes to reduce jitter.
- **Near -Up (ceiling, dot ≈ -1):** 180° about world X. Deterministic but the "roll" axis is arbitrary; acceptable for v1 (see Known Limitations).
- **Multi-actor selection:** rigid rotation about `Hit.Point` preserves relative offsets. The pivot is the cursor hit, not the median - large selections rotate around the cursor hit (Blender-like).
- **Surface gap (0.1 cm):** applied along the normal after rotation (existing `SurfaceGap` logic, unchanged).
- **Align on, contact off (user toggles contact off while align on):** both clear - align requires a surface.

## HUD

- `[align]` badge while `C` is held and align is applied (hit valid, Free).
- `[align:none]` while `C` is held, a trace was attempted, and it missed.
- `[align:locked]` while `C` is held but suppressed by an axis/plane constraint (so the user sees why it isn't rotating).

Badges appear/disappear with the hold; none show when `C` is not held.

## Testing (TDD)

Pure math (unit-tested, `BlenderXformMathTest.cpp`):
- `AlignUpToNormalQuat(Up)` = identity.
- `AlignUpToNormalQuat(-Up)` = 180° about X.
- `AlignUpToNormalQuat(n)` maps Up to n (within 1e-3) for several n (floor, wall, 45° slope).
- Near-Up (dot = 1 - 1e-7) stays identity (no jitter).

Policy (unit-tested, `BlenderXformInputProcessorTest.cpp`):
- `C` key-repeat (hold) sets align-held only in Move; idle/Scale/Rotate `C` = native UE.
- `C` short press (no repeat) toggles contact; `C` hold (repeat) does NOT toggle contact.
- `C` release clears align-held; a release after a hold does not toggle contact.
- Align is suppressed when an axis/plane constraint is set (align-held stays but applies nothing).

Op (unit-tested, `BlenderXformOpTest.cpp` with fake sink):
- Move + Free + align + valid hit: `bAlignApplied` true, `AlignQuat` maps Up to hit normal.
- Move + axis + align + valid hit: `bAlignApplied` false (suppressed).
- Move + align + miss: `bAlignApplied` false, rotation unchanged.
- Numeric Move + align: align bypassed.

Integration (manual gate):
- Drag over floor -> wall -> sphere: object follows normals live.
- Cancel restores rotation; Undo is one step.
- `[align]` / `[align:none]` / `[align:locked]` badges appear correctly.

## Architecture

1. `FBlenderXformInputProcessor` owns the transient `bAlignHeld` flag (set on `C` key-repeat, cleared on `C` key-up) and the short-press-vs-hold discrimination for contact toggling.
2. `FBlenderXformOp` receives the align-held state + hit, computes `AlignQuat` in the Move branch, and carries it in `FXApplied`.
3. `FBlenderXformMath::AlignUpToNormalQuat` is the pure, unit-tested solver.
4. `FXEditorSink::Apply` composes the align rotation about `AlignPivot` in the Move case (reusing the Rotate path's pivot math).
5. The existing `TraceStaticSurface` (which already returns `Hit.Normal`) is reused unchanged.

## Known Limitations (v1)

- Ceiling case (dot ≈ -1) rotates 180° about world X; the roll axis is arbitrary and may not match user expectation. A future version could pick the active actor's forward as the flip axis.
- Align is world-Z only; Local/constrained-axis align is deferred.
- Rigid only; per-actor normal tracking is deferred.

## Implementation Plan (separate, after spec approval)

- **Task A:** `AlignUpToNormalQuat` math + unit tests.
- **Task B:** Extend `FXApplied` with align fields; `EditorSink::Apply` Move case composes the rotation; extend `AppliedNearlyEqual`.
- **Task C:** `C` hold policy + transient `bAlignHeld` + short-press-vs-hold contact discrimination; Op Move branch sets align when Free+align-held+hit.
- **Task D:** HUD badges `[align]` / `[align:none]` / `[align:locked]`; miss restores rotation.
- **Task E:** Build + automation gate + manual acceptance.

Each task: failing test -> minimal impl -> run `BlenderXform` suite -> commit (single-layer subagent per repo convention). Does not touch Iteration 3-4 scope.
