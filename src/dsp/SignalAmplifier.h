//=============================================================================
// fairklaus — Signalverstärker eines Kanals (Fairchild 670), Paper Fig. 5
//
// Raffensperger, DAFx-12 (2012), Abschnitt 4:
//   - Eingang: Vin (mit Rin) -> Abschluss Rtr -> Eingangstransformator (T101,
//     Tabelle 2) -> Sekundär -> RL1 = 100 kOhm. Gitterspannung:
//       Vg1 = V_RL1 + Vx,  Vg2 = -V_RL1 + Vx        (Gl. 4-6, gegenphasig)
//     mit Vx(t) = Vsc(t-1) + Vbias, Vbias = -7.2 V ("zero"-Poti, konstant).
//     (Vorzeichen: die Sidechain liefert eine positive Steuerspannung Vsc >= 0,
//      die das Gitter negativer macht -> Remote-Cutoff -> weniger Verstärkung.)
//   - Push/Pull-Hälften: je eine TubeStage (Fig. 6); die beiden 2*C1
//     Kathodenkondensatoren sind über eine bidirektionale z^-1 gekoppelt.
//   - Ausgang: Vout = Vpush_Rout - Vpull_Rout       (Gl. 9)
//=============================================================================
#pragma once

#include "Wdf.h"
#include "WdfTransformer.h"
#include "TubeStage.h"

namespace fairklaus {

struct SignalAmplifier
{
	static constexpr double V_BIAS = -7.2;   // Vbias (Paper Abschnitt 4)
	static constexpr double R_IN   = 600.0;  // Quellenwiderstand
	static constexpr double R_TERM = 1360.0; // Eingang-Abschlusswiderstand
	static constexpr double R_L    = 100e3;  // RL1 = RL2 = 100 kOhm (Paper)

	SignalAmplifier (double fs)
	    : input (fs, TransformerTable2::signalInput (), R_IN, R_TERM, R_L),
	      push (fs, TransformerTable2::signalOutput ()),
	      pull (fs, TransformerTable2::signalOutput ()),
	      cathodeLink (1.0)
	{
		push.setDelayFace (&cathodeLink.a);
		pull.setDelayFace (&cathodeLink.b);
		setSampleRate (fs);
	}

	// self-referential pointers (delayFace) -> nicht kopieren
	SignalAmplifier (const SignalAmplifier&) = delete;
	SignalAmplifier& operator= (const SignalAmplifier&) = delete;

	void setSampleRate (double fs)
	{
		input.setSampleRate (fs);
		push.setSampleRate (fs);
		pull.setSampleRate (fs);
		vCtrlDelayed = 0.0;
		vCtrlPrev = 0.0;
	}

	// vCtrl = Vsc (Sidechain-Steuerspannung, >= 0). Rückgabe: Vout (über Rout).
	double process (double vIn, double vCtrl)
	{
		vCtrlDelayed = vCtrlPrev;                 // z^-1: Vx(t) = Vsc(t-1) + Vbias
		vCtrlPrev = vCtrl;
		const double vx = V_BIAS - vCtrlDelayed;
		const double vRL1 = input.process (vIn);   // V_RL1 (Gittervorspannung vom Trafo)
		const double vPush = push.process (vRL1 + vx);   // Gl. 4
		const double vPull = pull.process (-vRL1 + vx);  // Gl. 6 (gegenphasig)
		vOut = vPush - vPull;                      // Gl. 9
		return vOut;
	}

	// Gain-Reduction-Schätzung aus dem Anodenstrom der Push-Röhre:
	// Verstärkung ~ gm ~ Ia^(1/3)  ->  GR = (20/3) * log10(Ia_rest / Ia_aktuell)
	double gainReductionDb (double iaRest) const
	{
		const double ia = std::max (push.iaLast, 1e-9);
		const double r = std::max (iaRest, 1e-9);
		double gr = (20.0 / 3.0) * std::log10 (r / ia);
		if (!(gr > 0.0)) gr = 0.0;
		if (gr > 60.0) gr = 60.0;
		return gr;
	}

	InputCircuit input;
	TubeStage push, pull;
	wdf::BidirectionalDelay cathodeLink;
	double vOut = 0.0;

private:
	double vCtrlDelayed = 0.0;
	double vCtrlPrev = 0.0;
};

} // namespace fairklaus
