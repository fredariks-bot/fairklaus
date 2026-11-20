//=============================================================================
// fairklaus — Zeitkonstanten-Netzwerk der Sidechain als WDF, Paper Fig. 8
//
// Raffensperger, DAFx-12 (2012), Fig. 8 + Tabelle 3:
//   Knoten Vsc = Vsc+ - Vsc-, gespeist von der Stromquelle Isc (Norton-Einspeisung
//   in den Parallel-Adaptor "root"). Drei parallele Zweige zwischen Vsc+ und Vsc-:
//     Zweig T:  RT || CT            (Haupt-Zeitkonstante)
//     Zweig U:  RU in Serie mit CU  (programmgesteuerte Release-Erhöhung)
//     Zweig V:  RV in Serie mit CV
//   "RU und RV sind für einige Schalterstellungen offen" (Tabelle 3 -> 1 GΩ).
//   Program Dependent Release: bei anhaltend hohem Pegel laden sich CU/CV auf
//   und verlängern die Release-Zeit (Paper Abschnitt 5).
//
//   WDF-Topologie:
//     root    = Parallel[ paralA, paralB ]
//     paralA  = Parallel[ RT, CT ]
//     serieA  = Serie[ RU, CU ];  serieB = Serie[ RV, CV ]
//     paralB  = Parallel[ serieA, serieB ]
//   Einspeisung: root.incident( root.reflected() - 2*Isc*root.Rp ); Vsc = CT.voltage()
//=============================================================================
#pragma once

#include "Wdf.h"

namespace fairklaus {

struct TimeConstantNetwork
{
	// Tabelle 3 des Papers: {CT, CU, CV, RT, RU, RV} für die 6 Schalterstellungen
	static constexpr double TABLE[6][6] = {
		{ 2e-6, 8e-6, 20e-6,  51.9e3,  1e9,   1e9   },  // Pos. 1: 0.3 s
		{ 2e-6, 8e-6, 20e-6, 149.9e3,  1e9,   1e9   },  // Pos. 2: 0.8 s
		{ 4e-6, 8e-6, 20e-6, 220.0e3,  1e9,   1e9   },  // Pos. 3: 2.0 s
		{ 8e-6, 8e-6, 20e-6, 220.0e3,  1e9,   1e9   },  // Pos. 4: 5.0 s
		{ 4e-6, 8e-6, 20e-6, 220.0e3, 100e3,  1e9   },  // Pos. 5: 2.0 s / 10.0 s
		{ 2e-6, 8e-6, 20e-6, 220.0e3, 100e3, 100e3  },  // Pos. 6: 0.3 s / 5.0 s / 25.0 s
	};

	TimeConstantNetwork (double fs) { setSampleRate (fs); }

	void setSampleRate (double fs)
	{
		ct.set (table[0], fs);
		cu.set (table[1], fs);
		cv.set (table[2], fs);
		rt.Rp = table[3];
		ru.Rp = table[4];
		rv.Rp = table[5];
		wiring ();
	}

	// pos: 1..6 (Schalterstellung, Panel zeigt 1..5)
	void setPosition (int pos)
	{
		if (pos < 1) pos = 1;
		if (pos > 6) pos = 6;
		table = TABLE[pos - 1];
		rt.Rp = table[3];
		ru.Rp = table[4];
		rv.Rp = table[5];
	}

	void updateCaps (double fs)
	{
		ct.set (table[0], fs);
		cu.set (table[1], fs);
		cv.set (table[2], fs);
		wiring ();
	}

	// Isc als Norton-Stromquelle (Blatt des Root-Paralleladaptors) einspeisen.
	// Vsc = Spannung über CT wird positiv (Paper Gl. 13: Vdiff = |Vstage3| - Vsc
	// -> stabiles Gleichgewicht).
	double process (double iscVal)
	{
		src.Is = iscVal;
		const double u = root.reflected ();   // up
		dbgU = u; dbgBA = paralA.b; dbgBct = ct.b; dbgInnerB = inner.b;
		root.incident (u);                     // down, Stamm offen (b=a)
		dbgAct = ct.a; dbgV = ct.voltage ();
		return ct.voltage ();
	}
	double dbgU=0, dbgBA=0, dbgBct=0, dbgInnerB=0, dbgAct=0, dbgV=0;

	double vSc = 0.0;
	double dbgRootR () const { return root.Rp; }

private:
	void wiring ()
	{
		paralA.connect (&rt, &ct);
		serieA.connect (&ru, &cu);
		serieB.connect (&rv, &cv);
		paralB.connect (&serieA, &serieB);
		inner.connect (&paralA, &paralB);
		src.Rp = 1e12;                          // Norton: Parallelwiderstand ~ unendlich
		root.connect (&src, &inner);
	}

	const double* table = TABLE[0];
	wdf::CurrentSource src { 0.0, 1.0 };
	wdf::Capacitor ct, cu, cv;
	wdf::Resistor rt, ru, rv;
	wdf::Serie serieA, serieB;
	wdf::Parallel paralA, paralB, inner, root;
};

} // namespace fairklaus
