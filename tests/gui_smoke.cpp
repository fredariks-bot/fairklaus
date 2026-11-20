//=============================================================================
// fairklaus — GUI-Smoke-Test (nur Linux mit Editor-Build, CI via xvfb-run)
//
// Ladet das gebaute .vst3, oeffnet den Editor in einem echten X-Fenster und
// schickt synthetische Maus-Events an das CFrame. Prueft, ob Drag + Mausrad
// am THRESHOLD-Knopf den Parameter aendern und der Power-Schalter toggelt.
//=============================================================================
#include <cstdio>
#include <string>
#include <cmath>
#include <dlfcn.h>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstplugview.h"
#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/events.h"
#include "vstgui/lib/platform/linux/x11platform.h"

#include "Parameters.h"

// X11 kommt danach: dessen Makros (None, Left, ...) kollidieren mit VSTGUI::MouseButton
#include <X11/Xlib.h>
#undef None
#undef Left
#undef Middle
#undef Right
#undef Fourth
#undef Fifth

using namespace Steinberg;
using namespace Steinberg::Vst;

static int failures = 0;
static void expect (bool ok, const char* what)
{
	std::printf ("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) ++failures;
}

int main (int argc, char** argv)
{
	setvbuf (stdout, nullptr, _IONBF, 0);
	if (argc < 2) { std::printf ("usage: gui_smoke <plugin.vst3>\n"); return 2; }

	Display* dpy = XOpenDisplay (nullptr);   // xvfb-run setzt DISPLAY
	expect (dpy != nullptr, "XOpenDisplay (xvfb)");
	if (!dpy) return 1;
	VSTGUI::X11::RunLoop::init ();
	std::printf ("runloop ok\n");
	Window win = XCreateSimpleWindow (dpy, DefaultRootWindow (dpy), 0, 0, 1058, 297, 0, 0, 0);
	XMapWindow (dpy, win);
	XFlush (dpy);

	std::string so = std::string (argv[1]) + "/Contents/x86_64-linux/PunishChild 670.so";
	void* h = dlopen (so.c_str (), RTLD_NOW | RTLD_LOCAL);
	if (!h) { std::printf ("dlopen failed: %s\n", dlerror ()); return 2; }
	auto getFactory = (IPluginFactory* (*)()) dlsym (h, "GetPluginFactory");
	auto* factory = getFactory ();
	PClassInfo ci;
	factory->getClassInfo (0, &ci);
	void* obj = nullptr;
	factory->createInstance (ci.cid, IComponent::iid, &obj);
	auto* comp = static_cast<IComponent*> (obj);
	expect (comp->initialize (nullptr) == kResultOk, "initialize");

	IAudioProcessor* ap = nullptr; comp->queryInterface (IAudioProcessor::iid, (void**) &ap);
	IEditController* ctrl = nullptr; comp->queryInterface (IEditController::iid, (void**) &ctrl);
	ProcessSetup setup { kRealtime, kSample32, 512, 48000.0 };
	ap->setupProcessing (setup);
	expect (comp->setActive (true) == kResultOk, "setActive");

	ctrl->setParamNormalized (fairklaus::kParamPower, 1.0);
	ctrl->setParamNormalized (fairklaus::kParamThreshold, 0.0);
	const double thrBefore = ctrl->getParamNormalized (fairklaus::kParamThreshold);

	IPlugView* view = ctrl->createView (ViewType::kEditor);
	expect (view != nullptr, "createView(Editor) liefert Editor (GUI-Build)");
	if (!view) return 1;
	std::printf ("view ok\n");

	expect (view->attached ((void*) win, kPlatformTypeX11EmbedWindowID) == kResultOk, "attached");
	std::printf ("attached ok\n");

	auto* vstEditor = static_cast<VSTGUIEditor*> (view);
	VSTGUI::CFrame* frame = vstEditor->getFrame ();
	expect (frame != nullptr, "Editor-Frame vorhanden");
	if (!frame) return 1;
	std::printf ("frame ok %gx%g\n", frame->getWidth (), frame->getHeight ());

	// Threshold-Knopf liegt bei (525, 110): Drag nach oben (y kleiner) -> Wert steigt
	{
		VSTGUI::MouseDownEvent down (VSTGUI::CPoint (525, 110), VSTGUI::MouseEventButtonState (VSTGUI::MouseButton::Left));
		frame->onMouseDownEvent (down);
		VSTGUI::MouseMoveEvent move (VSTGUI::CPoint (525, 60), VSTGUI::MouseEventButtonState (VSTGUI::MouseButton::Left));
		frame->onMouseMoveEvent (move);
		VSTGUI::MouseUpEvent up (VSTGUI::CPoint (525, 60), VSTGUI::MouseEventButtonState (VSTGUI::MouseButton::None));
		frame->onMouseUpEvent (up);
	}
	const double thrAfterDrag = ctrl->getParamNormalized (fairklaus::kParamThreshold);
	std::printf ("thr: vorher=%.4f nach Drag=%.4f\n", thrBefore, thrAfterDrag);
	expect (thrAfterDrag > thrBefore + 0.05, "Drag am Threshold-Knopf aendert den Parameter");

	// Mausrad am Threshold-Knopf: deltaY < 0 -> Wert sinkt
	{
		VSTGUI::MouseWheelEvent wheel;
		wheel.mousePosition = VSTGUI::CPoint (525, 110);
		wheel.deltaY = -1.0;
		frame->onMouseWheelEvent (wheel);
	}
	const double thrAfterWheel = ctrl->getParamNormalized (fairklaus::kParamThreshold);
	std::printf ("thr: nach Drag=%.4f nach Wheel=%.4f\n", thrAfterDrag, thrAfterWheel);
	expect (thrAfterWheel < thrAfterDrag - 0.01, "Mausrad am Threshold-Knopf aendert den Parameter");

	// Power-Schalter bei (65, 49) klicken -> toggelt auf 0
	const double powBefore = ctrl->getParamNormalized (fairklaus::kParamPower);
	{
		VSTGUI::MouseDownEvent down (VSTGUI::CPoint (65, 49), VSTGUI::MouseEventButtonState (VSTGUI::MouseButton::Left));
		frame->onMouseDownEvent (down);
		VSTGUI::MouseUpEvent up (VSTGUI::CPoint (65, 49), VSTGUI::MouseEventButtonState (VSTGUI::MouseButton::None));
		frame->onMouseUpEvent (up);
	}
	const double powAfter = ctrl->getParamNormalized (fairklaus::kParamPower);
	std::printf ("power: vorher=%.1f nach Klick=%.1f\n", powBefore, powAfter);
	expect (powAfter != powBefore, "Power-Schalter toggelt");

	std::printf ("events done\n");
	view->removed ();
	view->release ();
	comp->setActive (false);
	comp->terminate ();
	comp->release ();
	XDestroyWindow (dpy, win);
	XCloseDisplay (dpy);
	VSTGUI::X11::RunLoop::exit ();

	if (failures) { std::printf ("GUI SMOKE: %d FAILED\n", failures); return 1; }
	std::printf ("GUI SMOKE: all ok\n");
	return 0;
}
