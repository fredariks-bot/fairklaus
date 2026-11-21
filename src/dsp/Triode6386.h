//=============================================================================
// fairklaus — GE 6386 "remote cutoff" Triode, Black-Box-Modell
//
// Raffensperger, DAFx-12 (2012), Gl. (1) und Tabelle 1:
//   Ia = p1 * Vak^p2 / ( (p3 - p4*Vgk)^p5 * (p6 + exp(p7*Vak - p8*Vgk)) )
//
// Parameter per Levenberg-Marquardt an das GE-6386-Datenblatt gefittet.
// Vak und Vgk werden auf den physikalischen Bereich begrenzt (Vak >= 0,
// Vgk <= 0, kein Gitterstrom — Paper Abschnitt 4).
//=============================================================================
#pragma once

#include <cmath>

namespace fairklaus {

struct Triode6386
{
	// Parameter aus Paper Tabelle 1
	static constexpr double p1 = 3.981e-8;
	static constexpr double p2 = 2.383;
	static constexpr double p3 = 0.5;
	static constexpr double p4 = 0.1;
	static constexpr double p5 = 1.8;
	static constexpr double p6 = 0.5;
	static constexpr double p7 = -0.03922;
	static constexpr double p8 = 0.2;

	// pow(p3 - p4*vgk, p5): konstant pro Gitterspannung (Newton-Iteration hoistbar)
	static inline double denomConst (double vgk)
	{
		if (vgk > 0.0) vgk = 0.0;
		return std::pow (p3 - p4 * vgk, p5);
	}

	// kombinierte Auswertung (ein pow + ein exp): Ia (Gl. 1) und dIa/dVak (Gl. 7)
	static inline void eval (double vgk, double vak, double D, double& ia, double& dia)
	{
		if (vak < 0.0) vak = 0.0;
		if (vgk > 0.0) vgk = 0.0;
		const double e   = (double) std::expf ((float) (p7 * vak - p8 * vgk));
		const double E   = p6 + e;
		const double num = (double) std::powf ((float) vak, (float) p2);
		ia = (p1 * num) / (D * E);
		if (vak <= 1e-9) { dia = 0.0; return; }            // Ia ~ Vak^p2 mit p2 > 1
		const double dnum = p2 * num / vak;                // = p2 * Vak^(p2-1)
		dia = (p1 / D) * ((dnum * E - num * (p7 * e)) / (E * E));
	}

	// Anodenstrom in Ampere (Gl. 1)
	static inline double Ia (double vgk, double vak)
	{
		double ia, d; eval (vgk, vak, denomConst (vgk), ia, d); return ia;
	}

	// analytische Ableitung dIa/dVak (fuer Newton-Raphson, Paper Gl. 7)
	static inline double dIa_dVak (double vgk, double vak)
	{
		double ia, d; eval (vgk, vak, denomConst (vgk), ia, d); return d;
	}
};

} // namespace fairklaus
