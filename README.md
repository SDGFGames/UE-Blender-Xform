# Blender Transform Shortcuts for UE5

A toggleable Unreal Engine 5 editor plugin that brings **Blender's modal G/S/R
transform workflow** into the level viewport — without breaking native UE controls.

Press `G`/`S`/`R`, move the mouse (or type an exact number), constrain to an axis
or plane, and confirm or cancel — exactly like Blender.

> Tested on **UE 5.7 (macOS)**. Editor-only plugin; no runtime/game dependency.

## Features

- **Move / Scale / Rotate** — `G` / `S` / `R` start an interactive modal op that follows the cursor.
- **Axis constraints** — `X` / `Y` / `Z` lock to a single axis.
- **Plane constraints** — `Shift+X` / `Shift+Y` / `Shift+Z` lock *out* that axis (transform in the other two).
- **Global / Local** — press the same axis key again to toggle Global → Local → off (Local uses the active object's axes).
- **Numeric entry** — type a value for an exact transform, e.g. `G X 5 Enter` moves +5 on X. Supports `.` and `-`.
- **Clear transforms** — `Alt+G` / `Alt+S` / `Alt+R` instantly reset location / scale / rotation (one undo step).
- **Duplicate & grab** — `Shift+D` duplicates the selection in place and immediately starts a Move (Blender-style). **Confirm** (Enter/LMB) keeps the copy; **cancel** (Esc/RMB) removes it and re-selects the originals. The whole duplicate+move is one undo step, and you can press `S`/`R`/`X`/`Y`/`Z`/numbers on the copy just like a normal grab.
- **Snapping (bound to UE)** — snap step sizes come from UE's viewport **Snap Settings** (translate / rotate / scale). When the matching grid-snap toggle is on, transforms snap automatically; hold **Ctrl** to force snapping on demand (Blender-style) even when the toggle is off.
- **Precision (`Shift`)** — hold **Shift** while dragging for fine, slowed control (default 0.1×).
- **Axis guide lines** — constraining to an axis draws Blender's red/green/blue guide line through the pivot.
- **Adjustable feel** — `MouseSensitivity` / `RotateSensitivity` tune drag magnitude. (Typed values always stay exact, regardless of snap/sensitivity.)
- **Confirm / Cancel** — **LMB or Enter** confirms; **RMB or Esc** cancels and restores (selection and gizmo snap back to the object).
- **One undo step** — the whole operation is a single `Ctrl/Cmd+Z`.
- **Viewport HUD** — a compact value tag rides next to the cursor (e.g. `X 5.00`, `Z 90°`, or `x2.000` while scaling), plus a key-hint bar along the viewport bottom listing the active shortcuts. Both show only during an op.
- **Non-destructive toggle** — a toolbar button, the **`Alt+Shift+B`** shortcut (rebindable), or the `bEnabled` setting turns it off; native UE shortcuts (`G` = Game View, `R` = Scale gizmo, …) come straight back. **The toolbar icon reflects the state** — Blender-orange when on, muted/slashed when off.

## Keymap

| Key | Action |
|---|---|
| `G` / `S` / `R` | Start Move / Scale / Rotate |
| `Alt`+`G` / `S` / `R` | Clear location / scale / rotation (instant) |
| `Shift`+`D` | Duplicate the selection + grab (cancel removes the copy) |
| `X` / `Y` / `Z` | Constrain to that axis (press again → Local → off) |
| `Shift`+`X`/`Y`/`Z` | Constrain to the plane perpendicular to that axis |
| `Ctrl` (hold) | Force snap on demand (uses UE's grid sizes) |
| `Shift` (hold) | Precision: fine, slowed dragging |
| `0-9` `.` `-` | Type an exact value |
| `Enter` / `LMB` | Confirm |
| `Esc` / `RMB` | Cancel (restores; keeps the selection) |
| `Alt`+`Shift`+`B` | Toggle the plugin on/off (rebindable) |

Sensitivity and precision live under *Editor Preferences → Plugins → Blender Transform Shortcuts* (**Feel**). **Snap step sizes are bound to UE's viewport Snap Settings** (the grid-snap dropdowns) — there's nothing to configure twice. Rebind the toggle under *Editor Preferences → Keyboard Shortcuts → Blender Transform*.

> **Note on `Shift`:** `Shift`+an axis *key* sets a plane constraint; `Shift` *held during the drag* is precision. They compose — e.g. `S Shift+Z` then keep holding Shift to fine-scale in the XY plane.

## Install

1. Copy (or symlink) the `BlenderXform/` folder into your project's `Plugins/` directory.
2. Open the project; the editor compiles the plugin on load (C++ project required).
3. The mode is on by default — toggle it from the **Blender XForm** toolbar button, or under
   *Editor Preferences → Plugins → Blender Transform Shortcuts*.

## Build (command line, macOS example)

```bash
"<UE_ENGINE>/Engine/Build/BatchFiles/Mac/Build.sh" \
  <YourGameModule>Editor Mac Development -Project="<path>/<YourProject>.uproject" -waitmutex
```

## How it works

A Slate `IInputProcessor` (registered first, index 0) intercepts the keys and drives a small modal
state machine. Pure transform math is unit-tested via UE automation tests; transforms apply to the
selection through `GEditor` inside one `FScopedTransaction`. See `docs/` for the full design spec and
implementation plan.

## License

MIT — see [LICENSE](LICENSE).
