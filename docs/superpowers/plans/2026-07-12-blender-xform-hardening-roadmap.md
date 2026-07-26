# Blender XForm Hardening Roadmap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Status:** Approved — Iteration 1 authorized (D1–D5 adopted recommended defaults); Iterations 2–4 await per-stage approval per D5.

**Goal:** 将 BlenderXform 从当前“核心功能可用、单元测试通过”的状态，迭代为输入边界安全、事务可恢复、数学稳定、性能可控、可持续发布的 UE 5.7 编辑器插件。

**Architecture:** 保持现有 `InputProcessor -> Op -> Math -> EditorSink -> HUD` 分层，不进行大规模重构。先在输入和编辑器副作用边界补安全护栏与真实集成测试，再改善数学求解、Surface Snap 和多视口 HUD，最后补齐 CI、文档和发布流程。

**Tech Stack:** Unreal Engine 5.7 Editor C++、Slate `IInputProcessor`、`FScopedTransaction`、UE Automation Tests、UnrealBuildTool、Windows/macOS 编辑器构建。

## Global Constraints

- 本路线图只针对 Editor 插件，不增加 Runtime/Game 模块依赖。
- 保持 Toggle OFF 时不消费 BlenderXform 输入、不改变 UE 原生快捷键行为。
- 每个功能修复必须先写失败测试，再做最小实现，再运行相关测试和完整 `BlenderXform` 测试集。
- 不在正确性加固阶段顺带加入组件编辑、Individual Origins、法线对齐等新产品功能。
- 不改变当前 `G/S/R`、`X/Y/Z`、`Shift+Axis`、`Shift+D`、`C`、确认/取消的默认键位。
- 所有活动操作必须保持单个 Undo 步骤；Cancel 不得留下 Undo 记录或预览 Transform。
- Windows UE 5.7 构建和 `-NullRHI` 自动化测试是每个迭代的最低验收门槛。
- macOS Escape 硬件轮询保留，除非新的焦点/视口生命周期测试证明可安全替换。

---

## Baseline Evidence

审查基线日期：2026-07-12。

- `G1_ProjectEditor Win64 Development` 构建成功。
- `Automation RunTests BlenderXform`：27 succeeded、0 warnings、0 failed、0 not run。
- 当前插件版本：`0.1.0`。
- 当前自动化测试主要覆盖纯数学、Op 状态机、输入策略、HUD 文本和单次 WorldStatic Trace；尚未充分覆盖真实 Slate 焦点、事务、Undo、附着层级、多视口和模块卸载。

## Approval Gates

- [x] **A1 — 批准 Iteration 1：输入、焦点、事务与 Actor 安全护栏。**
- [ ] **A2 — 批准 Iteration 2：变换数学和数值输入稳定性。**
- [ ] **A3 — 批准 Iteration 3：Surface Snap 性能/精度与多视口 HUD。**
- [ ] **A4 — 批准 Iteration 4：集成测试、CI、文档和发布流程。**
- [x] **A5 — 批准 Deferred Backlog 仅作为后续候选，不进入本轮加固。**

建议先批准并执行 Iteration 1；Iteration 2-4 可整体批准，也可以在前一阶段验收后逐阶段批准。

---

# Iteration 1 — 输入、焦点、事务与 Actor 安全护栏

**目标版本建议：** `0.2.0`

**完成定义：** 活动操作只能由发起视口驱动；焦点离开时不会误提交；危险编辑器快捷键不会在开放事务内修改世界；模块卸载会取消操作；锁定或不可编辑 Actor 不会被直接变换；附着 Actor 的 Clear Transform 使用相对语义。

## Task 1.1：将模态操作绑定到发起视口

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformInputProcessorTest.cpp`

**Interfaces:**
- Produce: `FLevelEditorViewportClient* InitiatingViewportClient`
- Produce: `bool OwnsViewport(const FLevelEditorViewportClient* ViewportClient) const`
- Produce: `void EndActiveOperation(bool bCommit)` or等价的单一生命周期入口

- [ ] 写失败测试：操作启动后，另一个 viewport client 不能成为鼠标更新来源。
- [ ] 写失败测试：非发起视口的 LMB/RMB 不得提交或取消当前操作。
- [ ] 在 `CaptureStartCursor()` 时保存发起 viewport client 和 viewport 指针。
- [ ] `UpdateFromMouse()`、`EnsureViewCache()`、deprojection 全部只使用发起视口。
- [ ] 发起视口销毁或失效时执行 Cancel，并清空 viewport 引用。
- [ ] Commit、Cancel、插件关闭和模块卸载后清空 viewport 引用。
- [ ] 运行 `BlenderXform.Input` 测试，预期全部通过。

**Acceptance:**
- 多视口切换不会让 Actor 跳变。
- 点击另一个视口或非视口 UI 不会被解释为当前操作的确认。
- 发起视口关闭时操作自动取消且不留下 Undo 项。

## Task 1.2：加入 Slate 键盘焦点和模态窗口门禁

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformInputProcessorTest.cpp`

**Interfaces:**
- Produce: `bool IsKeyboardFocusInInitiatingViewport() const`
- Produce: 可纯测试的 focus/context policy，输入至少包含 viewport-focused、text-entry-focused、modal-window-active 三种状态

- [ ] 写失败策略测试：文本输入焦点时不能启动 `G/S/R`。
- [ ] 写失败策略测试：活动 modal window 时不能启动操作。
- [ ] 写失败策略测试：Level Viewport 拥有键盘焦点时允许启动。
- [ ] 将 `CanStartHere()` 从“鼠标在视口内”提升为“鼠标在视口内且键盘焦点属于该视口”。
- [ ] 活动操作期间检测焦点离开；按审批决定执行 Cancel，而不是静默继续。
- [ ] 保留 Toggle OFF 的完全 pass-through 行为。

**Acceptance:**
- Outliner 重命名、Details 文本框、搜索框、保存对话框中输入 `G/S/R` 不会启动变换。
- Level Viewport 正常聚焦时现有快捷键行为不变。

## Task 1.3：收紧活动操作期间的编辑器快捷键策略

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformInputProcessorTest.cpp`

**Interfaces:**
- Replace/extend: `FBlenderXformInputPolicy::ShouldConsumeUnsafeEditorShortcut(...)`
- Produce: 明确的 active-op modified-chord allowlist/denylist policy

- [ ] 将现有“unrelated Ctrl chords pass through”测试改为安全策略测试。
- [ ] 写失败测试：Ctrl/Cmd+X、V、W、A 在活动操作中被消费。
- [ ] 写失败测试：Ctrl/Cmd+Z、Y、S 继续被消费。
- [ ] 写失败测试：`Alt+Shift+B` 仍允许触发插件关闭，并由下一 Tick 或显式入口取消操作。
- [ ] 实现默认拒绝修改性 Ctrl/Cmd/Alt chord，仅放行明确批准的非修改性 chord。
- [ ] 对未来新增放行 chord，要求在测试中逐个列出，禁止“其他全部放行”。

**Acceptance:**
- 活动事务期间无法通过快捷键 Duplicate、Cut、Paste、Select All 或 Save。
- Toggle hotkey 仍能关闭插件，关闭后 Actor 恢复操作前状态。

## Task 1.4：模块卸载与析构时取消活动事务

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformModule.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformEditorSink.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformOpTest.cpp`

**Interfaces:**
- Produce: `void FBlenderXformInputProcessor::Shutdown()`
- Guarantee: `Shutdown()` 幂等；活动时 Cancel，非活动时无副作用

- [ ] 写失败测试：活动 Op 的 shutdown 路径必须调用 Sink Cancel，不能调用 Commit。
- [ ] 实现 `InputProcessor::Shutdown()`，先 Cancel 再清 session surface state 和 viewport state。
- [ ] `ShutdownModule()` 在注销 input preprocessor 前调用 `Shutdown()`。
- [ ] InputProcessor 析构中增加兜底 `Shutdown()` 或断言活动状态已结束。
- [ ] Sink 析构中禁止开放事务被静默 finalize；活动事务必须先 Cancel。
- [ ] 验证重复 Shutdown 不崩溃、不重复调用 Cancel。

**Acceptance:**
- Live Coding、模块卸载、编辑器退出路径不会留下预览 Transform。
- Shutdown 后不新增 Undo 项。

## Task 1.5：统一 Actor/Level 可变换性规则

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformEditorSink.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformEditorSink.cpp`
- Modify: `BlenderXform/Source/BlenderXform/BlenderXform.Build.cs` only if an editor utility module is required
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformEditorSinkTest.cpp`

**Interfaces:**
- Produce: `bool CanTransformActor(const AActor* Actor)` covering class, editability, location lock and level lock
- Preserve: Landscape remains rejected

- [ ] 写失败测试：location-locked Actor 被拒绝。
- [ ] 写失败测试：locked Level 中的 Actor 被拒绝。
- [ ] 写失败测试：普通 editable Actor 被允许。
- [ ] 写失败测试：Landscape 继续被拒绝。
- [ ] 实现统一 eligibility check，并让 `HasSelection()`、`CaptureSelection()`、`ClearComponent()`、`BeginDuplicate()` 使用同一规则。
- [ ] 混合选区采用审批后的策略；推荐严格模式：任何一个 Actor 不允许变换，则整个操作 pass-through。
- [ ] HUD 或日志给出拒绝原因，至少区分 locked actor、locked level、unsupported class。

**Acceptance:**
- 插件不绕过 UE Actor/Level 锁定保护。
- 同一 Actor 在 modal transform、Shift+D 和 Alt-clear 中得到一致判断。

## Task 1.6：修正附着 Actor 的 Alt-clear 语义和通知

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformEditorSink.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformEditorSinkTest.cpp`

**Interfaces:**
- Preserve: 无父 Actor 时清除世界 location/rotation/scale
- Produce: 有父 Actor 时清除 root component relative location/rotation/scale

- [ ] 写失败测试：附着 Actor 的 Alt+G 将 relative location 置零，而不是移动到世界原点。
- [ ] 写失败测试：附着 Actor 的 Alt+R/Alt+S 清除 relative rotation/scale。
- [ ] 写失败测试：ClearComponent 调用最终编辑器移动通知并可 Undo。
- [ ] 最小实现 relative/world 分支。
- [ ] 调用与 modal path 一致的 `PostEditMove(true)` 和 viewport/pivot 刷新。
- [ ] 无实际变化时取消 transaction，避免无意义 Undo 项。

**Acceptance:**
- Parent-child 层级中的 Alt+G/S/R 与 Blender 的 clear-relative-transform 语义一致。
- Clear 后依赖 Construction Script/灯光/可视化正确刷新。

## Iteration 1 Verification Gate

- [ ] 构建：

```powershell
& 'D:\SDGF_G1_Project\UnrealEngine\Engine\Build\BatchFiles\Build.bat' G1_ProjectEditor Win64 Development '-Project=D:\SDGF_G1_Project\G1_Project\G1_Project.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected: `Result: Succeeded`。

- [ ] 自动化测试：

```powershell
& 'D:\SDGF_G1_Project\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\SDGF_G1_Project\G1_Project\G1_Project.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests BlenderXform; Quit' '-TestExit=Automation Test Queue Empty' '-ReportOutputPath=D:\SDGF_G1_Project\G1_Project\Saved\Automation\BlenderXformIteration1' -log
```

Expected: 0 failed、0 not run。

- [ ] 手动验收：单视口、多视口、文本框焦点、点击 Toolbar、Alt+Shift+B、locked actor、locked level、attached actor、Undo/Cancel。
- [ ] 审核 Git diff，确认没有引入 Iteration 2-4 的功能。

---

# Iteration 2 — 变换数学和数值输入稳定性

**目标版本建议：** `0.3.0`

## Task 2.1：按约束类型拆分 Move 求解

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Public/BlenderXformMath.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformMath.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformMathTest.cpp`

- [ ] 为 Free、single-axis、plane 三种鼠标求解分别写失败测试。
- [ ] 增加“约束轴接近相机 forward”测试，结果必须有限且具有可操作位移。
- [ ] Free 使用 view-facing plane。
- [ ] Plane 使用约束平面 ray intersection。
- [ ] Single-axis 使用 ray 与世界轴线的最近点参数求解。
- [ ] Local basis、precision 和 grid snap 继续在求解后应用。

**Acceptance:** 轴向移动在正视、侧视和轴指向相机时均可控，无 NaN/Inf 和明显跳变。

## Task 2.2：鼠标旋转改为累计角度

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformOp.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformOp.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Public/BlenderXformMath.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformMath.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformMathTest.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformOpTest.cpp`

- [ ] 写失败测试：从 +179° 跨到 -179° 时累计结果增加约 2°，不跳回 -179°。
- [ ] 写失败测试：鼠标可累计超过 360°。
- [ ] Op 保存上一帧屏幕角度和累计角度；mode switch/Begin/Reset 时正确清零。
- [ ] Numeric rotation 继续直接使用精确输入，不经过累计器。
- [ ] Snap 对累计结果生效。

**Acceptance:** 鼠标旋转可连续多圈，跨 ±180° 无反向跳变。

## Task 2.3：数值输入有效性、范围和不支持上下文提示

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformOp.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformOp.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformHUD.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformOpTest.cpp`

- [ ] 写失败测试：超长数值、非 finite 解析结果不能进入 Apply。
- [ ] 写失败测试：Free/Plane Move numeric 不再静默显示成功后应用零位移。
- [ ] 为 numeric buffer 增加最大长度和 `FMath::IsFinite` 检查。
- [ ] 对无效输入显示 `[invalid]`，并阻止 Commit，直到用户修正或 Backspace 清空。
- [ ] 对当前不支持的 Free/Plane Move numeric 显示 `[axis required]`；本迭代不发明新的平面数值方向语义。
- [ ] Scale 零值和负值策略形成明确测试；推荐保留负 scale、允许零 scale但显示警告。

**Acceptance:** 无效数字不会生成无穷或非有限 Transform；不支持的数字上下文不会静默 no-op。

## Iteration 2 Verification Gate

- [ ] 完整 Windows 构建成功。
- [ ] 所有 `BlenderXform.Math`、`BlenderXform.Op` 和完整测试集通过。
- [ ] 手动从多种相机角度验证 X/Y/Z axis move、plane move、360°+ rotation、极值 numeric input。

---

# Iteration 3 — Surface Snap 性能/精度与多视口 HUD

**目标版本建议：** `0.4.0`

## Task 3.1：输入更新和 Surface Trace 每帧合并

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformInputProcessor.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformInputProcessorTest.cpp`

- [ ] 写失败测试：同一 frame 内多次 mouse move 只触发一次 world update/trace。
- [ ] MouseMove 只缓存最新 cursor 状态并标记 dirty。
- [ ] `Tick()` 每帧最多执行一次 deprojection、surface trace 和 Op update。
- [ ] Modifier/snap 状态变化同样标记 dirty，不直接重复 Apply。
- [ ] Commit 前若存在 dirty cursor state，先刷新最后一次 preview 再提交。

**Acceptance:** 高频鼠标输入不会让 trace/apply 次数超过每帧一次，最终位置仍对应最后鼠标坐标。

## Task 3.2：Surface Snap 锚点与 Bounds 策略显式化

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Public/BlenderXformSettings.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformEditorSink.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformEditorSink.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformSurfaceSnap.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformSurfaceSnap.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformSurfaceSnapTest.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformEditorSinkTest.cpp`

- [ ] 保持默认策略为 Collision Bounds，确保现有用户行为不变。
- [ ] 增加显式 Visual Bounds 和 Actor Origin 策略，是否加入 OBB 在审批时单独决定。
- [ ] 将固定 `0.1 cm` gap 移入设置，带合理 clamp。
- [ ] 写测试覆盖 collision/visual/origin/gap。
- [ ] HUD 显示当前 surface anchor policy，或在设置说明中明确。

**Acceptance:** 用户可解释并选择接触基准；默认行为兼容现版本。

## Task 3.3：Surface Trace 目标策略与性能观测

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Public/BlenderXformSettings.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformEditorSink.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformEditorSinkTest.cpp`

- [ ] 保持默认 WorldStatic + static mobility。
- [ ] 将最大隐藏命中重试次数做成内部常量并记录 trace 次数。
- [ ] 增加可选 debug stat/log，输出每帧 trace pass、retry 和耗时；默认关闭。
- [ ] 增加测试：hidden target 后的 visible target、complex miss/simple hit、selection ignored。
- [ ] 仅在有明确需求时开放自定义 object channel；本任务不默认追踪 Movable target。

**Acceptance:** Surface Snap 性能可观测，默认目标范围不扩大，不引入意外可移动对象吸附。

## Task 3.4：HUD 绑定发起视口并改为上下文提示

**Files:**
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformHUD.h`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformHUD.cpp`
- Modify: `BlenderXform/Source/BlenderXform/Private/BlenderXformModule.cpp`
- Test: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformHUDTest.cpp`

- [ ] HUD callback 提供发起 viewport identity，而不是读取全局 active viewport。
- [ ] 非发起 viewport 不绘制 cursor tag 和 hint bar。
- [ ] Move 显示 C surface；Scale/Rotate 不显示无效的 C 提示。
- [ ] Numeric invalid/axis-required 状态可见。
- [ ] 所有用户可见固定文本迁移到 `LOCTEXT` 或等价本地化路径。

**Acceptance:** 四视口布局中 HUD 只出现在发起视口，位置正确，提示与当前 mode 一致。

## Iteration 3 Verification Gate

- [ ] 完整构建和测试通过。
- [ ] 用高轮询率鼠标和大型选区观察每帧 trace/apply 次数。
- [ ] 验证复杂碰撞、简单碰撞、隐藏 Actor、旋转 Mesh、视觉外伸 Mesh。
- [ ] 四视口布局中 HUD 不重复、不串 viewport。

---

# Iteration 4 — 集成测试、CI、文档和发布流程

**目标版本建议：** `1.0.0-rc1`

## Task 4.1：建立真实 Editor Sink / Undo / Selection 集成测试

**Files:**
- Create: `BlenderXform/Source/BlenderXform/Private/Tests/BlenderXformEditorIntegrationTest.cpp`
- Modify: `BlenderXform/Source/BlenderXform/BlenderXform.Build.cs` only if test helpers require another editor module

- [ ] Spawn、select、transform、commit、undo，验证 Actor transform 恢复。
- [ ] Begin、preview、cancel，验证 transform 和 selection 恢复且无 Undo 项。
- [ ] Shift+D confirm/cancel，验证 copy 生命周期和 originals selection。
- [ ] Attached parent/child、locked actor、locked level、Blueprint actor 分别建立测试。
- [ ] 多 Level 混合选区按审批策略建立测试。

**Acceptance:** 关键事务和选区行为不再只依赖 fake sink 测试。

## Task 4.2：建立可重复的 Windows CI

**Files:**
- Create: CI workflow or repository-local build script selected by repository policy
- Create: `Scripts/Test-BlenderXform.ps1`
- Modify: `README.md`

- [ ] 脚本接收 Engine 路径、uproject 路径和 report 目录参数。
- [ ] 脚本依次执行 UBT build 和 `Automation RunTests BlenderXform`。
- [ ] 任一 build/test failure 返回非零退出码。
- [ ] 保存 `index.json` 和 editor log 为 CI artifacts。
- [ ] CI 至少在 Windows UE 5.7 受控 runner 上运行。

**Acceptance:** 新提交可以自动阻止编译失败和 BlenderXform 自动化测试回归。

## Task 4.3：macOS 回归与平台支持矩阵

**Files:**
- Modify: `README.md`
- Create: `docs/platform-support.md`

- [ ] 在 macOS UE 5.7 运行同一测试集。
- [ ] 手动验证 hardware Escape、Cmd chords、Retina/DPI HUD 和 Shift precision。
- [ ] 记录 Windows/macOS 已验证版本、测试日期和已知限制。
- [ ] Linux 未验证时明确标记，不宣称支持。

**Acceptance:** 平台声明由实际构建和测试证据支持。

## Task 4.4：版本、Changelog、安装和打包

**Files:**
- Modify: `BlenderXform/BlenderXform.uplugin`
- Modify: `README.md`
- Create: `CHANGELOG.md`
- Create: `Scripts/Install-BlenderXform.ps1`
- Create: `Scripts/Install-BlenderXform.sh`

- [ ] 根据已批准并完成的迭代更新 Version/VersionName。
- [ ] Changelog 按 Added/Changed/Fixed/Known Limitations 记录。
- [ ] Windows 安装脚本使用 copy 或 junction，不创建包含 Unix 路径的文本占位文件。
- [ ] macOS/Linux 脚本使用有效 symlink，并验证目标包含 `.uplugin`。
- [ ] 打包产物排除 `Binaries`、`Intermediate`、测试报告和宿主项目文件。
- [ ] 在干净测试项目中安装、编译、启用、禁用和删除插件。

**Acceptance:** 用户可以在干净项目中按文档完成安装和卸载，仓库版本与发布说明一致。

## Iteration 4 Verification Gate

- [ ] Windows CI 全绿。
- [ ] macOS 构建/测试全绿。
- [ ] 干净项目安装测试通过。
- [ ] README、Changelog、版本号和实际功能一致。
- [ ] 创建 release candidate tag 前执行完整 diff 和测试报告审核。

---

# Deferred Backlog — 不进入本轮加固

以下内容只有在 A5 单独批准后，才进入后续产品设计和独立 implementation plan：

- [ ] Pivot 模式：Median、Active、Bounding Box、Individual Origins、Custom Pivot。
- [ ] Orientation：Normal、View、Parent、自定义 Transform Orientation。
- [ ] Component、BSP、vertex/edge/face 编辑。
- [ ] Free/Plane Move 的 Blender 式数值方向语义。
- [ ] 数值单位与表达式：`1m`、`50cm`、`2*100`。
- [ ] Surface Snap 自动旋转对齐法线。
- [ ] Vertex/edge/face snapping。
- [ ] Movable target surface snapping。
- [ ] Proportional editing。
- [ ] 用户自定义完整键位映射。

这些项目需要独立的交互设计，不能作为“修 bug 时顺便实现”的内容。

---

# Owner Decisions Required Before Iteration 1

## D1：焦点离开发起视口时如何处理

- [x] **Cancel（推荐）：**恢复操作前状态，不留下 Undo；最安全、最容易理解。
- [ ] Commit：保留当前 preview；风险是误点击 UI 会意外提交。
- [ ] Pause：回到视口后继续；状态最复杂，当前不推荐。

## D2：混合选区包含 locked/unsupported Actor 时如何处理

- [x] **拒绝整个操作（推荐）：**所有 Actor 保持一致，无部分变换。
- [ ] 跳过不可变对象：允许部分变换，但需要 HUD 明确显示 skipped count。

## D3：活动操作期间的 modifier chords

- [x] **默认全部消费，仅白名单 Alt+Shift+B（推荐）。**
- [ ] 只阻止已知修改性快捷键：兼容性更高，但第三方插件命令仍可能破坏事务。

## D4：Surface Snap Bounds 扩展范围

- [x] **Iteration 3 加入 Collision / Visual / Origin 三种策略（推荐）。**
- [ ] 只做性能优化，保持 Collision-first 单一策略。
- [ ] 同期加入 OBB/几何支撑点：精度更高，但范围和风险明显增大，建议独立设计。

## D5：版本节奏

- [x] **每个 Iteration 单独版本和审核（推荐）：0.2.0 -> 0.3.0 -> 0.4.0 -> 1.0.0-rc1。**
- [ ] 四个 Iteration 合并为一次大版本：审核周期更长、回归定位更困难。

---

# Plan Self-Review

- Spec coverage: 已覆盖输入/焦点、危险快捷键、事务卸载、Actor 锁定、attached clear、数学退化、数值校验、Surface Snap 性能/Bounds、多视口 HUD、真实集成测试、CI、平台和发布。
- Scope control: Pivot/Orientation/Component/法线对齐等产品扩展已移入 Deferred Backlog。
- Dependency order: Iteration 1 提供 viewport ownership 和生命周期基础；Iteration 3 HUD/每帧合并依赖这些基础；Iteration 4 在行为稳定后建立发布门槛。
- Verification: 每个 Iteration 都有独立构建、自动化和手动验收门槛。
- Approval readiness: D1-D5 是开始 Iteration 1 前唯一需要产品所有者明确选择的决策。
