# Blender Transform Shortcuts — Implementation Plan

> **For agentic workers:** Execute task-by-task. Steps use `- [ ]` tracking. TDD the pure layers; compile/e2e gate the rest.

**Goal:** A toggleable UE5.7 editor plugin giving Blender G/S/R modal transforms (single-axis/plane, Global/Local double-tap, numeric entry) without breaking native UE.

**Architecture:** Editor C++ plugin registers a Slate `IInputProcessor` that gates on (toggle ON + level viewport focused + selection), drives a modal state machine `FBlenderXformOp`, which calls pure `FBlenderXformMath` and applies to selected actors via `GEditor` + `FScopedTransaction`. HUD via `UDebugDrawService`. Toggle via toolbar + `UBlenderXformSettings`.

**Tech Stack:** UE 5.7, Slate/SlateCore, UnrealEd, LevelEditor, ToolMenus, automation tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`).

## Global Constraints

- Engine: **UE 5.7**, Mac. Build via `…/<YourUEProject>/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh`.
- Source of truth: `UE-Blender-Xform/BlenderXform/`. Deploy to host project **by symlink only**: `…/<GameModule>/Plugins/BlenderXform`. **Touch nothing else** in `<YourUEProject>`. Cleanly removable.
- Git: stage only paths under `UE-Blender-Xform/`. **Never `git add -A`.** No push (user pushes).
- Non-breaking: toggle OFF ⇒ processor returns false for all input. Consume keys only when we own them. Never touch W/E.
- Units: cm (move), degrees (rotate), factor (scale). Orientation: Global + Local (double-tap). Pivot: selection median. One Undo step per op.

---

### Task 0: Plugin scaffold that compiles and loads

**Files:** Create `BlenderXform.uplugin`, `Source/BlenderXform/BlenderXform.Build.cs`, `Public/BlenderXformTypes.h`, `Private/BlenderXformModule.cpp`.

**Interfaces — Produces:** module `FBlenderXformModule : IModuleInterface`; enums in `BlenderXformTypes.h`:
```cpp
enum class EXMode : uint8 { None, Move, Scale, Rotate };
enum class EXAxis : uint8 { Free, X, Y, Z };
enum class EXOrient : uint8 { Global, Local };
struct FXConstraint { EXAxis Axis=EXAxis::Free; bool bPlane=false; EXOrient Orient=EXOrient::Global; FVector BX=FVector::ForwardVector, BY=FVector::RightVector, BZ=FVector::UpVector; };
```

- [ ] **S1** `.uplugin`: FriendlyName "Blender Transform Shortcuts", one Editor module `BlenderXform`, LoadingPhase Default, EnabledByDefault true.
- [ ] **S2** `Build.cs` PublicDependencyModuleNames: Core, CoreUObject, Engine, Slate, SlateCore, InputCore, UnrealEd, LevelEditor, ToolMenus, DeveloperSettings, EditorFramework.
- [ ] **S3** Module `StartupModule`/`ShutdownModule` log `UE_LOG(LogTemp, Log, TEXT("[BlenderXform] startup"))` only (wire registration in later tasks).
- [ ] **S4** Build green (Task 7 command) and confirm log on editor load.
- [ ] **S5** Commit `git add UE-Blender-Xform/BlenderXform && git commit -m "feat(blender-xform): plugin scaffold"`.

---

### Task 1: Pure math `FBlenderXformMath` (TDD)

**Files:** Create `Public/BlenderXformMath.h`, `Private/BlenderXformMath.cpp`, `Private/Tests/BlenderXformMathTest.cpp`.

**Interfaces — Produces (all static, pure, no editor state):**
```cpp
FVector  ConstrainVector(const FVector& V, const FXConstraint& C);                 // project onto axis / remove locked axis / free
FVector  MoveDelta (const FVector& worldStart, const FVector& worldNow, const FXConstraint& C, bool bNum, double Num);
FVector  ScaleFactors(const FVector2D& pivotS, const FVector2D& startS, const FVector2D& nowS, const FXConstraint& C, bool bNum, double Num); // per-axis multipliers
double   RotateAngleDeg(const FVector2D& pivotS, const FVector2D& startS, const FVector2D& nowS, const FVector& axisWorld, const FVector& camFwd, bool bNum, double Num);
```
Rule: `ConstrainVector` — Free→V; single axis A→ (V·BA)·BA; plane locking A→ V − (V·BA)·BA. `MoveDelta` numeric→ Num·(single-axis dir) (plane numeric falls back to constrained mouse). `ScaleFactors` from screen-dist ratio `|nowS−pivotS|/|startS−pivotS|` on constrained axes, 1 elsewhere; numeric→Num on constrained axes. `RotateAngleDeg` = signed angle between (startS−pivotS) and (nowS−pivotS), sign by `sign(axisWorld·camFwd)`; numeric→Num.

- [ ] **S1 Write failing tests** (`BlenderXformMathTest.cpp`), cases:
  - ConstrainVector: Free→V; X→(V.X,0,0); planeZ(bPlane,Axis=Z)→(V.X,V.Y,0); Local X with BX=(0,1,0)→ projects onto (0,1,0).
  - MoveDelta numeric X 5 → (5,0,0). MoveDelta(start=(0,0,0),now=(3,4,5),X)→(3,0,0).
  - ScaleFactors numeric free 2 → (2,2,2); numeric X 2 → (2,1,1).
  - RotateAngleDeg numeric 90 → ~90; geometric quarter-turn CCW about +Z facing −Z cam → ~±90 (assert magnitude 90, sign deterministic).
- [ ] **S2** Build + run automation (Task 7 + Task 1 test cmd) → FAIL (unimplemented).
- [ ] **S3** Implement `BlenderXformMath.cpp` per rules above.
- [ ] **S4** Run automation → PASS.
- [ ] **S5** Commit `…math + tests`.

**Run tests:** `…/Build.sh` then `UnrealEditor-Cmd <uproject> -ExecCmds="Automation RunTests BlenderXform.Math; Quit" -unattended -nop4 -nosplash -log` and grep for `Test Completed. Result={Success}`.

---

### Task 2: Modal state machine `FBlenderXformOp` (TDD via mock sink)

**Files:** Create `Private/BlenderXformOp.h/.cpp`, `Private/Tests/BlenderXformOpTest.cpp`.

**Interfaces — Consumes:** `FBlenderXformMath`, `FXConstraint`. **Produces:**
```cpp
struct FXApplied { EXMode Mode; FVector MoveDelta; FVector ScaleFac; double RotDeg; FVector RotAxis; bool bCommitted; bool bCancelled; };
class IXApplySink { public: virtual void Apply(const FXApplied&)=0; virtual void Commit()=0; virtual void Cancel()=0; virtual FVector Pivot() const=0; virtual void ActiveBasis(FVector&X,FVector&Y,FVector&Z) const=0; };
class FBlenderXformOp {
  void Begin(EXMode, IXApplySink&);          // snapshot via sink
  void CycleAxis(EXAxis, bool bShiftPlane);  // Global→Local→Free
  void PushDigit(TCHAR); void Backspace();    // numeric buffer ("-", ".", digits)
  void UpdateFromScreen(const FVector& wStart,const FVector& wNow,const FVector2D& pS,const FVector2D& sS,const FVector2D& nS,const FVector& camFwd);
  void Commit(); void Cancel();
  bool IsActive() const; FString HudString() const;  // e.g. "Move | X (local) | 5"
};
```
`CycleAxis(A)` on same A: Global→Local→Free (cleared). Different A: set Global A. `bShiftPlane` sets plane. On Local, basis comes from `sink.ActiveBasis`; Global uses world basis.

- [ ] **S1 Write failing tests** with a `FFakeSink` recording `FXApplied`:
  - Begin(Move)+CycleAxis(X)+PushDigit('5')+UpdateFromScreen(any)+Commit → last Apply MoveDelta==(5,0,0), bCommitted via Commit() called.
  - Begin(Move)+Cancel → sink.Cancel() called, no commit.
  - CycleAxis(X)×3 → constraint Free again (assert via HudString lacking axis).
  - Begin(Scale)+PushDigit('2')+Commit → ScaleFac==(2,2,2).
  - Begin(Rotate)+CycleAxis(Z)+PushDigit('9')+PushDigit('0')+Commit → RotDeg==90.
- [ ] **S2** Build+run → FAIL.
- [ ] **S3** Implement Op (numeric buffer parse, constraint cycle, call Math, push `FXApplied` to sink each update/commit).
- [ ] **S4** Run → PASS.
- [ ] **S5** Commit.

---

### Task 3: Production apply sink (`FXEditorSink`) — GEditor + transaction + deprojection

**Files:** add to `BlenderXformOp.cpp` (or `Private/BlenderXformEditorSink.{h,cpp}`).

**Interfaces — Consumes:** `IXApplySink`. **Produces:** `FXEditorSink : IXApplySink` operating on `GEditor->GetSelectedActors()`.
- `Apply`: open `FScopedTransaction` lazily on first apply; for each selected actor `Actor->Modify()`, set `Transform = f(Snapshot, delta, pivot)` (move: +MoveDelta; scale/rotate: about Pivot); `GEditor->RedrawLevelEditingViewports()`.
- `Pivot()`: average of selected actor locations. `ActiveBasis()`: `GEditor->GetSelectedActors()` active actor quat → Forward/Right/Up.
- `Commit`: end transaction (keep). `Cancel`: restore snapshots then `transaction.Cancel()`.

- [ ] **S1** Implement sink. (E2E-verified, not unit; relies on GEditor.)
- [ ] **S2** Build green.
- [ ] **S3** Commit.

---

### Task 4: Input processor `FBlenderXformInputProcessor` (gate + key map)

**Files:** Create `Private/BlenderXformInputProcessor.{h,cpp}`; register in module Startup.

**Interfaces — Consumes:** `FBlenderXformOp`, `FXEditorSink`, `UBlenderXformSettings`. `IInputProcessor` overrides: `HandleKeyDownEvent`, `HandleMouseMoveEvent`, `HandleMouseButtonDownEvent`, `Tick`.
- Gate `ShouldHandle()`: `Settings->bEnabled` && level viewport is the active/hovered (`GCurrentLevelEditingViewportClient` valid & viewport hovered) && `GEditor->GetSelectedActors()->Num()>0`.
- Idle: G/S/R → `Op.Begin(...)` consume; else false.
- Active: X/Y/Z (+Shift)→CycleAxis; digits/.- /BackSpace→numeric; Enter/LMB→Commit; Esc/RMB→Cancel; G/S/R→switch mode; mouse move→compute screen inputs (deproject via `GCurrentLevelEditingViewportClient`) → `Op.UpdateFromScreen`. Consume handled keys.

- [ ] **S1** Implement; register/unregister `FSlateApplication::Get().RegisterInputPreProcessor(...)` in module Startup/Shutdown.
- [ ] **S2** Build green.
- [ ] **S3** Commit.

---

### Task 5: HUD `FBlenderXformHUD` (UDebugDrawService)

**Files:** Create `Private/BlenderXformHUD.{h,cpp}`.
- Register `UDebugDrawService::Register(TEXT("Editor"), FDebugDrawDelegate::CreateRaw(...))`; in draw, if `Op.IsActive()` draw `Op.HudString()` top-left via `Canvas->DrawText`.

- [ ] **S1** Implement + register in Startup, deregister in Shutdown. **S2** Build green. **S3** Commit.

---

### Task 6: Settings + toggle command + toolbar

**Files:** Create `Public/BlenderXformSettings.h`, `Private/BlenderXformSettings.cpp`, `Private/BlenderXformCommands.{h,cpp}`; wire in module Startup.
- `UBlenderXformSettings : UDeveloperSettings` (EditorPerProjectUserSettings): `UPROPERTY(EditAnywhere,config) bool bEnabled=true; float MouseSensitivity=1.f; float RotateSensitivity=1.f;`
- `FBlenderXformCommands : TCommands` with `ToggleMode` (default chord none). Register `UToolMenus` level-editor toolbar button toggling `bEnabled`, label reflects state.

- [ ] **S1** Implement. **S2** Build green. **S3** Commit.

---

### Task 7: Build & compile-green (Mac)

**Build command:**
```bash
UE=<UEPROJECT>/UE_5.7
UPROJ=<UEPROJECT>/<GameModule>/<GameModule>.uproject
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" <GameModule>Editor Mac Development -Project="$UPROJ" -waitmutex
```
- [ ] Build green after each task; fix errors before moving on.

---

### Task 8: Deploy (symlink) + real-machine e2e verification (acceptance gate)

- [ ] **S1 Symlink:** `ln -s <repo>/BlenderXform "<UEPROJECT>/<GameModule>/Plugins/BlenderXform"` (only if absent).
- [ ] **S2** Launch editor (or attach via `<ProjectMcpBridge>`); place a cube at known transform.
- [ ] **S3 Verify via MCP + computer-use** each acceptance case from spec §7.3:
  - Toggle ON; `G X 5 Enter` → read actor → X increased by 5 (±ε).
  - `G Shift+Z` drag → Z unchanged.
  - `S 2 Enter` → scale ×2. `R Z 90 Enter` → yaw +90.
  - Local: rotate cube 45°, `G X` Local (double-tap X) drag → moves along local X.
  - `Esc` cancels (transform restored). Ctrl+Z undoes a committed op in one step.
  - **Regression:** Toggle OFF → `G` toggles Game View (UE intact); `R`/`E`/`W` gizmos intact.
- [ ] **S4** Record results; fix any failures; re-verify. Done when all green.

## Self-Review

- **Spec coverage:** §1 accept criteria → Tasks 1–8; §4 components → Tasks 0–6 (Module T0, Settings/Commands T6, InputProcessor T4, Op T2, Math T1, HUD T5, Sink T3); §5 behavior → T2/T4; §6 non-breaking → T4 gate + T8 regression; §7 testing → T1/T2 unit + T8 e2e; §8 deploy → T7/T8. No gaps.
- **Placeholders:** none — math rules, signatures, test cases, build/test/symlink commands all concrete.
- **Type consistency:** `FXConstraint`, `EXMode/EXAxis/EXOrient`, `IXApplySink`/`FXApplied`, `FBlenderXformOp` method names consistent across T0–T4.
