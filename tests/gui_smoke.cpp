//=============================================================================
// fairklaus — GUI-Smoke-Test (headless, kein X11 noetig)
//
// Baut den echten Editor (PanelEditor) und die echten Controls (KnobControl,
// SwitchControl) auf, treibt sie mit synthetischen Maus-Events und prueft,
// ob Drag / Mausrad / Klick die Parameter des Prozessors aendern:
//   - Threshold-Knopf: Drag nach oben erhoeht, Mausrad runter senkt
//   - Time-Constant-Knopf: Detents rasten bei 1..5 ein
//   - Power-Schalter: Klick toggelt
//=============================================================================
#include <cstdio>
#include <cmath>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

#include "Parameters.h"
#include "PunishChild670.h"
#include "editor/Editor.h"

// sonst aus linuxmain.cpp des Plugins; hier selbst definieren (gui_smoke ist kein Modul)
void* moduleHandle = nullptr;

using namespace Steinberg;
using namespace Steinberg::Vst;

static int failures = 0;
static void expect (bool ok, const char* what)
{
	std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) ++failures;
}

int main ()
{
	setvbuf (stdout, nullptr, _IONBF, 0);

	auto* comp = static_cast<IComponent*> (new fairklaus::PunishChild670 ());
	expect (comp->initialize (nullptr) == kResultOk, "initialize");
	IAudioProcessor* ap = nullptr; comp->queryInterface (IAudioProcessor::iid, (void**) &ap);
	IEditController* ctrl = nullptr; comp->queryInterface (IEditController::iid, (void**) &ctrl);
	ProcessSetup setup { kRealtime, kSample32, 512, 48000.0 };
	ap->setupProcessing (setup);
	expect (comp->setActive (true) == kResultOk, "setActive");

	ctrl->setParamNormalized (fairklaus::kParamPower, 1.0);
	ctrl->setParamNormalized (fairklaus::kParamThreshold, 0.0);

	auto* editor = new fairklaus::PanelEditor (static_cast<fairklaus::PunishChild670*> (comp));

	VSTGUI::CButtonState leftBtn (VSTGUI::kLeftButton);
	VSTGUI::CButtonState noBtn (0);
	VSTGUI::CPoint p0 (0, 0);

	// --- Threshold-Knopf: Drag nach oben (dy positiv) erhoeht ---
	fairklaus::KnobControl knob (VSTGUI::CRect (485, 70, 565, 150), editor, fairklaus::kParamThreshold);
	knob.setValueNormalized (0.0f);
	{
		VSTGUI::CPoint down (525, 110);
		knob.onMouseDown (down, leftBtn);
		VSTGUI::CPoint move (525, 60);   // dy = 110-60 = 50 -> +0.25
		knob.onMouseMoved (move, leftBtn);
		knob.onMouseUp (move, noBtn);
	}
	const double thrDrag = ctrl->getParamNormalized (fairklaus::kParamThreshold);
	std::printf ("thr nach Drag = %.4f\n", thrDrag);
	expect (std::fabs (thrDrag - 0.25) < 0.01, "Drag am Threshold-Knopf aendert Parameter (~0.25)");

	// --- Mausrad: deltaY < 0 senkt ---
	{
		VSTGUI::MouseWheelEvent wheel;
		wheel.mousePosition = VSTGUI::CPoint (525, 110);
		wheel.deltaY = -1.0;
		knob.onMouseWheelEvent (wheel);
	}
	const double thrWheel = ctrl->getParamNormalized (fairklaus::kParamThreshold);
	std::printf ("thr nach Wheel = %.4f\n", thrWheel);
	expect (thrWheel < thrDrag - 0.01, "Mausrad am Threshold-Knopf senkt Parameter");

	// --- Time-Constant-Knopf mit Detents: grosser Drag rastet auf 1.0 ein ---
	fairklaus::KnobControl tc (VSTGUI::CRect (620, 70, 700, 150), editor, fairklaus::kParamTimeConstant);
	tc.setDetents (5);
	tc.setValueNormalized (0.0f);
	{
		VSTGUI::CPoint down (660, 110);
		tc.onMouseDown (down, leftBtn);
		VSTGUI::CPoint move (660, 10);    // dy = 100 -> 0.5 -> gerastet 0.5
		tc.onMouseMoved (move, leftBtn);
		VSTGUI::CPoint move2 (660, -500);
		tc.onMouseMoved (move2, leftBtn); // grosser Drag -> rastet auf 1.0
		tc.onMouseUp (move2, noBtn);
	}
	const double tcVal = ctrl->getParamNormalized (fairklaus::kParamTimeConstant);
	std::printf ("time-const nach Drag = %.4f\n", tcVal);
	expect (tcVal == 1.0, "Time-Constant rastet bei 1..5 ein (hier 1.0)");

	// --- Power-Schalter: Klick toggelt auf 0 ---
	fairklaus::SwitchControl sw (VSTGUI::CRect (52, 22, 78, 76), editor, fairklaus::kParamPower);
	sw.setValueNormalized (1.0f);
	{
		VSTGUI::CPoint click (65, 49);
		sw.onMouseDown (click, leftBtn);
	}
	const double powVal = ctrl->getParamNormalized (fairklaus::kParamPower);
	std::printf ("power nach Klick = %.1f\n", powVal);
	expect (powVal == 0.0, "Power-Schalter toggelt auf OFF");

	editor->forget ();
	comp->setActive (false);
	comp->terminate ();
	comp->release ();

	if (failures) { std::printf ("GUI SMOKE: %d FAILED\n", failures); return 1; }
	std::printf ("GUI SMOKE: all ok\n");
	return 0;
}
