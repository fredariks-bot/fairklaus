//=============================================================================
// fairklaus — Eine Hälfte (Push oder Pull) des Class-A-Ausgangsverstärkers
//
// Raffensperger, DAFx-12 (2012), Fig. 6 und Gl. 7-9:
//   WDF-Baum mit der nichtlinearen Triode am Stamm (adaptierter Port "root"):
//     root (Serie)  = [ serie_T, paral_K ]      <- Röhrenport (Stamm)
//     serie_T       = Serie[ Ausgangstrafo, Vp ]   Vp = 240 V + R12 (33 Ohm)
//     Trafo.child   = paral_O = Parallel[ Rout (600), Rsc (1k) ]
//     serie_K       = Serie[ Ck (2*C1), DelayFace ]  <- zur anderen Hälfte
//     paral_K       = Parallel[ Vk, serie_K ]      Vk = Vbal (-3.1 V) + R11 (705)
//
//   Implizite Gleichung (Paper Gl. 7/8), gelöst mit Newton-Raphson:
//     Vak + R0 * Ia_total(Vgk, Vak) - a = 0
//     b   = Vak - R0 * Ia_total
//   mit Ia_total = NTI * Ia_6386, NTI = 2 (zwei 6386 pro Hälfte, Paper Anm. zu
//   Gl. 8). Vgk = Vgate - Vk(t-1) (Paper: verzögerte Kathodenspannung).
//   Ausgang: Vout_half = Spannung über Rout = Trafo.vOut() (Paper Gl. 9).
//=============================================================================
#pragma once

#include "Wdf.h"
#include "WdfTransformer.h"
#include "Triode6386.h"

namespace fairklaus {

struct TubeStage : wdf::OnePort
{
	static constexpr double NTI = 2.0;        // zwei 6386-Trioden pro Hälfte
	static constexpr double V_PSU  = 240.0;   // Anodenspannung
	static constexpr double R12    = 33.0;    // Messwiderstand Anode
	static constexpr double V_BAL  = -3.1;    // Balance-Spannung (Paper Fig. 5)
	static constexpr double R11    = 705.0;   // Balance-Widerstand
	static constexpr double C1     = 4e-6;    // Kathoden-Bypass (Paper Fig. 5)
	static constexpr double R_OUT  = 600.0;   // Last 600 Ohm
	static constexpr double R_SC   = 1000.0;  // Sidechain-Eingang 1 kOhm

	TubeStage (double fs, const TransformerParams& outTp)
	    : transfo (fs, outTp), vp (V_PSU, R12), vk (V_BAL, R11), ck (2.0 * C1, fs),
	      rout (R_OUT), rsc (R_SC)
	{
	}

	TubeStage (const TubeStage&) = delete;
	TubeStage& operator= (const TubeStage&) = delete;

	void setSampleRate (double fs)
	{
		ck.set (2.0 * C1, fs);
		transfo.setSampleRate (fs);
		wiring ();
	}

	void setPsuRipple (double phase) { vp.Vs = V_PSU + psuRippleAmp * std::sin (phase); }

	// delayFace: eine Seite der bidirektionalen z^-1 zwischen den beiden 2*C1
	void setDelayFace (wdf::OnePort* f) { delayFace = f; wiring (); }

	void wiring ()
	{
		paralO.connect (&rout, &rsc);
		transfo.connectChild (&paralO);
		serieT.connect (&transfo, &vp);
		serieK.connect (&ck, delayFace);
		paralK.connect (&vk, &serieK);
		root.connect (&serieT, &paralK);
		Rp = root.Rp;
	}

	// vGate = Gitterspannung (Vg1 bzw. Vg2). Rückgabe: V_Rout dieser Hälfte.
	double process (double vGate)
	{
		const double a = root.reflected ();          // Welle der Schaltung in den Röhrenport
		const double vgk = vGate - vkDelayed;        // Gl. 4-6 mit Vk(t-1)
		const double vak = solveTube (a, vgk);       // Newton-Raphson, Gl. 7
		const double iaTot = NTI * Triode6386::Ia (vgk, vak);
		iaLast = iaTot;
		vakLast = vak;
		root.incident (vak - root.Rp * iaTot);       // Gl. 8: b = Vak - R0*Ia
		vkDelayed = paralK.voltage ();               // z^-1 für nächstes Sample
		return transfo.vOut ();                      // V_Rout dieser Hälfte
	}

	double reflected () override { return root.reflected (); }
	void   incident (double w) override { root.incident (w); }
	double R () const override { return root.R (); }
	double G () const override { return root.G (); }

	double iaLast = 0.0;    // für Gain-Reduction-Anzeige
	double vkDelayedPub () const { return vkDelayed; }
	double vakLast = 0.0;
	double psuRippleAmp = 0.0;

private:
	// Gl. 7: F(Vak) = Vak + R0*NTI*Ia(Vgk,Vak) - a = 0 (monoton in Vak)
	double solveTube (double a, double vgk)
	{
		const double R0 = root.Rp;
		double vak = vakLast > 0.0 ? vakLast : 100.0;   // Warmstart
		for (int i = 0; i < 16; ++i)
		{
			const double ia = NTI * Triode6386::Ia (vgk, vak);
			const double F  = vak + R0 * ia - a;
			if (std::fabs (F) < 1e-10 * (1.0 + std::fabs (a))) break;
			double dF = 1.0 + R0 * NTI * Triode6386::dIa_dVak (vgk, vak);
			if (!(dF > 1e-12)) dF = 1.0;
			double step = F / dF;
			if (step > 50.0) step = 50.0;
			if (step < -50.0) step = -50.0;
			vak -= step;
			if (vak < 0.0) vak = 0.0;
			if (vak > 500.0) vak = 500.0;
		}
		return vak;
	}

	NonIdealTransformer transfo;
	wdf::VoltageSource vp, vk;
	wdf::Capacitor ck;
	wdf::Resistor rout, rsc;
	wdf::Serie root, serieT, serieK;
	wdf::Parallel paralO, paralK;
	wdf::OnePort* delayFace = nullptr;
	double vkDelayed = 0.0;
};

} // namespace fairklaus
