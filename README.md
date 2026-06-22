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
- **Snapping (`Ctrl`)** — hold **Ctrl** while dragging to snap: translate to a grid (default 10 uu), rotate to 5°, scale to 0.1 — exactly like Blender. Increments are configurable.
- **Precision (`Shift`)** — hold **Shift** while dragging for fine, slowed control (default 0.1×).
- **Adjustable feel** — `MouseSensitivity` / `RotateSensitivity` tune drag magnitude. (Typed values always stay exact, regardless of snap/sensitivity.)
- **Confirm / Cancel** — **LMB or Enter** confirms; **RMB or Esc** cancels and restores.
- **One undo step** — the whole operation is a single `Ctrl/Cmd+Z`.
- **Viewport HUD** — shows the live state, e.g. `Move | X (local) | 5.00 [snap]`.
- **Non-destructive toggle** — a toolbar button (and `bEnabled` setting) turns it off; native UE shortcuts (`G` = Game View, `R` = Scale gizmo, …) come straight back. **The toolbar icon reflects the state** — Blender-orange when on, muted/slashed when off.

## Keymap

| Key | Action |
|---|---|
| `G` / `S` / `R` | Start Move / Scale / Rotate |
| `X` / `Y` / `Z` | Constrain to that axis (press again → Local → off) |
| `Shift`+`X`/`Y`/`Z` | Constrain to the plane perpendicular to that axis |
| `Ctrl` (hold) | Snap to increments while dragging (grid / 5° / 0.1) |
| `Shift` (hold) | Precision: fine, slowed dragging |
| `0-9` `.` `-` | Type an exact value |
| `Enter` / `LMB` | Confirm |
| `Esc` / `RMB` | Cancel (restores; keeps the selection) |

Snap increments and sensitivity live under *Editor Preferences → Plugins → Blender Transform Shortcuts* (categories **Feel** and **Snapping**).

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
