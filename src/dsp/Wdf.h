//=============================================================================
// fairklaus — WDF (Wave Digital Filter) Primitiven
//
// Nach: Raffensperger, "Toward a Wave Digital Filter Model of the Fairchild
// 670 Limiter", Proc. DAFx-12, York, 2012, Abschnitt 3.
//
// Konvention (Fettweis): jedes Element besitzt einen Port mit Portwiderstand
// Rp und zwei Wellen:
//   a = v + Rp*i  (einfallende Welle, Adaptor -> Element)
//   b = v - Rp*i  (reflektierte Welle, Element -> Adaptor)
// Ein Element berechnet aus a die ausgehende Welle b (reflected()/incident()).
// Adaptoren (Serie/Parallel) verbinden zwei Ports; der adaptierte Port (this)
// ist reflexionsfrei, wenn Rp = Rl+Rr (Serie) bzw. Rp = Rl||Rr (Parallel).
//=============================================================================
#pragma once

#include <cmath>

namespace fairklaus {
namespace wdf {

//-----------------------------------------------------------------------------
// Basisklasse Ein-Port
//-----------------------------------------------------------------------------
struct OnePort
{
	double Rp = 1.0;   // Portwiderstand
	double a  = 0.0;   // einfallende Welle
	double b  = 0.0;   // reflektierte Welle

	virtual ~OnePort () = default;
	virtual double reflected ()        = 0;  // b berechnen und zurückgeben
	virtual void   incident (double w) = 0;  // a setzen

	double voltage () const { return 0.5 * (a + b); }
	double current () const { return (a - b) / (2.0 * Rp); }
	virtual double R () const { return Rp; }
	virtual double G () const { return 1.0 / Rp; }
};

//-----------------------------------------------------------------------------
// Widerstand: b = 0 (vollständig absorbierend, wenn R == Rp)
//-----------------------------------------------------------------------------
struct Resistor : OnePort
{
	Resistor () = default;
	explicit Resistor (double r) { Rp = r; }
	double reflected () override { b = 0.0; return b; }
	void   incident (double w) override { a = w; }
};

//-----------------------------------------------------------------------------
// Kondensator: Rp = 1/(2*C*Fs), b(n) = a(n-1)   (Paper: "port resistance 1/(2CFs)")
//-----------------------------------------------------------------------------
struct Capacitor : OnePort
{
	Capacitor () = default;
	Capacitor (double c, double fs) { set (c, fs); }
	void set (double c, double fs) { Rp = 1.0 / (2.0 * c * fs); }
	double reflected () override { b = a; return b; }    // b(n) = a(n-1)
	void   incident (double w) override { a = w; }
};

//-----------------------------------------------------------------------------
// Induktivität: Rp = 2*L*Fs, b(n) = -a(n-1)
//-----------------------------------------------------------------------------
struct Inductor : OnePort
{
	Inductor () = default;
	Inductor (double l, double fs) { set (l, fs); }
	void set (double l, double fs) { Rp = 2.0 * l * fs; }
	double reflected () override { b = -a; return b; }   // b(n) = -a(n-1)
	void   incident (double w) override { a = w; }
};

//-----------------------------------------------------------------------------
// Spannungsquelle (Thevenin): Vs mit Serienwiderstand R, b = 2*Vs - a
//-----------------------------------------------------------------------------
struct VoltageSource : OnePort
{
	VoltageSource () = default;
	VoltageSource (double vs, double r) : Vs (vs) { Rp = r; }
	// Thevenin-Quelle Vs mit Serienwiderstand Rp: ausgehende Welle b = Vs
	double reflected () override { b = Vs; return b; }
	void   incident (double w) override { a = w; }
	double Vs;
};

//-----------------------------------------------------------------------------
// Stromquelle (Norton): Is mit Parallelwiderstand R, b = a + 2*Rp*Is
//-----------------------------------------------------------------------------
struct CurrentSource : OnePort
{
	CurrentSource () = default;
	CurrentSource (double is, double r) : Is (is) { Rp = r; }
	// Norton-Quelle Is mit Parallelwiderstand Rp: ausgehende Welle b = Rp*Is
	double reflected () override { b = Rp * Is; return b; }
	void   incident (double w) override { a = w; }
	double Is;
};

struct OpenCircuit : OnePort
{
	explicit OpenCircuit (double r) { Rp = r; }
	double reflected () override { b = a; return b; }
	void   incident (double w) override { a = w; }
};

struct ShortCircuit : OnePort
{
	explicit ShortCircuit (double r) { Rp = r; }
	double reflected () override { b = -a; return b; }
	void   incident (double w) override { a = w; }
};

//-----------------------------------------------------------------------------
// Serien-Adaptor (2 Ports + adaptierter Port this): Rp = Rl + Rr
//-----------------------------------------------------------------------------
struct Serie : OnePort
{
	OnePort* l = nullptr;
	OnePort* r = nullptr;

	void connect (OnePort* left, OnePort* right)
	{
		l = left;
		r = right;
		Rp = l->R () + r->R ();
	}
	double reflected () override
	{
		b = -(l->reflected () + r->reflected ());
		return b;
	}
	void incident (double wave) override
	{
		const double lrW = wave + l->b + r->b;
		l->incident (l->b - (l->R () / Rp) * lrW);
		r->incident (r->b - (r->R () / Rp) * lrW);
		a = wave;
	}
};

//-----------------------------------------------------------------------------
// Parallel-Adaptor (2 Ports + adaptierter Port this): Rp = Rl || Rr
//-----------------------------------------------------------------------------
struct Parallel : OnePort
{
	OnePort* l = nullptr;
	OnePort* r = nullptr;

	void connect (OnePort* left, OnePort* right)
	{
		l = left;
		r = right;
		Rp = (l->R () * r->R ()) / (l->R () + r->R ());
	}
	double reflected () override
	{
		const double g = l->G () + r->G ();          // = G_trunk (adaptiert)
		b = (l->G () * l->reflected () + r->G () * r->reflected ()) / g;
		return b;
	}
	void incident (double wave) override
	{
		// wave = vom Parent einfallende Welle (b_trunk); G_trunk = G_l + G_r
		const double g = l->G () + r->G ();
		const double v = (wave * g + l->b * l->G () + r->b * r->G ()) / (2.0 * g);
		l->incident (2.0 * v - l->b);
		r->incident (2.0 * v - r->b);
		a = wave;
	}
};

//-----------------------------------------------------------------------------
// Idealtransformator (2-Port). N = Np/Ns.
// Wellengleichungen (mit Rp_s = Rp_p / N^2):  b_p = N*a_s,  b_s = a_p/N
//-----------------------------------------------------------------------------
struct IdealTransformer : OnePort
{
	IdealTransformer () = default;
	explicit IdealTransformer (double n, double rp) : N (n) { Rp = rp; }

	void connectChild (OnePort* child) { childEl = child; }

	double reflected () override            // child -> parent
	{
		aS = childEl->reflected ();         // vom Sekundärkreis reflektierte Welle
		b  = N * aS;
		return b;
	}
	void incident (double wave) override    // parent -> child
	{
		a  = wave;
		childEl->incident (a / N);
	}
	double N;
	double aS = 0.0;
	OnePort* childEl = nullptr;
};

//-----------------------------------------------------------------------------
// Bidirektionale Einheitsverzögerung (z^-1 in beide Richtungen) zwischen zwei
// Port-Faces — bricht die unberechenbare schleifenfreie Kopplung der beiden
// 2*C1-Kathodenkondensatoren (Paper Abschnitt 4, Fig. 6).
//-----------------------------------------------------------------------------
struct BidirectionalDelay
{
	struct Face : OnePort
	{
		BidirectionalDelay* link = nullptr;
		bool isA = true;
		double reflected () override { b = isA ? link->sB : link->sA; return b; }
		void   incident (double w) override { a = w; if (isA) link->sA = a; else link->sB = a; }
	};
	Face a, b;
	double sA = 0.0, sB = 0.0;

	explicit BidirectionalDelay (double r)
	{
		a.Rp = b.Rp = r;
		a.link = this; a.isA = true;
		b.link = this; b.isA = false;
	}
	void setPortResistance (double r) { a.Rp = b.Rp = r; }
};

} // namespace wdf
} // namespace fairklaus
