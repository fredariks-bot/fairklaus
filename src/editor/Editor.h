//=============================================================================
// fairklaus — Editor "PunishChild 670" (VSTGUI4, programmatisch gezeichnet)
//
// Panel 1058x297 wie das Referenzbild: olivgrüne Frontplatte, roter Schriftzug,
// VU-Meter (Gain Reduction), 4 Drehknöpfe (INPUT GAIN 0..20, THRESHOLD -20..+10,
// TIME CONSTANT 1..5, OUTPUT GAIN 0..20), Power-Schalter mit Lampe.
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

class PunishChild670;

//------------------------------------------------------------------------
// Drehknopf (schwarz, gelber Zeiger), vertikales Ziehen
//------------------------------------------------------------------------
class KnobControl : public VSTGUI::CControl
{
public:
	KnobControl (const VSTGUI::CRect& r, VSTGUI::IControlListener* listener, Steinberg::int32 tag);
	void setDetents (int n) { detents = n; }   // n Stufen -> rastet beim Ziehen ein (z.B. 5 fuer Time Constant)
	void draw (VSTGUI::CDrawContext* ctx) override;
	VSTGUI::CMouseEventResult onMouseDown (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
	VSTGUI::CMouseEventResult onMouseMoved (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
	VSTGUI::CMouseEventResult onMouseUp (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;
	CLASS_METHODS_NOCOPY (KnobControl, VSTGUI::CControl)
private:
	double snap (double v) const
	{
		if (detents > 1) v = std::round (v * (detents - 1)) / (detents - 1);
		return v;
	}
	double lastY = 0.0;
	int detents = 0;
};

//------------------------------------------------------------------------
// OS-Umschalter (1x/2x/4x/8x): Klick zyklisch weiterschalten
//------------------------------------------------------------------------
class OsSelector : public VSTGUI::CControl
{
public:
	OsSelector (const VSTGUI::CRect& r, VSTGUI::IControlListener* listener, Steinberg::int32 tag);
	void draw (VSTGUI::CDrawContext* ctx) override;
	VSTGUI::CMouseEventResult onMouseDown (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
	CLASS_METHODS_NOCOPY (OsSelector, VSTGUI::CControl)
};

//------------------------------------------------------------------------
// Zweistufen-Schalter (Power ON/OFF)
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
	PanelView (const VSTGUI::CRect& r, PunishChild670* proc);
	void draw (VSTGUI::CDrawContext* ctx) override;

	KnobControl* knobInput = nullptr;
	KnobControl* knobThreshold = nullptr;
	KnobControl* knobTimeConst = nullptr;
	KnobControl* knobOutput = nullptr;
	KnobControl* knobTrim = nullptr;
	SwitchControl* swPower = nullptr;
	VSTGUI::CControl* osSel = nullptr;
	VuMeterView* meter = nullptr;
	bool lampOn = true;              // gelbe Lampe (Power an)

private:
	void drawKnobScale (VSTGUI::CDrawContext* ctx, double cx, double cy, double r,
	                    const char* label, const std::vector<std::string>& marks);
	PunishChild670* proc;
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
	explicit PanelEditor (PunishChild670* processor);
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

	PunishChild670* processor;
	PanelView* panel = nullptr;
};

} // namespace fairklaus
