# Blender-Style Transform — Viewport HUD: Cursor Tag + Hint Bar — Design Spec

- **Date:** 2026-06-23
- **Status:** Approved (brainstorming aligned 2026-06-23)
- **Scope:** viewport HUD polish only — **no change to transform math or modal Op behavior**.
- **Builds on:** `2026-06-21-blender-transform-shortcuts-design.md` (the base plugin).

## 1. Goal

During an active `G`/`S`/`R`/`Shift+D` op, replace the single fixed top-left status line with:

1. a **compact value read-out that follows the cursor** (Blender's mouse read-out), and
2. a **key-hint bar pinned to the bottom** of the viewport.

So live feedback lands where the artist's eyes already are (the cursor / object), and the shortcuts become discoverable — without touching the tested math/Op core.

## 2. Current state

`FBlenderXformHUD::Draw` (`BlenderXformHUD.cpp`) draws every frame via `UDebugDrawService("Editor")`:

- the red/green/blue constraint guide lines through the pivot, and
- one status string (e.g. `Move | X (local) | 5.00 [snap]`) at a **fixed `(28,64)`**, large orange font + black shadow.

The string is supplied by the module's `GetText` lambda = `Op.Hud()` (empty when no op → nothing draws; `BlenderXformModule.cpp:92`). There are **no key hints**, and the HUD does not consult the cursor position today.

## 3. Design

### 3a. Cursor value tag (live)

- Follows the mouse with a small fixed offset (~`+18,+8` px); **clamped to stay fully inside the viewport rect**; when the cursor is outside the viewport, fall back to a fixed corner.
- **Content (compact — minimal occlusion):** axis + value + currently-active badges. Examples: `X 5.00`, rotate `Z 90°`, free `5.00`; while typing, the buffer with a caret `X 5.|`; append a small `[snap]` / `[fine]` when those are live. Mode (Move/Scale/Rotate) is **not** repeated in the tag (the user just pressed it; the hint bar frames it).
  - **Decided:** compact tag. (Alternative considered: relocate the full `Move ▸ X(local) ▸ 5.00` string to the cursor — rejected as too much occlusion; trivially switchable later if wanted.)
- **Visual:** orange text (current color) + black shadow on a small semi-transparent dark panel for legibility.

### 3b. Bottom key-hint bar

- A horizontal semi-transparent dark strip pinned a few px above the viewport bottom (anchored to `Canvas->SizeY`), small white text.
- **v1 content is STATIC**, one line covering the common keys:
  `X/Y/Z axis · Shift+axis plane · Shift fine · Ctrl snap · 0-9 . - type · Enter/LMB confirm · Esc/RMB cancel`
- Shown **only during an active op** (gated the same way as the tag).
- **Deferred (out of scope):** context-aware hints that change with mode/state (Blender-style). Noted for a later pass; v1 stays static (YAGNI).

### 3c. Wiring / where the code changes land

- **`FBlenderXformOp`** — add a pure, unit-tested `CursorTag()` formatter (compact string from Op state: axis, value, numeric buffer/caret, live `[snap]`/`[fine]`). This becomes the HUD's text source.
- **`BlenderXformModule.cpp`** — point the existing `GetText` lambda at `CursorTag()` instead of `Hud()` (one line). (`Hud()`/`HudString()` then go unused by the HUD; fold or leave — settled in implementation, no public-API need.)
- **`BlenderXformHUD.cpp`** — (1) draw the text at the cursor with a panel + clamp instead of fixed `(28,64)`; (2) draw the static hint bar at the bottom; (3) read the cursor pixel itself from `GCurrentLevelEditingViewportClient->Viewport->GetMousePos()` (the same source the input processor uses) — so **no new callback plumbing**. Gate both on the tag string being non-empty (= op active).
- **Axis guide lines:** unchanged.

## 4. Testing

- **Unit:** `CursorTag()` formatting (pure) — e.g. Move + axis X + value 5 → `X 5.00`; typing buffer → caret; snap/fine badges present when active. Lives alongside the existing Op tests.
- **Visual / on-machine:** tag follows and clamps at viewport edges; hint bar legible at bottom; no regression to the axis lines or the transform itself. (Canvas drawing isn't unit-testable; verify in the editor.)

## 5. Risk

Low. Confined to HUD draw + one Op formatter + a one-line module wiring change. No change to transform math or modal behavior → **zero functional regression risk**; worst case is mispositioned/hard-to-read text, caught visually.

## 6. Files touched

- `Source/BlenderXform/Private/BlenderXformOp.h` / `.cpp` — `CursorTag()`.
- `Source/BlenderXform/Private/BlenderXformHUD.cpp` — cursor-follow tag + panel + bottom hint bar + read cursor.
- `Source/BlenderXform/Private/BlenderXformModule.cpp` — wire `GetText` → `CursorTag()`.
- `Source/BlenderXform/Private/Tests/BlenderXformOpTest.cpp` — `CursorTag()` test.
- `README.md` — refresh the HUD bullet (cursor tag + hint bar).

## 7. Hygiene

- Source of truth in this workspace; deploy via the existing single `Plugins/BlenderXform` symlink (test-only, cleanly removable).
- Git: commit only files under `UE-Blender-Xform/` with explicit narrow paths (never `git add -A`). **Pushing is the user's job — and per the user, this design doc is not to be pushed.**
