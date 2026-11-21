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
#include <vector>
#include <dlfcn.h>

#include "Parameters.h"
#include "PunishChild670.h"     // zuerst: sonst kollidiert das setState-Makro in vstsinglecomponenteffect.h
#include "editor/Editor.h"
#include "vstgui/lib/events.h"
#include "vstgui/lib/platform/platformfactory.h"

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"

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
	// Platform-Factory initialisieren (Event::Event() braucht getTicks()); kein X11/RunLoop noetig
	VSTGUI::initPlatform ((VSTGUI::PlatformInstanceHandle) dlopen (nullptr, RTLD_LAZY));

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

	VSTGUI::CButtonState leftBtn (VSTGUI::kLButton);
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
	std::printf ("wheel: start\n");
	{
		VSTGUI::MouseWheelEvent wheel;
		wheel.mousePosition = VSTGUI::CPoint (525, 110);
		wheel.deltaY = -1.0;
		std::printf ("wheel: event gebaut\n");
		knob.onMouseWheelEvent (wheel);
		std::printf ("wheel: handler fertig\n");
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

	// --- End-to-End: Threshold per Knopf aendern -> process() -> Ausgabe aendert sich ---
	{
		ctrl->setParamNormalized (fairklaus::kParamThreshold, 0.0);
		auto runAmp = [&] () {
			const int32 N = 256;
			std::vector<Sample32> inL (N), inR (N), outL (N), outR (N);
			Sample32* inCh[2] = { inL.data (), inR.data () };
			Sample32* outCh[2] = { outL.data (), outR.data () };
			AudioBusBuffers inB, outB;
			inB.numChannels = 2; inB.silenceFlags = 0; inB.channelBuffers32 = inCh;
			outB.numChannels = 2; outB.silenceFlags = 0; outB.channelBuffers32 = outCh;
			AudioBusBuffers inA[1] = { inB }, outA[1] = { outB };
			double m = 0;
			for (int b = 0; b < 120; ++b)
			{
				for (int i = 0; i < N; ++i)
				{
					double x = 0.9 * std::sin (2 * M_PI * 1000 * (b * N + i) / 48000.0);
					inL[i] = (Sample32) x; inR[i] = (Sample32) x;
				}
				ProcessData d {};
				d.processMode = kRealtime; d.symbolicSampleSize = kSample32; d.numSamples = N;
				d.numInputs = 1; d.numOutputs = 1; d.inputs = inA; d.outputs = outA;
				ap->process (d);
				if (b >= 60) for (int i = 0; i < N; ++i) m = std::max (m, (double) std::fabs (outL[i]));
			}
			return m;
		};
		const double ampLow = runAmp ();
		ctrl->setParamNormalized (fairklaus::kParamThreshold, 1.0);   // max Limiting
		const double ampHigh = runAmp ();
		std::printf ("amp thr=low %.4f  thr=high %.4f\n", ampLow, ampHigh);
		expect (ampHigh < ampLow * 0.9, "Threshold per Knopf/Param aendert Gain Reduction in process()");
	}

	// --- OS-Umschalter: Klick zyklisch 4x -> 8x (norm 0.5 -> 1.0) ---
	fairklaus::OsSelector os (VSTGUI::CRect (52, 158, 78, 206), editor, fairklaus::kParamOversampling);
	os.setValueNormalized (2.0f / 3.0f);   // 4x
	{
		VSTGUI::CPoint click (65, 180);
		os.onMouseDown (click, leftBtn);
	}
	const double osVal = ctrl->getParamNormalized (fairklaus::kParamOversampling);
	std::printf ("os nach Klick = %.2f (1.0 = 8x)\n", osVal);
	expect (osVal > 0.99, "OS-Umschalter klickt auf 8x (norm 1.0)");

	// --- Klick auf Skalen-Zahl: TC-Knopf Mitte (660,110), "5" bei 45° -> (705,65) ---
	{
		ctrl->setParamNormalized (fairklaus::kParamTimeConstant, 0.0);   // 1
		VSTGUI::CPoint click5 (705, 65);    // Zahl "5" (oben rechts)
		tc.onMouseDown (click5, leftBtn);
		const double tc5 = ctrl->getParamNormalized (fairklaus::kParamTimeConstant);
		std::printf ("time-const nach Klick auf 5 = %.4f (erwartet 1.0)\n", tc5);
		expect (tc5 > 0.99, "Klick auf Skalen-Zahl 5 springt auf 5");
		VSTGUI::CPoint click1 (615, 65);    // Zahl "1" (oben links)
		tc.onMouseDown (click1, leftBtn);
		const double tc1 = ctrl->getParamNormalized (fairklaus::kParamTimeConstant);
		std::printf ("time-const nach Klick auf 1 = %.4f (erwartet 0.0)\n", tc1);
		expect (tc1 < 0.01, "Klick auf Skalen-Zahl 1 springt auf 1");
	}

	// --- Wheel auf Time-Constant (Detents): eine Stufe pro Raster ---
	{
		ctrl->setParamNormalized (fairklaus::kParamTimeConstant, 1.0);   // 5
		VSTGUI::MouseWheelEvent wheel;
		wheel.mousePosition = VSTGUI::CPoint (660, 110);
		wheel.deltaY = -1.0;
		tc.onMouseWheelEvent (wheel);   // 5 -> 4
	}
	const double tcWheel = ctrl->getParamNormalized (fairklaus::kParamTimeConstant);
	std::printf ("time-const nach Wheel = %.4f (erwartet 0.75 = Pos 4)\n", tcWheel);
	expect (std::fabs (tcWheel - 0.75) < 0.01, "Mausrad am Time-Constant schaltet eine Detent-Stufe weiter");

	// --- Routing durch echtes CFrame: Events ueber frame->onMouse*Event ---
	{
		ctrl->setParamNormalized (fairklaus::kParamThreshold, 0.0);
		VSTGUI::CFrame frame (VSTGUI::CRect (0, 0, 1058, 297), editor);
		fairklaus::KnobControl knob2 (VSTGUI::CRect (485, 70, 565, 150), editor, fairklaus::kParamThreshold);
		frame.addView (&knob2);
		knob2.setValueNormalized (0.0f);
		VSTGUI::MouseDownEvent down (VSTGUI::CPoint (525, 110), VSTGUI::MouseEventButtonState (VSTGUI::MouseButton::Left));
		frame.onMouseDownEvent (down);
		VSTGUI::MouseMoveEvent move (VSTGUI::CPoint (525, 60), VSTGUI::MouseEventButtonState (VSTGUI::MouseButton::Left));
		frame.onMouseMoveEvent (move);
		VSTGUI::MouseUpEvent up (VSTGUI::CPoint (525, 60), VSTGUI::MouseEventButtonState (VSTGUI::MouseButton::None));
		frame.onMouseUpEvent (up);
		VSTGUI::MouseWheelEvent wheel;
		wheel.mousePosition = VSTGUI::CPoint (525, 110);
		wheel.deltaY = 1.0;
		frame.onMouseWheelEvent (wheel);
		const double thrFrame = ctrl->getParamNormalized (fairklaus::kParamThreshold);
		std::printf ("thr nach Frame-Routing (drag+wheel) = %.4f\n", thrFrame);
		expect (thrFrame > 0.2, "Events ueber CFrame-Routing aendern den Parameter");
	}

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

	VSTGUI::exitPlatform ();
	editor->forget ();
	comp->setActive (false);
	comp->terminate ();
	comp->release ();

	if (failures) { std::printf ("GUI SMOKE: %d FAILED\n", failures); return 1; }
	std::printf ("GUI SMOKE: all ok\n");
	return 0;
}
