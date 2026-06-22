# Blender-Style Transform Shortcuts for UE5 — Design Spec

- **Date:** 2026-06-21
- **Status:** Approved (brainstorming aligned)
- **Workspace:** `<repo>/`
- **Target engine:** UE 5.7 (Mac), installed at `…/<YourUEProject>/UE_5.7/`
- **Test host project:** `…/<YourUEProject>/<GameModule>/<GameModule>.uproject` — **test-only, plugin-only** (we touch nothing in that project except adding one entry under `Plugins/`, and it must be cleanly removable)

## 1. Goal & Acceptance Criteria

Give a UE artist Blender's modal transform muscle-memory inside the UE level viewport **without breaking native UE operation.**

A build is "done" when, in the real UE 5.7 editor:

1. **Blender-close experience**
   - With the mode ON and an actor selected, `G`/`S`/`R` start an interactive Move/Scale/Rotate that follows the mouse; **LMB or Enter** confirms, **RMB or Esc** cancels (restores).
   - `X`/`Y`/`Z` constrain to a single axis; pressing the same axis again toggles **Global → Local → off**.
   - `Shift+X`/`Shift+Y`/`Shift+Z` constrain to the plane perpendicular to that axis (the "any two directions" case); also Global/Local via re-press.
   - Numeric typing (`0-9 . -`, Backspace) sets an exact value, e.g. `G X 5 Enter` ⇒ +5cm on X; shown on a viewport HUD.
   - Whole operation is **one Undo step** (Ctrl+Z restores the pre-op state).
2. **Non-breaking UE**
   - The mode is a **toggle**. While OFF, UE behaves 100% normally (G = Game View, R = Scale gizmo, W/E/R gizmo modes, etc.).
   - While ON, we consume **only** the keys we own at the moment we own them (pass-through discipline); W/E gizmo keys are never touched.
   - Removal = delete the one `Plugins/` entry; no residue in the host project.

## 2. Scope

**In:** Move/Scale/Rotate; single-axis + plane constraints; Global + Local orientation (double-tap); numeric entry; viewport HUD; multi-select rigid transform around the selection median; UE undo integration; on/off toggle (toolbar button + optional user-bound hotkey).

**Out (deferred, documented):**
- Multi-select **individual origins** (per-object pivot/axes). v1 multi-select Local uses the **active** actor's basis around the shared median.
- Treating numeric input as meters (v1 uses UE-native cm / degrees / unitless scale factor).
- Snapping increments, proportional editing, vertex/component-level edits.

## 3. Architecture

**Approach A — Slate `IInputProcessor` + direct actor transforms via `GEditor` transactions.** A standalone **Editor**-type C++ plugin registers an input pre-processor on module startup (`FSlateApplication::Get().RegisterInputPreProcessor`). The pre-processor runs *before* normal Slate routing, so returning `true` from a key handler consumes the event and shadows UE's default (this is how G/R get intercepted). All transforms apply to selected actors through `GEditor` with `FScopedTransaction` for undo.

Rejected alternatives: a custom `UEdMode` (wants to be the sole active mode — conflicts with "always available when toggled"; heavier; harder raw-key timing) and subclassing `FEditorViewportClient` (invasive, version-fragile).

## 4. Components (single responsibility each)

1. **`FBlenderXformModule`** (`IModuleInterface`) — entry point. Startup: register input processor, toolbar/menu toggle command, settings. Shutdown: unregister all. Deps: Slate, SlateCore, LevelEditor, UnrealEd, ToolMenus, InputCore, Engine, Core, CoreUObject.
2. **`UBlenderXformSettings`** (`UDeveloperSettings`, EditorPerProjectUserSettings) — `bEnabled` (the toggle), `MouseSensitivity`, `RotateSensitivity`. Visible under Project/Editor Settings; persists the toggle.
3. **`FBlenderXformInputProcessor`** (`IInputProcessor`) — input gate. Acts only when: toggle ON **and** the hovered/focused widget is a level-editor viewport **and** there is a selection (for op start). Translates raw key/mouse events to `FBlenderXformOp` calls. Returns `true` only for events it owns.
4. **`FBlenderXformOp`** — modal state machine ("operator"). Holds: `EMode {None,Move,Scale,Rotate}`, `EAxis {Free,X,Y,Z}`, `bPlane`, `EOrient {Global,Local}`, numeric buffer, start-mouse, per-actor start-transform snapshot, pivot, active-actor basis. Methods: `Begin(mode)`, `CycleAxis(axis,bPlane)`, `PushDigit/Backspace`, `UpdateFromMouse(view,mousePos)`, `Commit`, `Cancel`. Opens the transaction on `Begin`, applies on every update, finalizes on `Commit`, cancels+restores on `Cancel`.
5. **`FBlenderXformMath`** — **pure functions, no engine-state side effects.** Given camera/view, pivot, start & current cursor, constraint basis vectors, and (optional) numeric value, returns the transform delta. Identical code for Global vs Local — the caller passes world basis (Global) or the active actor's rotated basis (Local). **TDD layer.**
6. **`FBlenderXformHUD`** — registers with `UDebugDrawService` (Editor) to draw the current state string on the viewport, e.g. `Move | X (local) | 5.0`.
7. **`FBlenderXformCommands` + toolbar/menu** (`TCommands`) — a toggle button reflecting `bEnabled`, plus a `UICommandInfo` the user can bind to a hotkey in Editor Preferences (default unbound, to avoid stealing more keys).

### Data flow
```
Slate input → InputProcessor (gate) → Op (state machine)
                                         → Math (pure: compute delta)   [unit-tested]
                                         → apply to selected actors (GEditor + FScopedTransaction)
                                         → RedrawLevelEditingViewports
              HUD ← reads Op state, draws overlay
Commit (LMB/Enter): finalize transaction.  Cancel (RMB/Esc): restore snapshot + cancel transaction.
```

## 5. Behavior Spec (state machine / key map)

**Idle** (toggle ON, selection present, cursor over a level viewport):
- `G`→`Begin(Move)`, `S`→`Begin(Scale)`, `R`→`Begin(Rotate)`. Snapshot all selected transforms; pivot = selection median; cache active actor's quat basis. **Consume** the key.
- Any other key passes through (UE behaves normally).

**Active op:**
- `X`/`Y`/`Z` → `CycleAxis`: 1st press = constrain Global axis; same axis again = Local axis; again = Free. `Shift+`axis = same but plane (lock that axis, free the other two).
- `0-9 . -` / Backspace → edit numeric buffer; when non-empty the **value** drives magnitude and the mouse is ignored for magnitude.
- Mouse move → when buffer empty, cursor drives magnitude.
- `G`/`S`/`R` mid-op → switch mode, keep selection/snapshot (Blender-style).
- `Enter`/`LMB` → `Commit`. `Esc`/`RMB` → `Cancel`.
- All keys above are **consumed** during an active op so UE doesn't double-handle them.

**Math (Global or Local via the basis passed in):**
- **Move:** deproject start & current cursor to world rays at the pivot's depth; world delta = (current − start) projected onto the constrained axis (dot) or plane (remove the locked-axis component); Free = full view-plane delta. Numeric `n` ⇒ delta = `n * axisDir` (single axis) — required so `G X 5` is exact.
- **Scale:** factor `f = dist(cursor,pivotScreen) / dist(startCursor,pivotScreen)`; apply on constrained axes only (others = 1). Numeric `n` ⇒ `f = n`. Actor scaled about pivot.
- **Rotate:** angle = signed screen-space sweep of cursor around the pivot; rotate about the constrained world/local axis (Free = camera-forward axis). Numeric `n` ⇒ angle = `n` degrees.

**Pivot / multi-select:** all selected actors transform rigidly about the shared median pivot (location offset, plus rotation/scale about pivot for R/S). Each actor `Modify()`'d inside the single transaction.

**Undo:** one `FScopedTransaction` per op; Cancel restores the snapshot then cancels.

## 6. Non-breaking-UE guarantees

- Toggle OFF ⇒ input processor early-returns `false` for everything; zero behavior change.
- Toggle ON but conditions unmet (no selection / not over a level viewport / no active op) ⇒ pass-through; only `G/S/R` are consumed, and only to *start* an op.
- W/E (gizmo translate/rotate) and other shortcuts untouched.
- No edits to host-project files beyond the symlinked `Plugins/BlenderXform`.

## 7. Testing strategy

1. **`FBlenderXformMath` (TDD, UE automation tests** à la `UEMatBridgeProbeTest.cpp`): synthetic camera + pivot + cursor → assert delta. Cases: `G X 5`⇒(5,0,0); `G Shift+Z`⇒delta.Z==0; `S 2` on free⇒uniform×2; `R Z 90`⇒yaw +90; Local axis with a rotated active actor⇒delta along the actor's basis.
2. **`FBlenderXformOp`** automation tests: feed synthetic key/mouse sequences against a mockable "apply" sink; assert resulting transform + that Cancel restores and Commit keeps.
3. **End-to-end real-machine (acceptance gate)** via MCP (`<ProjectMcpBridge>`) + computer-use: place a cube at a known transform, toggle ON, run `G X 5 Enter`, read the actor transform back, assert +5 X. Repeat S/R, plane, Local, Cancel, and a toggle-OFF regression (G still toggles Game View).

## 8. Deployment & hygiene

- Plugin source of truth lives in this workspace: `UE-Blender-Xform/BlenderXform/` (`.uplugin` + `Source/`).
- Deploy by **symlink**: `…/<GameModule>/Plugins/BlenderXform → …/UE-Blender-Xform/BlenderXform`. Single source of truth; clean removal = remove the symlink.
- Build (fast iteration) via `…/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh <GameModule>Editor Mac Development -Project=<uproject>`; or let the editor hot-compile on load.
- Git: commit only files under `UE-Blender-Xform/` (never `git add -A`; the repo holds unrelated untracked user work). Pushing is the user's job.

## 9. Open / deferred

- Multi-select individual origins (per-object pivot & axes).
- Numeric-as-meters option; snapping increments.
- Optional default hotkey for the toggle (v1 ships unbound + toolbar button).
