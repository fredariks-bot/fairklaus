//=============================================================================
// fairklaus — Editor "PunishChild 670" Implementierung (siehe Editor.h)
//=============================================================================
#include "Editor.h"
#include "../PunishChild670.h"
#include "vstgui/lib/cvstguitimer.h"

#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace VSTGUI;

namespace fairklaus {

static const double kPanelW = 1058.0;
static const double kPanelH = 297.0;

// Farben (Referenzbild)
static const CColor kOliveTop    {  88,  82,  50 };
static const CColor kOliveBottom {  46,  43,  28 };
static const CColor kKnobBlack   {  22,  22,  22 };
static const CColor kKnobRim     { 120, 118, 110 };
static const CColor kYellow      { 255, 224,  80 };
static const CColor kLabelWhite  { 238, 236, 224 };
static const CColor kTitleRed    { 198,  34,  22 };
static const CColor kMeterFace   { 244, 240, 214 };
static const CColor kMeterInk    {  30,  30,  30 };
static const CColor kNeedleRed   { 214,  38,  28 };
static const CColor kSlotDark    {  12,  12,  12 };

static double deg2rad (double d) { return d * M_PI / 180.0; }

// Knopfwinkel: Wert 0..1 -> 135°..405° (7:30 bis 4:30 Uhr)
static double knobAngle (double v) { return deg2rad (135.0 + 270.0 * v); }

//------------------------------------------------------------------------
// Knopf
//------------------------------------------------------------------------
KnobControl::KnobControl (const CRect& r, IControlListener* listener, int32 tag)
    : CControl (r, listener, tag)
{
	setDefaultValue (0.0);
}

void KnobControl::draw (CDrawContext* ctx)
{
	const CRect r = getViewSize ();
	const CPoint c (r.getCenter ());
	const double rad = r.getWidth () / 2.0;

	// Schatten
	ctx->setFillColor (CColor (0, 0, 0, 90));
	ctx->drawEllipse (CRect (c.x - rad + 2, c.y - rad + 3, c.x + rad + 2, c.y + rad + 3), kDrawFilled);
	// Körper
	ctx->setFillColor (kKnobBlack);
	ctx->drawEllipse (r, kDrawFilled);
	ctx->setFrameColor (kKnobRim);
	ctx->setLineWidth (1.5);
	ctx->drawEllipse (r, kDrawStroked);
	// Griffmulde
	ctx->setFrameColor (CColor (60, 60, 60));
	ctx->setLineWidth (1.0);
	ctx->drawEllipse (CRect (c.x - rad * 0.72, c.y - rad * 0.72, c.x + rad * 0.72, c.y + rad * 0.72), kDrawStroked);
	// Zeiger
	const double a = knobAngle (getValueNormalized ());
	const CPoint tip (c.x + std::cos (a) * rad * 0.82, c.y - std::sin (a) * rad * 0.82);
	ctx->setFrameColor (kYellow);
	ctx->setLineWidth (3.0);
	ctx->drawLine (c, tip);

	setDirty (false);
}

CMouseEventResult KnobControl::onMouseDown (CPoint& where, const CButtonState& buttons)
{
	if (buttons.isLeftButton ())
	{
		lastY = where.y;
		beginEdit ();
		return kMouseEventHandled;
	}
	return kMouseEventHandled;
}

CMouseEventResult KnobControl::onMouseMoved (CPoint& where, const CButtonState& buttons)
{
	if (buttons.isLeftButton ())
	{
		const double dy = lastY - where.y;
		lastY = where.y;
		setValueNormalized (getValueNormalized () + dy * 0.005);
		valueChanged ();
		setDirty (true);
		return kMouseEventHandled;
	}
	return kMouseEventNotHandled;
}

CMouseEventResult KnobControl::onMouseUp (CPoint&, const CButtonState&)
{
	endEdit ();
	return kMouseEventHandled;
}

void KnobControl::onMouseWheelEvent (MouseWheelEvent& event)
{
	if (event.deltaY != 0.0)
	{
		beginEdit ();
		setValueNormalized (getValueNormalized () + (float) event.deltaY * 0.05f);
		valueChanged ();
		setDirty (true);
		endEdit ();
		event.consumed = true;
	}
}

//------------------------------------------------------------------------
// Schalter
//------------------------------------------------------------------------
SwitchControl::SwitchControl (const CRect& r, IControlListener* listener, int32 tag)
    : CControl (r, listener, tag)
{
	setDefaultValue (1.0);
}

void SwitchControl::draw (CDrawContext* ctx)
{
	const CRect r = getViewSize ();
	// Slot
	ctx->setFillColor (kSlotDark);
	ctx->setFrameColor (kKnobRim);
	ctx->setLineWidth (1.0);
	CRect slot (r.left + r.getWidth () / 2 - 7, r.top + 4, r.right - (r.getWidth () / 2 - 7), r.bottom - 4);
	ctx->drawRect (slot, kDrawFilledAndStroked);
	// Hebel
	const bool up = getValueNormalized () > 0.5;
	CRect lever (slot.left - 4, up ? slot.top - 2 : slot.bottom - 10, slot.right + 4,
	              up ? slot.top + 10 : slot.bottom + 2);
	ctx->setFillColor (CColor (200, 196, 184));
	ctx->setFrameColor (CColor (60, 60, 60));
	ctx->drawRect (lever, kDrawFilledAndStroked);
	setDirty (false);
}

CMouseEventResult SwitchControl::onMouseDown (CPoint&, const CButtonState& buttons)
{
	if (buttons.isLeftButton ())
	{
		beginEdit ();
		setValueNormalized (getValueNormalized () > 0.5 ? 0.0 : 1.0);
		valueChanged ();
		endEdit ();
		return kMouseEventHandled;
	}
	return kMouseEventHandled;
}

//------------------------------------------------------------------------
// VU-Meter
//------------------------------------------------------------------------
VuMeterView::VuMeterView (const CRect& r) : CView (r) {}

void VuMeterView::draw (CDrawContext* ctx)
{
	const CRect r = getViewSize ();
	// Zifferblatt
	ctx->setFillColor (kMeterFace);
	ctx->setFrameColor (kKnobBlack);
	ctx->setLineWidth (2.0);
	ctx->drawRect (r, kDrawFilledAndStroked);

	static CFontDesc fontSmall ("Arial", 11);
	static CFontDesc fontVu ("Arial", 13, kBoldFace);

	const CPoint pivot (r.getCenter ().x, r.bottom - 28);
	const double R = r.getWidth () / 2.0 - 18;
	// Skala: Wert v in [-20, +3] -> Winkel 150° (links) bis 30° (rechts)
	auto angleFor = [&] (double v) { return deg2rad (150.0 - (v + 20.0) / 23.0 * 120.0); };

	// Skalenstriche + Zahlen
	const double marks[] = { -20, -10, -5, 0, 3 };
	const char* labels[] = { "-20", "-10", "-5", "0", "+3" };
	ctx->setFrameColor (kMeterInk);
	ctx->setLineWidth (1.2);
	ctx->setFont (&fontSmall);
	ctx->setFontColor (kMeterInk);
	for (int i = 0; i < 5; ++i)
	{
		const double a = angleFor (marks[i]);
		const CPoint p1 (pivot.x + std::cos (a) * (R - 6), pivot.y - std::sin (a) * (R - 6));
		const CPoint p2 (pivot.x + std::cos (a) * R, pivot.y - std::sin (a) * R);
		ctx->drawLine (p1, p2);
		const CPoint lp (pivot.x + std::cos (a) * (R + 12), pivot.y - std::sin (a) * (R + 12));
		CRect lr (lp.x - 14, lp.y - 8, lp.x + 14, lp.y + 8);
		ctx->drawString (labels[i], lr, kCenterText);
	}
	ctx->setFont (&fontVu);
	ctx->drawString ("VU", CRect (r.left + 8, r.bottom - 26, r.right - 8, r.bottom - 8), kCenterText);

	// Nadel: GR 0..20 dB -> Skalenwert 0..-20
	const double v = -gr;
	const double a = angleFor (v);
	const CPoint tip (pivot.x + std::cos (a) * (R - 4), pivot.y - std::sin (a) * (R - 4));
	ctx->setFrameColor (kNeedleRed);
	ctx->setLineWidth (2.0);
	ctx->drawLine (pivot, tip);
	ctx->setFillColor (kMeterInk);
	ctx->drawEllipse (CRect (pivot.x - 4, pivot.y - 4, pivot.x + 4, pivot.y + 4), kDrawFilled);

	setDirty (false);
}

//------------------------------------------------------------------------
// Panel
//------------------------------------------------------------------------
PanelView::PanelView (const CRect& r, PunishChild670* p) : CView (r), proc (p)
{
	fontTitle = new CFontDesc ("Arial", 34, kBoldFace);
	fontLabel = new CFontDesc ("Arial", 13, kBoldFace);
	fontSmall = new CFontDesc ("Arial", 11);
}

void PanelView::drawKnobScale (CDrawContext* ctx, double cx, double cy, double rad,
                               const char* label, const std::vector<std::string>& marks)
{
	ctx->setFrameColor (kYellow);
	ctx->setLineWidth (1.4);
	ctx->setFont (fontSmall);
	ctx->setFontColor (kLabelWhite);
	for (size_t i = 0; i < marks.size (); ++i)
	{
		const double v = (double) i / (double) (marks.size () - 1);
		const double a = knobAngle (v);
		const CPoint p1 (cx + std::cos (a) * (rad + 5), cy - std::sin (a) * (rad + 5));
		const CPoint p2 (cx + std::cos (a) * (rad + 11), cy - std::sin (a) * (rad + 11));
		ctx->drawLine (p1, p2);
		const CPoint lp (cx + std::cos (a) * (rad + 24), cy - std::sin (a) * (rad + 24));
		CRect lr (lp.x - 16, lp.y - 8, lp.x + 16, lp.y + 8);
		ctx->drawString (marks[i].c_str (), lr, kCenterText);
	}
	ctx->setFont (fontLabel);
	ctx->setFontColor (kYellow);
	CRect lr (cx - 60, cy + rad + 30, cx + 60, cy + rad + 48);
	ctx->drawString (label, lr, kCenterText);
}

void PanelView::draw (CDrawContext* ctx)
{
	const CRect r = getViewSize ();
	// olivgrüner Hintergrund (oben heller, unten dunkler)
	ctx->setFillColor (kOliveTop);
	ctx->drawRect (r, kDrawFilled);
	ctx->setFillColor (kOliveBottom);
	ctx->drawRect (CRect (r.left, r.top + r.getHeight () * 0.45, r.right, r.bottom), kDrawFilled);
	// dezente Vignette
	ctx->setFrameColor (CColor (0, 0, 0, 60));
	ctx->setLineWidth (2.0);
	ctx->drawRect (r, kDrawStroked);
	// Schrauben in den Ecken
	ctx->setFillColor (CColor (150, 148, 138));
	for (auto [sx, sy] : { std::make_pair (14.0, 14.0), std::make_pair (kPanelW - 14.0, 14.0),
	                       std::make_pair (14.0, kPanelH - 14.0), std::make_pair (kPanelW - 14.0, kPanelH - 14.0) })
	{
		ctx->drawEllipse (CRect (sx - 6, sy - 6, sx + 6, sy + 6), kDrawFilled);
		ctx->setFrameColor (CColor (60, 58, 50));
		ctx->drawLine (CPoint (sx - 4, sy), CPoint (sx + 4, sy));
		ctx->setFrameColor (CColor (150, 148, 138));
	}

	// Beschriftung links: Power
	ctx->setFont (fontSmall);
	ctx->setFontColor (kLabelWhite);
	ctx->drawString ("ON",   CRect (20, 4, 110, 20), kLeftText);
	ctx->drawString ("OFF",  CRect (20, 78, 110, 94), kLeftText);

	// gelbe Lampe unter dem Power-Schalter
	const CPoint lampC (65, 112);
	ctx->setFillColor (lampOn ? kYellow : CColor (80, 76, 40));
	ctx->drawEllipse (CRect (lampC.x - 9, lampC.y - 9, lampC.x + 9, lampC.y + 9), kDrawFilled);
	ctx->setFrameColor (kKnobBlack);
	ctx->setLineWidth (1.5);
	ctx->drawEllipse (CRect (lampC.x - 9, lampC.y - 9, lampC.x + 9, lampC.y + 9), kDrawStroked);

	// Knopf-Skalen + Labels
	drawKnobScale (ctx, 390, 110, 40, "INPUT GAIN", { "0", "4", "8", "12", "16", "20" });
	drawKnobScale (ctx, 525, 110, 40, "THRESHOLD", { "-20", "-10", "0", "+10" });
	drawKnobScale (ctx, 660, 110, 40, "TIME CONSTANT", { "1", "2", "3", "4", "5" });
	drawKnobScale (ctx, 795, 110, 40, "OUTPUT GAIN", { "0", "4", "8", "12", "16", "20" });
	drawKnobScale (ctx, 945, 110, 40, "TRIM", { "-24", "-18", "-12", "-6", "0" });

	// Titel
	ctx->setFont (fontTitle);
	ctx->setFontColor (kTitleRed);
	ctx->drawString ("PunishChild 670", CRect (0, 236, kPanelW, 290), kCenterText);

	setDirty (false);
}

//------------------------------------------------------------------------
// Editor
//------------------------------------------------------------------------
PanelEditor::PanelEditor (PunishChild670* p) : VSTGUIEditor (p), processor (p) {}

PanelEditor::~PanelEditor ()
{
	if (processor) processor->unregisterEditor (this);
}

bool PLUGIN_API PanelEditor::open (void* parent, const PlatformType& platformType)
{
	if (frame)
		return true;
	CRect r (0, 0, (int32) kPanelW, (int32) kPanelH);
	frame = new CFrame (r, this);

	panel = new PanelView (r, processor);
	frame->addView (panel);

	const double kCy = 110, kRad = 40;
	panel->knobInput     = new KnobControl (CRect (390 - kRad, kCy - kRad, 400 + kRad, kCy + kRad), this, kParamInputGain);
	panel->knobThreshold = new KnobControl (CRect (525 - kRad, kCy - kRad, 550 + kRad, kCy + kRad), this, kParamThreshold);
	panel->knobTimeConst = new KnobControl (CRect (660 - kRad, kCy - kRad, 700 + kRad, kCy + kRad), this, kParamTimeConstant);
	panel->knobOutput    = new KnobControl (CRect (795 - kRad, kCy - kRad, 850 + kRad, kCy + kRad), this, kParamOutputGain);
	panel->knobTrim      = new KnobControl (CRect (945 - kRad, kCy - kRad, 945 + kRad, kCy + kRad), this, kParamTrim);
	panel->swPower = new SwitchControl (CRect (52, 22, 78, 76), this, kParamPower);
	panel->meter   = new VuMeterView (CRect (120, 25, 312, 225));

	frame->addView (panel->knobInput);
	frame->addView (panel->knobThreshold);
	frame->addView (panel->knobTimeConst);
	frame->addView (panel->knobOutput);
	frame->addView (panel->knobTrim);
	frame->addView (panel->swPower);
	frame->addView (panel->meter);

	// Anfangswerte aus den Parametern
	auto* ctrl = static_cast<IEditController*> (processor);
	auto sync = [&] (VSTGUI::CControl* c, ParamID id) { if (c) c->setValueNormalized (ctrl->getParamNormalized (id)); };
	sync (panel->knobInput, kParamInputGain);
	sync (panel->knobThreshold, kParamThreshold);
	sync (panel->knobTimeConst, kParamTimeConstant);
	sync (panel->knobOutput, kParamOutputGain);
	sync (panel->knobTrim, kParamTrim);
	sync (panel->swPower, kParamPower);

	if (!frame->open (parent, platformType))
	{
		frame->forget ();
		frame = nullptr;
		return false;
	}
	return true;
}

tresult PLUGIN_API PanelEditor::canResize ()
{
	return kResultTrue;
}

tresult PLUGIN_API PanelEditor::onSize (Steinberg::ViewRect* newSize)
{
	if (frame && newSize)
	{
		double z = (newSize->right - newSize->left) / kPanelW;
		if (z < 0.5) z = 0.5;
		if (z > 4.0) z = 4.0;
		frame->setZoom (z);
		newSize->right = newSize->left + (int32) (kPanelW * z + 0.5);
		newSize->bottom = newSize->top + (int32) (kPanelH * z + 0.5);
	}
	return VSTGUIEditor::onSize (newSize);
}

void PLUGIN_API PanelEditor::close ()
{
	if (processor) processor->unregisterEditor (this);
	if (frame)
	{
		frame->close ();
		frame->forget ();
		frame = nullptr;
	}
}

VSTGUI::CMessageResult PanelEditor::notify (VSTGUI::CBaseObject* sender, const char* message)
{
	if (message == CVSTGUITimer::kMsgTimer)
		pollParameters ();
	return VSTGUIEditor::notify (sender, message);
}

void PanelEditor::pollParameters ()
{
	if (panel && panel->meter && processor)
	{
		panel->meter->setGainReduction (processor->currentGainReductionDb ());
		panel->meter->setDirty ();
	}
	if (panel)
	{
		auto* ctrl = static_cast<EditController*> (processor);
		panel->lampOn = ctrl->getParamNormalized (kParamPower) > 0.5;
		auto sync = [&] (VSTGUI::CControl* c, Steinberg::Vst::ParamID id)
		{
			if (c)
			{
				const auto v = (float) ctrl->getParamNormalized (id);
				if (v != c->getValueNormalized ()) c->setValueNormalized (v);
			}
		};
		sync (panel->knobInput, kParamInputGain);
		sync (panel->knobThreshold, kParamThreshold);
		sync (panel->knobTimeConst, kParamTimeConstant);
		sync (panel->knobOutput, kParamOutputGain);
	sync (panel->knobTrim, kParamTrim);
		sync (panel->swPower, kParamPower);
			panel->setDirty ();
	}
}

void PanelEditor::valueChanged (VSTGUI::CControl* pControl)
{
	const auto tag = (Steinberg::Vst::ParamID) pControl->getTag ();
	auto* ec = static_cast<EditController*> (processor);
	ec->beginEdit (tag);
	ec->setParamNormalized (tag, pControl->getValueNormalized ());
	ec->performEdit (tag, ec->getParamNormalized (tag));
	ec->endEdit (tag);
}

} // namespace fairklaus
