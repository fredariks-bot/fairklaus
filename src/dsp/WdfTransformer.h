//=============================================================================
// fairklaus — Nicht-idealer Übertrager als WDF-Zwei-Port
//
// Raffensperger, DAFx-12 (2012), Fig. 3/4 und Tabelle 2:
//   Primärseite:  Rp (Wicklungswiderstand) in Serie mit Lp (Streuinduktivität)
//   Kern:         Rc (Kernverluste) parallel Lm (Magnetisierungsinduktivität)
//   dann idealer Transformator Np/Ns
//   Sekundärseite: Rs in Serie mit Ls, Cw (Wicklungskapazität) parallel zum Load
//
// Topologie (WDF-Baum, adaptierter Port = Primäranschluss):
//   root(Serie) = [ serieA, paralC ]
//   serieA      = Serie[ Lp, Rp ]
//   paralC      = Parallel[ idealXfmr, paralA ]
//   paralA      = Parallel[ Lm, Rc ]
//   xfmr.child  = serieC
//   serieC      = Serie[ paralB, serieB ]
//   serieB      = Serie[ Rs, Ls ]
//   paralB      = Parallel[ load, Cw ]
//=============================================================================
#pragma once

#include "Wdf.h"

namespace fairklaus {

struct TransformerParams
{
	double N   = 1.0;   // Np/Ns
	double Rp  = 10.0;  // Ohm
	double Lp  = 4e-3;  // H
	double Rc  = 10e3;  // Ohm
	double Lm  = 35.7;  // H
	double Rs  = 50.0;  // Ohm
	double Ls  = 1e-3;  // H
	double Cw  = 210e-12; // F
	double RpPort = 600.0; // Portwiderstand der Primärseite
};

// Tabelle 2 des Papers: Signalverstärker Ein-/Ausgang und Sidechain-Eingang.
// N = Np/Ns (Primär/Sekundär). Physikalisch: Eingang 600 Ohm -> ~50 kOhm Grid
// (1:9, d.h. Np/Ns = 1/9), Ausgang ~60 kOhm CT -> 600 Ohm (9:1, Np/Ns = 9),
// Sidechain-Eingang 600 Ohm -> ~170 kOhm (1:17, Paper Abschnitt 5).
struct TransformerTable2
{
	static TransformerParams signalInput ()    // T101/102: 600 Ohm / 50 kOhm, 1:9
	{
		return { 1.0 / 9.0, 10.0, 4e-3, 10e3, 35.7, 50.0, 1e-3, 210e-12, 600.0 };
	}
	static TransformerParams signalOutput ()   // T201/202: 60 kOhm CT / 600 Ohm, 9:1
	{
		return { 9.0, 5.0, 100e-6, 10e3, 35.7, 1.0, 400e-6, 1e-12, 600.0 };
	}
	static TransformerParams sidechainInput () // T103/203: 600 Ohm / 170 kOhm, 1:17
	{
		return { 1.0 / 17.0, 10.0, 2e-3, 50e3, 5.7, 50.0, 1e-3, 10e-12, 600.0 };
	}
};

struct NonIdealTransformer : wdf::OnePort
{
	NonIdealTransformer (double fs, const TransformerParams& p) : prm (p)
	{
		Lp.set (p.Lp, fs);
		Lm.set (p.Lm, fs);
		Ls.set (p.Ls, fs);
		Cw.set (p.Cw, fs);
		Rp_.Rp = p.Rp;
		Rc_.Rp = p.Rc;
		Rs_.Rp = p.Rs;
		xfmr.N  = p.N;
		xfmr.Rp = p.RpPort;
		Rp      = p.RpPort;
	}

	void setSampleRate (double fs)
	{
		Lp.set (prm.Lp, fs);
		Lm.set (prm.Lm, fs);
		Ls.set (prm.Ls, fs);
		Cw.set (prm.Cw, fs);
		wiring (load);
	}

	// Sekundär-Last anschließen und WDF-Baum verdrahten
	void connectChild (wdf::OnePort* loadEl)
	{
		load = loadEl;
		wiring (loadEl);
	}

	double reflected () override { return root.reflected (); }
	void   incident (double w) override { root.incident (w); }
	double R () const override { return root.R (); }
	double G () const override { return root.G (); }

	// Spannung über der Sekundärwicklung (Cw-Spannung) = Ausgangsspannung
	double vOut () const { return Cw.voltage (); }

	TransformerParams prm;

private:
	void wiring (wdf::OnePort* loadEl)
	{
		serieA.connect (&Lp, &Rp_);
		serieB.connect (&Rs_, &Ls);
		paralA.connect (&Lm, &Rc_);
		paralB.connect (loadEl, &Cw);
		serieC.connect (&paralB, &serieB);
		xfmr.connectChild (&serieC);
		paralC.connect (&xfmr, &paralA);
		root.connect (&serieA, &paralC);
		Rp = root.Rp;
	}

	wdf::Inductor Lp, Lm, Ls;
	wdf::Capacitor Cw { 1e-12, 44100.0 };
	wdf::Resistor Rp_ { 1.0 }, Rc_ { 1.0 }, Rs_ { 1.0 };
	wdf::IdealTransformer xfmr { 1.0, 600.0 };
	wdf::Serie serieA, serieB, serieC, root;
	wdf::Parallel paralA, paralB, paralC;
	wdf::OnePort* load = nullptr;
};

//-----------------------------------------------------------------------------
// Eingangsverschaltung nach Fig. 4: Spannungsquelle Vin mit Rin, parallel
// Abschlusswiderstand Rterm, dann Übertrager, sekundär Lastwiderstand Rload.
// process(vIn) liefert die Spannung über Rload (bei Signalverstärker: V_RL1,
// Paper Gl. 4-6; bei Sidechain: V_Rload, Paper Abschnitt 5).
//-----------------------------------------------------------------------------
struct InputCircuit : wdf::OnePort
{
	InputCircuit (double fs, const TransformerParams& tp, double rin, double rterm, double rload)
	    : xfmr (fs, tp), rterm_ (rterm), rload_ (rload), vin_ (0.0, rin)
	{
		wiring ();
	}

	void setSampleRate (double fs)
	{
		xfmr.setSampleRate (fs);   // verdrahtet den Trafo intern neu (bottom-up)
		wiring ();
	}

	// WICHTIG: bottom-up verdrahten, damit die Portwiderstände stimmen
	void wiring ()
	{
		xfmr.connectChild (&rload_);
		paral.connect (&xfmr, &rterm_);
		serie.connect (&paral, &vin_);
		Rp = serie.Rp;
	}

	// vIn einspeisen (ein WDF-Schritt), V_Rload zurückgeben
	double process (double vIn)
	{
		vin_.Vs = vIn;
		const double u = serie.reflected ();  // Up-Pass (von den Blättern zum Stamm)
		serie.incident (-u);                  // Down-Pass; Stamm = Kurzschluss (Schleife zu)
		a = u;
		b = -u;
		return rload_.voltage ();             // V_Rload (Paper Gl. 4: V_RL1 = V_Rload)
	}

	double reflected () override { return serie.reflected (); }
	void   incident (double w) override { vin_.Vs = w; serie.incident (b); }
	double R () const override { return serie.R (); }
	double G () const override { return serie.G (); }

	NonIdealTransformer xfmr;
	wdf::Resistor rterm_, rload_;
	wdf::VoltageSource vin_;
	wdf::Serie serie;
	wdf::Parallel paral;
};

} // namespace fairklaus
