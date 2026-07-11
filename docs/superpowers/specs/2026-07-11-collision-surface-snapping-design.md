# Collision Surface Snapping Design

- **Date:** 2026-07-11
- **Status:** Approved
- **Engine:** Unreal Engine 5.7 editor plugin
- **Scope:** Add cursor-directed collision surface snapping to Blender-style Move operations.

## Goal

During `G` and the Move phase of `Shift+D`, let the user press `C` once to enable surface snapping and press `C` again to disable it. The session-level toggle persists across subsequent moves and commits/cancels, but resets when the plugin is disabled or the editor restarts.

## Interaction

- `C` toggles only during an active Move operation; idle `C` remains native UE input.
- Surface snapping is session-only and starts disabled.
- An enabled mode does not move the selection until the cursor actually moves after starting an operation.
- The viewport ray targets visible static `WorldStatic` collision. Complex collision is preferred, with simple collision fallback.
- A miss falls back to the existing Move preview.
- Selected actors move as one rigid group, keep relative transforms, rotation, and scale.
- Existing global/local axis and plane constraints remain authoritative.
- Existing grid snap runs before the final contact correction; contact wins when both cannot be satisfied exactly.
- Numeric input remains exact and bypasses surface correction.
- Collision-component bounds determine source support distance; actors without collision fall back to all primitive-component bounds.
- Contact keeps a fixed 0.1 cm safety gap.
- HUD: `[surface]` when armed/applied and `[surface:none]` after an attempted miss.

## Architecture

1. `FBlenderXformInputProcessor` owns the session toggle, detects first cursor movement, and deprojects the cursor ray.
2. `FXEditorSink` owns selected-actor filtering, cached aggregate bounds, and static-world trace filtering.
3. `FBlenderXformOp` receives a small hit-data value and asks a pure solver for the final Move delta.
4. `FBlenderXformSurfaceSnap` performs deterministic AABB support/contact math with no world/editor dependencies.
5. `FBlenderXformMath` exposes a reusable constrain-and-grid helper shared by ordinary and surface Move paths.

## Contact Solver

For start pivot `P`, start aggregate bounds `B`, surface point `H`, unit normal `N`, and gap `g`:

- Find the minimum projection of `B` on `N`.
- Compute pivot support distance `support = dot(P,N) - minProjection(B,N)`.
- Build the unconstrained surface target `H + N * (support + g) - P`.
- Constrain and grid-snap that target using the current constraint frame.
- Project the normal into the allowed movement subspace and apply the smallest correction that makes the translated bounds touch the plane at `H + N*g`.
- If the hit, bounds, normal, or allowed normal component is invalid, return the ordinary Move delta unchanged.

## Query Rules

- Object type: `ECC_WorldStatic`.
- Mobility filter: static.
- Ignore the complete current selection.
- Skip editor-hidden or temporarily hidden actors and invisible components, with a bounded retry loop.
- Orient the hit normal against the view ray.
- Trace complex first, then simple if no valid result is found.

## Lifecycle

- Commit/cancel/Shift+D cancel keep the surface mode toggle.
- Plugin disable and module shutdown reset it.
- Scale/Rotate do not apply or display surface snapping.
- Undo transaction and `PostEditMove` behavior remain unchanged.

## Verification

- Pure solver automation tests: free, axis, plane, local basis, diagonal normals, gap, grid precedence, invalid/fallback paths.
- Op tests: Move-only application, numeric bypass, HUD states, unchanged-apply suppression.
- Input-policy tests: Move-only plain-`C`, repeat handling, idle/Scale/Rotate behavior.
- Existing full `BlenderXform` automation suite must pass under `UnrealEditor-Cmd -NullRHI`.
- Build `G1_ProjectEditor Win64 Development` with UE 5.7.
