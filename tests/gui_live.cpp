//=============================================================================
// fairklaus — GUI-Live-Test: Editor in einem ECHTEN X11-Fenster attachen,
// resizen (Zoom), detachen, wieder attachen, freigeben (xvfb-run in der CI).
// Testet die Lifetime-Pfade, die in einem Host (Reaper) abstuerzen koennen:
// attached -> open -> onSize/setZoom -> removed -> close -> re-attached.
// Aufruf: xvfb-run -a gui_live
//=============================================================================
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <dlfcn.h>

// X11 zuerst, dann die kollidierenden Makros entfernen, bevor VSTGUI kommt
#include <X11/Xlib.h>
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef Status
#undef Status
#endif
#ifdef True
#undef True
#endif
#ifdef False
#undef False
#endif

#include "Parameters.h"
#include "PunishChild670.h"     // zuerst: sonst kollidiert das setState-Makro
#include "editor/Editor.h"
#include "vstgui/lib/platform/platformfactory.h"

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstplugview.h"

// sonst aus linuxmain.cpp des Plugins; hier selbst definieren (gui_live ist kein Modul)
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
	VSTGUI::initPlatform ((VSTGUI::PlatformInstanceHandle) dlopen (nullptr, RTLD_LAZY));

	Display* dpy = XOpenDisplay (nullptr);
	expect (dpy != nullptr, "XOpenDisplay (xvfb)");
	if (!dpy) return 2;
	Window win = XCreateSimpleWindow (dpy, DefaultRootWindow (dpy), 0, 0, 1058, 297, 0,
	                                  BlackPixel (dpy, 0), BlackPixel (dpy, 0));
	XMapWindow (dpy, win);
	XFlush (dpy);

	auto* comp = static_cast<IComponent*> (new fairklaus::PunishChild670 ());
	expect (comp->initialize (nullptr) == kResultOk, "initialize");
	IAudioProcessor* ap = nullptr; comp->queryInterface (IAudioProcessor::iid, (void**) &ap);
	IEditController* ctrl = nullptr; comp->queryInterface (IEditController::iid, (void**) &ctrl);
	ProcessSetup setup { kRealtime, kSample32, 512, 48000.0 };
	ap->setupProcessing (setup);
	expect (comp->setActive (true) == kResultOk, "setActive");
	ctrl->setParamNormalized (fairklaus::kParamPower, 1.0);

	IPlugView* view = ctrl->createView (ViewType::kEditor);
	expect (view != nullptr, "createView(Editor) liefert View");
	if (view)
	{
		expect (view->isPlatformTypeSupported (Steinberg::kPlatformTypeX11EmbedWindowID) == kResultTrue,
		        "X11EmbedWindowID unterstuetzt");
		expect (view->attached ((void*) (uintptr_t) win, Steinberg::kPlatformTypeX11EmbedWindowID) == kResultTrue,
		        "attached (Editor in X11-Fenster geoeffnet)");
		ViewRect sz {};
		view->getSize (&sz);
		std::printf ("size nach attached = %d x %d (erwartet ~1058 x 297)\n",
		             (int) (sz.right - sz.left), (int) (sz.bottom - sz.top));
		expect (std::fabs ((sz.right - sz.left) - 1058) < 4 && std::fabs ((sz.bottom - sz.top) - 297) < 4,
		        "Anfangsgroesse 1058x297");

		expect (view->canResize () == kResultTrue, "canResize");
		ViewRect big (0, 0, 2116, 594);   // 2x
		view->onSize (&big);
		view->getSize (&sz);
		std::printf ("size nach onSize(2x) = %d x %d (erwartet ~2116 x 594)\n",
		             (int) (sz.right - sz.left), (int) (sz.bottom - sz.top));
		expect (std::fabs ((sz.right - sz.left) - 2116) < 8, "onSize zoomt auf 2x");

		ViewRect small (0, 0, 400, 200);  // < 0.5x -> clamp auf 0.5x = 529x149
		view->onSize (&small);
		view->getSize (&sz);
		std::printf ("size nach onSize(400px) = %d x %d (erwartet ~529 x 149, Zoom-Clamp 0.5)\n",
		             (int) (sz.right - sz.left), (int) (sz.bottom - sz.top));
		expect (std::fabs ((sz.right - sz.left) - 529) < 8, "onSize clamped Zoom auf 0.5x");

		ViewRect orig (0, 0, 1058, 297);
		view->onSize (&orig);

		// Reaper oeffnet/schliesst Fenster ggf. mehrfach -> re-attach testen
		expect (view->removed () == kResultTrue, "removed (close)");
		expect (view->attached ((void*) (uintptr_t) win, Steinberg::kPlatformTypeX11EmbedWindowID) == kResultTrue,
		        "re-attached nach removed");
		view->getSize (&sz);
		expect (std::fabs ((sz.right - sz.left) - 1058) < 4, "nach re-attach wieder 1058 breit");
		expect (view->removed () == kResultTrue, "removed #2");
		view->release ();
	}

	XDestroyWindow (dpy, win);
	XCloseDisplay (dpy);
	VSTGUI::exitPlatform ();
	comp->setActive (false);
	comp->terminate ();
	comp->release ();

	if (failures) { std::printf ("GUI LIVE: %d FAILED\n", failures); return 1; }
	std::printf ("GUI LIVE: all ok\n");
	return 0;
}
