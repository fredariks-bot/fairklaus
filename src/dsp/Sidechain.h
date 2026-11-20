//=============================================================================
// fairklaus — Sidechain-Verstärker (Steuerverstärker), Paper Fig. 7
//
// Raffensperger, DAFx-12 (2012), Abschnitt 5 — Hybrid WDF/Black-Box:
//   - Eingangstransformator als WDF (Rin=600, Rterm=1360, Rload=152k,
//     Np/Ns=1/17, Rc=50k, Lm=5.7H, sonst Tabelle 2 Seite "Sidechain").
//   - Vpot = phiAC * V_Rload / 2                       (Poti φAC, Mittenanzapfung)
//   - Stufe 1 (Class B, 12AX7), Gl. 10:
//       Vstage1 = ln[(1+exp(Vpot - phi'DC)) / (1+exp(-Vpot - phi'DC))]
//     mit phi'DC = 12.2*(phiDC + 0.1)  (φDC = interne Schraube, hier 0.1)
//   - Stufen 2/3 (12BH7/6973), Verstärkung Av = 8.4, hartes Clipping ±100 V, Gl. 11
//   - Brückengleichrichter (Diodenmodell + Rout = 160 Ohm, Vd = 0.3 V, λ = 10):
//       Vdiff = |Vstage3| - Vsc(t-1)                   (Gl. 13, z^-1 im Rückkopplungszweig)
//       Inom  = (2Vd/(λ*Rout)) * ln(1 + exp(λ*Vdiff/(2Vd) - λ))   (Gl. 12)
//   - Sättigung, Gl. 14 (Imax = 0.5 A):
//       Isc = Inom - (Imax/10) * ln(1 + exp(10*Inom/Imax - 10))
//   - Isc speist das WDF-Zeitkonstanten-Netzwerk (Fig. 8) -> Vsc.
//=============================================================================
#pragma once

#include <cmath>
#include "WdfTransformer.h"

namespace fairklaus {

// numerisch stabiles softplus: ln(1+exp(x))
inline double softplus (double x)
{
	if (x > 40.0) return x;
	if (x < -40.0) return 0.0;
	return std::log1p (std::exp (x));
}

struct Sidechain
{
	static constexpr double R_IN   = 600.0;
	static constexpr double R_TERM = 1360.0;
	static constexpr double R_LOAD = 152e3;
	static constexpr double PHI_DC = 0.1;      // interne DC-Schwelle (Paper Fig. 11)
	static constexpr double AV     = 8.4;      // Stufen 2/3 (Paper Abschnitt 5)
	static constexpr double CLIP   = 100.0;    // Gl. 11
	static constexpr double VD     = 0.3;      // Germanium-Diodenflußspannung
	static constexpr double LAMBDA = 10.0;     // Formparameter Gl. 12
	static constexpr double R_OUT  = 160.0;    // nomineller Ausgangswiderstand
	static constexpr double I_MAX  = 0.5;      // Sättigungsstrom Gl. 14

	Sidechain (double fs)
	    : input (fs, TransformerTable2::sidechainInput (), R_IN, R_TERM, R_LOAD)
	{
		setSampleRate (fs);
	}

	void setSampleRate (double fs) { input.setSampleRate (fs); }

	// phiAC: 0..1 (Threshold-Poti). Rückgabe: Isc (in das Zeitkonstanten-Netzwerk).
	void setPhiAC (double phi) { phiAC = phi < 0.0 ? 0.0 : (phi > 1.0 ? 1.0 : phi); }

	// vSensed = abgetastete Ausgangsspannung Vout; vscDelayed = Vsc(t-1)
	double process (double vSensed, double vscDelayed)
	{
		const double vRload  = input.process (vSensed);
		dbgVRload = vRload;
		const double vpot    = 0.5 * phiAC * vRload;                       // φAC*V_Rload/2
		const double phiDCp  = 12.2 * (PHI_DC + 0.1);                      // φ'DC
		const double vstage1 = softplus (vpot - phiDCp) - softplus (-vpot - phiDCp); // Gl. 10
		double vstage3 = AV * vstage1;                                     // Gl. 11
		if (vstage3 > CLIP) vstage3 = CLIP;
		if (vstage3 < -CLIP) vstage3 = -CLIP;
		const double vdiff = std::fabs (vstage3) - vscDelayed;             // Gl. 13
		const double inom  = (2.0 * VD / (LAMBDA * R_OUT))
		                     * softplus (LAMBDA * vdiff / (2.0 * VD) - LAMBDA); // Gl. 12
		return inom - (I_MAX / 10.0) * softplus (10.0 * inom / I_MAX - 10.0); // Gl. 14
	}

	InputCircuit input;
	double phiAC = 0.0;
	double dbgVRload = 0.0;
};

} // namespace fairklaus
