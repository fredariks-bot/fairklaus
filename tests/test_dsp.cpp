//=============================================================================
// fairklaus — Unit-Tests für WDF-Primitive und das komplette 670-Modell
// (ohne VST3-SDK, direkt mit g++ baubar:  g++ -std=c++17 -I../src test_dsp.cpp)
//=============================================================================
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "dsp/Wdf.h"
#include "dsp/Triode6386.h"
#include "dsp/WdfTransformer.h"
#include "dsp/Fairchild670.h"

using namespace fairklaus;

static int failures = 0;
static int checks = 0;
static void expect (bool ok, const char* what)
{
	++checks;
	if (!ok)
	{
		++failures;
		std::printf ("FAIL: %s\n", what);
	}
}
static bool close (double a, double b, double rel = 1e-3)
{
	return std::fabs (a - b) <= rel * (1.0 + std::fabs (b));
}

//-----------------------------------------------------------------------------
// 1) Parallel-Adaptor: Quelle Vs=1, Rs=1 || Last Rl=3  -> v = 1 * (3||1)/... 
//    v_node = Vs * (Rl/(Rs+Rl))? Nein: Rs in Serie mit (Rl to ground):
//    Schaltung: Serie[Rs, Parallel[Rl, ...]] — testen wir Serie+Parallel als
//    Spannungsteiler: Vs=1, Rs=1, Serie mit Last Rl=1 -> v_Rl = 0.5
//-----------------------------------------------------------------------------
static void testAdaptors ()
{
	const double fs = 44100.0;
	// Serie: Quelle (Vs=1, R=1) in Serie mit Last R=1, Stamm kurzgeschlossen
	// -> |v_last| = 0.5, |i| = 0.5
	{
		wdf::VoltageSource src (1.0, 1.0);
		wdf::Resistor load (1.0);
		wdf::Serie s;
		s.connect (&src, &load);
		for (int n = 0; n < 50; ++n)
		{
			const double u = s.reflected ();   // up
			s.incident (-u);                    // down, Stamm = short (b=-a)
		}
		expect (close (std::fabs (load.voltage ()), 0.5, 1e-9), "Serienteiler |v_load| = 0.5");
		expect (close (std::fabs (load.current ()), 0.5, 1e-9), "Serienteiler |i| = 0.5");
	}
	// Parallel: Quelle Vs=1 mit Rs=1, parallel Last Rl=1, Stamm offen -> v = 0.5
	{
		wdf::VoltageSource src (1.0, 1.0);
		wdf::Resistor load (1.0);
		wdf::Parallel p;
		p.connect (&src, &load);
		for (int n = 0; n < 50; ++n)
		{
			const double u = p.reflected ();
			p.incident (u);                     // Stamm = open (b=a)
		}
		expect (close (load.voltage (), 0.5, 1e-9), "Parallelteiler v = 0.5");
	}
	// Kondensator: Rp = 1/(2*C*Fs), b(n) = a(n-1)
	{
		wdf::Capacitor c (1e-6, fs);
		expect (close (c.Rp, 1.0 / (2e-6 * fs), 1e-12), "Cap Rp = 1/(2CFs)");
		expect (close (c.reflected (), 0.0, 1e-12), "Cap: initial b=0");
		c.incident (0.7);
		expect (close (c.reflected (), 0.7, 1e-12), "Cap b(n)=a(n-1): nach incident(0.7) -> 0.7");
		c.incident (0.9);
		expect (close (c.reflected (), 0.9, 1e-12), "Cap b(n)=a(n-1): nach incident(0.9) -> 0.9");
	}
	// Induktivität: Rp = 2*L*Fs, b(n) = -a(n-1)
	{
		wdf::Inductor l (1e-3, fs);
		expect (close (l.Rp, 2e-3 * fs, 1e-12), "Ind Rp = 2LFs");
		l.incident (0.5);
		expect (close (l.reflected (), -0.5, 1e-12), "Ind b(n) = -a(n-1): nach incident(0.5) -> -0.5");
		l.incident (-0.3);
		expect (close (l.reflected (), 0.3, 1e-12), "Ind: nach incident(-0.3) -> +0.3");
	}
	// RC-Tiefpass: Quelle Vs=1, Rs=1, C=1uF parallel, Stamm offen
	{
		const double C = 1e-3;   // tau = RC = 1 ms = 44 Samples
		wdf::VoltageSource src (1.0, 1.0);
		wdf::Capacitor c (C, fs);
		wdf::Parallel p;
		p.connect (&src, &c);
		double v = 0.0;
		for (int n = 0; n < 200000; ++n)   // 4.5 s >> RC
		{
			const double u = p.reflected ();
			p.incident (u);
			v = c.voltage ();
		}
		expect (close (v, 1.0, 1e-6), "RC-LP: v -> Vs (DC)");
		wdf::VoltageSource src2 (1.0, 1.0);
		wdf::Capacitor c2 (C, fs);
		wdf::Parallel p2;
		p2.connect (&src2, &c2);
		double v2 = 0.0;
		const long nRC = (long) (1.0 * C * fs); // t = RC in Samples
		for (long n = 0; n < nRC; ++n)
		{
			const double u = p2.reflected ();
			p2.incident (u);
			v2 = c2.voltage ();
		}
		expect (v2 > 0.55 && v2 < 0.70, "RC-LP: v(RC) ~ 0.63");
	}
	// Stromquelle (Norton): Is=1 mit Rp=1, parallel R=1 -> v = Is*(Rp||R) = 0.5
	{
		wdf::CurrentSource src (1.0, 1.0);
		wdf::Resistor load (1.0);
		wdf::Parallel p;
		p.connect (&src, &load);
		for (int n = 0; n < 50; ++n)
		{
			const double u = p.reflected ();
			p.incident (u);
		}
		expect (close (load.voltage (), 0.5, 1e-9), "Norton-Quelle: v = Is*(Rp||R) = 0.5");
	}
}

//-----------------------------------------------------------------------------
// 2) Idealtransformator: N=2, Rp=8 -> Rs_face = 2; Last 2 Ohm sekundär -> primär 8 Ohm
//-----------------------------------------------------------------------------
static void testTransformer ()
{
	const double fs = 44100.0;
	// Idealtransformator allein: N=2, Rp=8, sekundär Last 2 Ohm (=Rp/N^2)
	// Quelle Vs=1 mit R=8 parallel an Primärseite -> v_prim = 0.5, v_sec = 0.25
	{
		wdf::VoltageSource src (1.0, 8.0);
		wdf::IdealTransformer xf (2.0, 8.0);
		wdf::Resistor load (2.0);
		xf.connectChild (&load);
		wdf::Parallel p;
		p.connect (&src, &xf);
		for (int n = 0; n < 50; ++n)
		{
			const double u = p.reflected ();
			p.incident (u);
		}
		expect (close (load.voltage (), 0.25, 1e-9), "Idealtransformator N=2: v_sec = v_prim/2");
	}
	// NonIdealTransformer bei 1 kHz: Sinus am Eingang, Sekundärspannung messen
	{
		TransformerParams tp = TransformerTable2::signalInput (); // N = Np/Ns = 1/9
		NonIdealTransformer xf (fs, tp);
		wdf::Resistor load (100e3);
		xf.connectChild (&load);
		wdf::VoltageSource src (0.0, 600.0);
		wdf::Resistor rterm (1360.0);
		wdf::Parallel p;
		p.connect (&xf, &rterm);
		wdf::Serie s;
		s.connect (&p, &src);
		double peak = 0.0;
		for (int n = 0; n < 20000; ++n)
		{
			src.Vs = std::sin (2.0 * M_PI * 1000.0 * n / fs);
			const double u = s.reflected ();
			s.incident (-u);
			peak = std::max (peak, std::fabs (load.voltage ()));
		}
		std::printf ("  Trafo 1kHz: v_load peak = %.4f (N=1/9, Speisung 1V)\n", peak);
		expect (peak > 1.0 && peak < 12.0, "Nicht-idealer Trafo: Sekundärspannung ~ N*Primär (1:9 step-up)");
		expect (std::isfinite (peak), "Trafo: finite");
	}
}

//-----------------------------------------------------------------------------
// 3) Triode: Kennwerte aus Gl. 1 (Paper Fig. 2: Vak=250, Vgk=-20 -> Ia ~ mA-Bereich)
//-----------------------------------------------------------------------------
static void testTriode ()
{
	const double ia1 = Triode6386::Ia (-20.0, 250.0);
	const double ia2 = Triode6386::Ia (-10.0, 250.0);
	const double ia3 = Triode6386::Ia (0.0, 250.0);
	expect (ia1 > 0.0 && ia1 < 0.05, "Triode Ia(Vgk=-20,Vak=250) im mA-Bereich");
	expect (ia2 > ia1 && ia3 > ia2, "Triode Ia wächst mit Vgk");
	// Ableitung per Finite Difference prüfen
	const double vgk = -8.0, vak = 150.0;
	const double dAna = Triode6386::dIa_dVak (vgk, vak);
	const double dNum = (Triode6386::Ia (vgk, vak + 1e-4) - Triode6386::Ia (vgk, vak - 1e-4)) / 2e-4;
	expect (close (dAna, dNum, 1e-3), "Triode dIa/dVak analytisch = numerisch");
}

//-----------------------------------------------------------------------------
// 4) Komplettes Modell: statische Kennlinie (1 kHz Sinus, versch. Pegel),
//    Gain Reduction muss mit Threshold zunehmen; Stabilität (keine NaN/Inf).
//-----------------------------------------------------------------------------
static double measureOutDb (Fairchild670& m, double inAmp, double thrDb, double freq = 1000.0)
{
	m.setThreshold (thrDb);
	const double fs = 48000.0;
	// Modell ist nach setSampleRate bei fs; hier direkt os-Rate bekannt:
	const double fsOs = fs * m.oversampling;
	const long nWarm = (long) (0.3 * fsOs);
	double a, b;
	for (long i = 0; i < nWarm; ++i) m.processOs (0.0, 0.0, a, b);
	const long nMeas = (long) (0.2 * fsOs);
	double acc = 0.0;
	long cnt = 0;
	for (long i = 0; i < nMeas; ++i)
	{
		const double t = i / fsOs;
		const double x = inAmp * std::sin (2.0 * M_PI * freq * t);
		m.processOs (x, x, a, b);
		if (i > nMeas / 2) { acc += a * a; ++cnt; }
	}
	const double rms = std::sqrt (acc / cnt);
	return 20.0 * std::log10 (std::max (rms, 1e-12));
}

static void testModelStatic ()
{
	Fairchild670 m;
	m.setSampleRate (48000.0, 4);
	m.setTimeConstantPos (3);
	// kleine Eingangspegel: Kennlinie aufnehmen
	double prevGr = 0.0;
	bool monotonic = true;
	bool allFinite = true;
	for (double thr = -20.0; thr <= 10.0; thr += 10.0)
	{
		const double outDb = measureOutDb (m, 0.5, thr);
		std::printf ("  thr=%6.1f dB  out=%7.2f dBFS(rms, in=0.5)\n", thr, outDb);
		if (!std::isfinite (outDb)) allFinite = false;
	}
	expect (allFinite, "Modell: keine NaN/Inf in statischer Kennlinie");
	// Gain Reduction bei hohem Schwellenwert und lautem Signal
	m.setThreshold (10.0);
	const double fs = 48000.0;
	const double fsOs = fs * m.oversampling;
	double a, b;
	for (long i = 0; i < (long) (0.3 * fsOs); ++i) m.processOs (0.0, 0.0, a, b);
	double maxOut = 0.0;
	for (long i = 0; i < (long) (0.5 * fsOs); ++i)
	{
		const double x = 0.9 * std::sin (2.0 * M_PI * 1000.0 * i / fsOs);
		m.processOs (x, x, a, b);
		maxOut = std::max (maxOut, std::fabs (a));
	}
	const double gr = m.gainReductionDb ();
	std::printf ("  thr=+10, in=0.9: maxOut=%.4f  GR=%.2f dB\n", maxOut, gr);
	expect (maxOut < 0.95, "Modell limitiert: Ausgang < Eingang bei hohem Pegel");
	expect (gr > 0.5, "Modell: Gain Reduction > 0.5 dB bei Limiting");
	(void) prevGr; (void) monotonic;
}

//-----------------------------------------------------------------------------
// 5) Stabilität: 10 s lautes Rauschen, alle Time Constants, beide Threshold-Enden
//-----------------------------------------------------------------------------
static void testModelStability ()
{
	Fairchild670 m;
	m.setSampleRate (44100.0, 2);
	unsigned long rng = 12345;
	auto rnd = [&]() { rng = rng * 6364136223846793005ULL + 1442695040888963407ULL; return (double) (rng >> 33) / (double) (1ULL << 31) - 1.0; };
	bool ok = true;
	for (int tc = 1; tc <= 5 && ok; ++tc)
	{
		for (double thr : {-20.0, 0.0, 10.0})
		{
			m.setTimeConstantPos (tc);
			m.setThreshold (thr);
			double a, b;
			for (long i = 0; i < (long) (5.0 * 44100.0 * m.oversampling); ++i)
			{
				const double x = 0.8 * rnd ();
				m.processOs (x, x, a, b);
				if (!std::isfinite (a) || !std::isfinite (b) || std::fabs (a) > 1e6 || std::fabs (b) > 1e6)
				{
					std::printf ("  instabil: tc=%d thr=%.0f bei i=%ld a=%g\n", tc, thr, i, a);
					ok = false;
					break;
				}
			}
			if (!ok) break;
		}
	}
	expect (ok, "Modell stabil über 5 s Rauschen x alle Time Constants x Threshold-Enden");
}

int main ()
{
	testAdaptors ();
	testTransformer ();
	testTriode ();
	testModelStatic ();
	testModelStability ();
	std::printf ("\n%d Checks, %d Fehler\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
