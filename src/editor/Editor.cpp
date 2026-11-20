//=============================================================================
// fairklaus — Editor "PunishChild 660" Implementierung (siehe Editor.h)
//=============================================================================
#include "Editor.h"
#include "../PunishChild660.h"

#include <cmath>

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
		if (listener) listener->beginEdit (getTag ());
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
		return kMouseEventHandled;
	}
	return kMouseEventNotHandled;
}

CMouseEventResult KnobControl::onMouseUp (CPoint&, const CButtonState&)
{
	if (listener) listener->endEdit (getTag ());
	return kMouseEventHandled;
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
		if (listener) listener->beginEdit (getTag ());
		setValueNormalized (getValueNormalized () > 0.5 ? 0.0 : 1.0);
		valueChanged ();
		if (listener) listener->endEdit (getTag ());
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
PanelView::PanelView (const CRect& r, PunishChild660* p) : CView (r), proc (p)
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

	// Beschriftungen links: Power / Mains
	ctx->setFont (fontSmall);
	ctx->setFontColor (kLabelWhite);
	ctx->drawString ("ON",   CRect (20, 4, 110, 20), kLeftText);
	ctx->drawString ("OFF",  CRect (20, 78, 110, 94), kLeftText);
	ctx->drawString ("60Hz", CRect (16, 140, 120, 156), kLeftText);
	ctx->drawString ("50Hz", CRect (16, 208, 120, 224), kLeftText);
	ctx->drawString ("MAINS", CRect (16, 228, 120, 244), kLeftText);

	// gelbe Lampe unter dem Power-Schalter
	const CPoint lampC (65, 112);
	ctx->setFillColor (lampOn ? kYellow : CColor (80, 76, 40));
	ctx->drawEllipse (CRect (lampC.x - 9, lampC.y - 9, lampC.x + 9, lampC.y + 9), kDrawFilled);
	ctx->setFrameColor (kKnobBlack);
	ctx->setLineWidth (1.5);
	ctx->drawEllipse (CRect (lampC.x - 9, lampC.y - 9, lampC.x + 9, lampC.y + 9), kDrawStroked);

	// Knopf-Skalen + Labels
	drawKnobScale (ctx, 400, 110, 42, "INPUT GAIN", { "0", "4", "8", "12", "16", "20" });
	drawKnobScale (ctx, 550, 110, 42, "THRESHOLD", { "-20", "-10", "0", "+10" });
	drawKnobScale (ctx, 700, 110, 42, "TIME CONSTANT", { "1", "2", "3", "4", "5" });
	drawKnobScale (ctx, 850, 110, 42, "OUTPUT GAIN", { "0", "4", "8", "12", "16", "20" });

	// Titel
	ctx->setFont (fontTitle);
	ctx->setFontColor (kTitleRed);
	ctx->drawString ("PunishChild 660", CRect (0, 236, kPanelW, 290), kCenterText);

	setDirty (false);
}

//------------------------------------------------------------------------
// Editor
//------------------------------------------------------------------------
PanelEditor::PanelEditor (PunishChild660* p) : VSTGUIEditor (p), processor (p) {}

PanelEditor::~PanelEditor ()
{
	if (processor) processor->unregisterEditor (this);
}

bool PLUGIN_API PanelEditor::open (void* parent, const PlatformType& platformType)
{
	(void) parent; (void) platformType;
	CRect r (0, 0, (int32) kPanelW, (int32) kPanelH);
	frame = new CFrame (r, this);

	panel = new PanelView (r, processor);
	frame->addView (panel);

	const double kCy = 110, kRad = 42;
	panel->knobInput     = new KnobControl (CRect (400 - kRad, kCy - kRad, 400 + kRad, kCy + kRad), this, kParamInputGain);
	panel->knobThreshold = new KnobControl (CRect (550 - kRad, kCy - kRad, 550 + kRad, kCy + kRad), this, kParamThreshold);
	panel->knobTimeConst = new KnobControl (CRect (700 - kRad, kCy - kRad, 700 + kRad, kCy + kRad), this, kParamTimeConstant);
	panel->knobOutput    = new KnobControl (CRect (850 - kRad, kCy - kRad, 850 + kRad, kCy + kRad), this, kParamOutputGain);
	panel->swPower = new SwitchControl (CRect (52, 22, 78, 76), this, kParamPower);
	panel->swMains = new SwitchControl (CRect (52, 158, 78, 206), this, kParamMains);
	panel->meter   = new VuMeterView (CRect (120, 25, 312, 225));

	frame->addView (panel->knobInput);
	frame->addView (panel->knobThreshold);
	frame->addView (panel->knobTimeConst);
	frame->addView (panel->knobOutput);
	frame->addView (panel->swPower);
	frame->addView (panel->swMains);
	frame->addView (panel->meter);

	// Anfangswerte aus den Parametern
	auto* ctrl = static_cast<IEditController*> (processor);
	auto sync = [&] (VSTGUI::CControl* c, ParamID id) { if (c) c->setValueNormalized (ctrl->getParamNormalized (id)); };
	sync (panel->knobInput, kParamInputGain);
	sync (panel->knobThreshold, kParamThreshold);
	sync (panel->knobTimeConst, kParamTimeConstant);
	sync (panel->knobOutput, kParamOutputGain);
	sync (panel->swPower, kParamPower);
	sync (panel->swMains, kParamMains);

	return true;
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

void PanelEditor::doIdleStuff ()
{
	if (panel && panel->meter && processor)
	{
		panel->meter->setGainReduction (processor->currentGainReductionDb ());
		panel->meter->setDirty ();
	}
	if (panel)
	{
		auto* ctrl = static_cast<IEditController*> (processor);
		panel->lampOn = ctrl->getParamNormalized (kParamPower) > 0.5;
		panel->setDirty ();
	}
}

void PanelEditor::onParameterChanged (ParamID id, double normalized)
{
	if (!panel) return;
	VSTGUI::CControl* c = nullptr;
	switch (id)
	{
		case kParamInputGain: c = panel->knobInput; break;
		case kParamThreshold: c = panel->knobThreshold; break;
		case kParamTimeConstant: c = panel->knobTimeConst; break;
		case kParamOutputGain: c = panel->knobOutput; break;
		case kParamPower: c = panel->swPower; break;
		case kParamMains: c = panel->swMains; break;
		default: return;
	}
	if (c) c->setValueNormalized ((float) normalized);
}

void PanelEditor::valueChanged (VSTGUI::CControl* pControl)
{
	const int32 tag = pControl->getTag ();
	beginEdit (tag);
	static_cast<IEditController*> (processor)->setParamNormalized (tag, pControl->getValueNormalized ());
	static_cast<IEditController*> (processor)->performEdit (tag);
	endEdit (tag);
}

} // namespace fairklaus
