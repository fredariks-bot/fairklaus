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

	// Anodenstrom in Ampere (Gl. 1)
	static inline double Ia (double vgk, double vak)
	{
		if (vak < 0.0) vak = 0.0;
		if (vgk > 0.0) vgk = 0.0;
		const double base = p3 - p4 * vgk;               // > 0 fuer Vgk <= 0
		const double denom = std::pow (base, p5) * (p6 + std::exp (p7 * vak - p8 * vgk));
		return (p1 * std::pow (vak, p2)) / denom;
	}

	// analytische Ableitung dIa/dVak (fuer Newton-Raphson, Paper Gl. 7)
	static inline double dIa_dVak (double vgk, double vak)
	{
		if (vak < 0.0) vak = 0.0;
		if (vgk > 0.0) vgk = 0.0;
		if (vak <= 0.0) return 0.0;                       // Ia ~ Vak^p2 mit p2 > 1
		const double base = p3 - p4 * vgk;
		const double e    = std::exp (p7 * vak - p8 * vgk);
		const double E    = p6 + e;
		const double num  = std::pow (vak, p2);
		const double dE   = p7 * e;
		const double dnum = p2 * std::pow (vak, p2 - 1.0);
		const double D    = std::pow (base, p5);
		return (p1 / D) * ((dnum * E - num * dE) / (E * E));
	}
};

} // namespace fairklaus
