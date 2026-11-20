//=============================================================================
// fairklaus — Editor "PunishChild 660" (VSTGUI4, programmatisch gezeichnet)
//
// Panel 1058x297 wie das Referenzbild: olivgrüne Frontplatte, roter Schriftzug,
// VU-Meter (Gain Reduction), 4 Drehknöpfe (INPUT GAIN 0..20, THRESHOLD -20..+10,
// TIME CONSTANT 1..5, OUTPUT GAIN 0..20), Power-Schalter mit Lampe, MAINS 50/60Hz.
//
// Wird nur kompiliert, wenn FAIRKLAUS_WITH_EDITOR definiert ist (GUI-Deps
// vorhanden; auf Linux: X11 + cairo + freetype + fontconfig).
//=============================================================================
#pragma once

#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/vstgui.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/ccolor.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cstring.h"
#include "vstgui/lib/cgraphicspath.h"

#include "../Parameters.h"

namespace fairklaus {

class PunishChild660;

//------------------------------------------------------------------------
// Drehknopf (schwarz, gelber Zeiger), vertikales Ziehen
//------------------------------------------------------------------------
class KnobControl : public VSTGUI::CControl
{
public:
	KnobControl (const VSTGUI::CRect& r, VSTGUI::IControlListener* listener, Steinberg::int32 tag);
	void draw (VSTGUI::CDrawContext* ctx) override;
	VSTGUI::CMouseEventResult onMouseDown (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
	VSTGUI::CMouseEventResult onMouseMoved (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
	VSTGUI::CMouseEventResult onMouseUp (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
	CLASS_METHODS_NOCOPY (KnobControl, VSTGUI::CControl)
private:
	double lastY = 0.0;
};

//------------------------------------------------------------------------
// Zweistufen-Schalter (Power ON/OFF, Mains 60/50 Hz)
//------------------------------------------------------------------------
class SwitchControl : public VSTGUI::CControl
{
public:
	SwitchControl (const VSTGUI::CRect& r, VSTGUI::IControlListener* listener, Steinberg::int32 tag);
	void draw (VSTGUI::CDrawContext* ctx) override;
	VSTGUI::CMouseEventResult onMouseDown (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
	CLASS_METHODS_NOCOPY (SwitchControl, VSTGUI::CControl)
};

//------------------------------------------------------------------------
// VU-Meter: zeigt Gain Reduction (0..20 dB) auf Skala -20..+3
//------------------------------------------------------------------------
class VuMeterView : public VSTGUI::CView
{
public:
	explicit VuMeterView (const VSTGUI::CRect& r);
	void draw (VSTGUI::CDrawContext* ctx) override;
	void setGainReduction (double grDb) { gr = grDb < 0 ? 0 : (grDb > 24 ? 24 : grDb); }
private:
	double gr = 0.0;
};

//------------------------------------------------------------------------
// Haupt-Panel: zeichnet Frontplatte + alle Beschriftungen, hält die Controls
//------------------------------------------------------------------------
class PanelView : public VSTGUI::CView
{
public:
	PanelView (const VSTGUI::CRect& r, PunishChild660* proc);
	void draw (VSTGUI::CDrawContext* ctx) override;

	KnobControl* knobInput = nullptr;
	KnobControl* knobThreshold = nullptr;
	KnobControl* knobTimeConst = nullptr;
	KnobControl* knobOutput = nullptr;
	SwitchControl* swPower = nullptr;
	SwitchControl* swMains = nullptr;
	VuMeterView* meter = nullptr;
	bool lampOn = true;              // gelbe Lampe (Power an)

private:
	void drawKnobScale (VSTGUI::CDrawContext* ctx, double cx, double cy, double r,
	                    const char* label, const std::vector<std::string>& marks);
	PunishChild660* proc;
	VSTGUI::SharedPointer<VSTGUI::CFontDesc> fontTitle;
	VSTGUI::SharedPointer<VSTGUI::CFontDesc> fontLabel;
	VSTGUI::SharedPointer<VSTGUI::CFontDesc> fontSmall;
};

//------------------------------------------------------------------------
// Der Editor (IPlugView) mit Parameter-Binding
//------------------------------------------------------------------------
class PanelEditor : public Steinberg::Vst::VSTGUIEditor,
                    public VSTGUI::IControlListener
{
public:
	explicit PanelEditor (PunishChild660* processor);
	~PanelEditor () override;

	bool PLUGIN_API open (void* parent, const VSTGUI::PlatformType& platformType) override;
	void PLUGIN_API close () override;

	// skalierbare Oberflaeche: Host darf Fenster Groesse aendern, wir zoomen das Panel
	Steinberg::tresult PLUGIN_API canResize () override;
	Steinberg::tresult PLUGIN_API onSize (Steinberg::ViewRect* newSize) override;
	VSTGUI::CMessageResult notify (VSTGUI::CBaseObject* sender, const char* message) override;

	// IControlListener
	void valueChanged (VSTGUI::CControl* pControl) override;

private:
	void pollParameters ();

	PunishChild660* processor;
	PanelView* panel = nullptr;
};

} // namespace fairklaus
