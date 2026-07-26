#include "Misc/AutomationTest.h"
#include "BlenderXformInputProcessor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformActiveOpModifierChordPolicyTest, "BlenderXform.Input.ActiveOpModifierChordPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformActiveOpModifierChordPolicyTest::RunTest(const FString&)
{
	// Task 1.3 / D3: during an active op, modifier chords are default-DENIED - only the plugin's own
	// Alt+Shift+B toggle is passed through (it disables the plugin; the next Tick cancels the op).
	// Mutating shortcuts (Duplicate/Cut/Paste/Select All/Save/Undo/Redo) must NOT reach the editor
	// while our transaction is open. New pass-throughs must be enumerated here, never blanket-allowed.
	TestTrue(TEXT("Alt+Shift+B (toggle hotkey) is passed through"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::B, true, false, false, true));

	// Mutating editor chords are consumed (return false = not passed through), regardless of Ctrl vs Cmd.
	TestFalse(TEXT("Ctrl+Z (undo) is consumed during an active op"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::Z, false, true, false, false));
	TestFalse(TEXT("Cmd+Shift+Z (redo) is consumed during an active op"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::Z, true, false, true, false));
	TestFalse(TEXT("Ctrl+Y (redo) is consumed during an active op"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::Y, false, true, false, false));
	TestFalse(TEXT("Ctrl+S (save) is consumed during an active op"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::S, false, true, false, false));
	TestFalse(TEXT("Ctrl+X (cut) is consumed during an active op"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::X, false, true, false, false));
	TestFalse(TEXT("Ctrl+V (paste) is consumed during an active op"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::V, false, true, false, false));
	TestFalse(TEXT("Ctrl+W is consumed during an active op"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::W, false, true, false, false));
	TestFalse(TEXT("Ctrl+A (select all) is consumed during an active op"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::A, false, true, false, false));
	TestFalse(TEXT("Ctrl+D (duplicate) is consumed during an active op"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::D, false, true, false, false));

	// The toggle must be the EXACT chord - variants don't pass through.
	TestFalse(TEXT("Alt+B without Shift does not pass through"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::B, false, false, false, true));
	TestFalse(TEXT("Ctrl+Alt+Shift+B does not pass through (Ctrl changes the chord)"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::B, true, true, false, true));
	TestFalse(TEXT("plain B with no modifiers does not pass through"),
		FBlenderXformInputPolicy::ShouldPassThroughModifierChord(EKeys::B, false, false, false, false));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformSurfaceSnapInputPolicyTest, "BlenderXform.Input.SurfaceSnapTogglePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformSurfaceSnapInputPolicyTest::RunTest(const FString&)
{
	TestTrue(TEXT("plain C toggles during Move"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::C, false, false, false, false, false));
	TestFalse(TEXT("key repeat cannot toggle repeatedly"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::C, true, false, false, false, false));
	TestFalse(TEXT("idle C remains native UE input"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::None, EKeys::C, false, false, false, false, false));
	TestFalse(TEXT("Scale does not toggle surface mode"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Scale, EKeys::C, false, false, false, false, false));
	TestFalse(TEXT("Rotate does not toggle surface mode"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Rotate, EKeys::C, false, false, false, false, false));
	TestFalse(TEXT("other keys do not toggle"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::V, false, false, false, false, false));
	TestFalse(TEXT("Shift+C is reserved rather than toggling"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::C, false, true, false, false, false));
	TestFalse(TEXT("Ctrl+C remains an editor chord"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::C, false, false, true, false, false));
	TestFalse(TEXT("Alt+C remains an editor chord"),
		FBlenderXformInputPolicy::ShouldToggleSurfaceSnap(EXMode::Move, EKeys::C, false, false, false, false, true));

	TestFalse(TEXT("same pixel has not begun surface placement"),
		FBlenderXformInputPolicy::HasMovedForSurfaceSnap(FVector2D(10, 20), FVector2D(10.5, 20.5)));
	TestTrue(TEXT("more than one pixel begins surface placement"),
		FBlenderXformInputPolicy::HasMovedForSurfaceSnap(FVector2D(10, 20), FVector2D(11.1, 20)));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformViewportOwnershipPolicyTest, "BlenderXform.Input.ViewportOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformViewportOwnershipPolicyTest::RunTest(const FString&)
{
	// Sentinel addresses stand in for distinct viewport clients; the policy is a pure pointer-identity
	// check and never dereferences them.
	FLevelEditorViewportClient* const VP_A = reinterpret_cast<FLevelEditorViewportClient*>(static_cast<uintptr_t>(0x1000));
	FLevelEditorViewportClient* const VP_B = reinterpret_cast<FLevelEditorViewportClient*>(static_cast<uintptr_t>(0x2000));

	TestTrue(TEXT("the initiating viewport owns itself"),
		FBlenderXformInputPolicy::OwnsViewport(VP_A, VP_A));

	// Task 1.1: once an op starts in VP_A, another viewport client must not become the mouse-update
	// source, and a click in it must not commit/cancel the op.
	TestFalse(TEXT("a different viewport is not owned - cannot become the update source"),
		FBlenderXformInputPolicy::OwnsViewport(VP_A, VP_B));
	TestFalse(TEXT("a click in a non-initiating viewport does not own the op"),
		FBlenderXformInputPolicy::OwnsViewport(VP_A, VP_B));

	TestFalse(TEXT("no candidate viewport is not owned"),
		FBlenderXformInputPolicy::OwnsViewport(VP_A, nullptr));
	TestFalse(TEXT("with no active op (null initiating) nothing is owned"),
		FBlenderXformInputPolicy::OwnsViewport(nullptr, VP_A));
	TestFalse(TEXT("two null viewports own nothing"),
		FBlenderXformInputPolicy::OwnsViewport(nullptr, nullptr));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlenderXformFocusContextPolicyTest, "BlenderXform.Input.FocusContextPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBlenderXformFocusContextPolicyTest::RunTest(const FString&)
{
	// Task 1.2: a G/S/R op may start only when keyboard focus is in the viewport and NOT in a text
	// field or modal window. Plain-struct input keeps the policy unit-testable without a live Slate app.
	auto Ctx = [](bool bVP, bool bText, bool bModal) {
		FBlenderXformInputContext C;
		C.bViewportKeyboardFocus = bVP;
		C.bTextInputFocus = bText;
		C.bModalWindowActive = bModal;
		return C;
	};

	TestTrue (TEXT("viewport focus alone allows start"),
		FBlenderXformInputPolicy::CanStartInContext(Ctx(true,  false, false)));

	// Typing in a rename/details/search/save field must never start a transform.
	TestFalse(TEXT("text-entry focus blocks start even with viewport focus"),
		FBlenderXformInputPolicy::CanStartInContext(Ctx(true,  true,  false)));
	TestFalse(TEXT("text-entry focus alone blocks start"),
		FBlenderXformInputPolicy::CanStartInContext(Ctx(false, true,  false)));

	// A modal window (save dialog, etc.) owns input and must block start.
	TestFalse(TEXT("modal window blocks start even with viewport focus"),
		FBlenderXformInputPolicy::CanStartInContext(Ctx(true,  false, true)));
	TestFalse(TEXT("modal window alone blocks start"),
		FBlenderXformInputPolicy::CanStartInContext(Ctx(false, false, true)));

	// No viewport focus at all blocks start (the key belongs to another panel).
	TestFalse(TEXT("no viewport focus blocks start"),
		FBlenderXformInputPolicy::CanStartInContext(Ctx(false, false, false)));

	// Worst case: focus left viewport and landed in a text field under a modal window.
	TestFalse(TEXT("text + modal together block start"),
		FBlenderXformInputPolicy::CanStartInContext(Ctx(false, true,  true)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
