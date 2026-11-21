//=============================================================================
// fairklaus — komplettes WDF-Schaltungsmodell des Fairchild 670 (Stereo)
//
// Raffensperger, DAFx-12 (2012), Fig. 1: Feedback-Topologie — die Sidechain
// tastet die AUSGANGSSPANNUNG Vout ab und steuert damit die Verstärkung des
// Signalverstärkers. Pro Überabtast-Schritt:
//
//   1. Isc = (IscL + IscR)/2   (Sidechain Fig. 7, Gl. 10-14; Abtastung von
//                               Vout(t-1), Rückkopplung mit z^-1: Gl. 13)
//   2. Vsc = Zeitkonstanten-Netzwerk (WDF, Fig. 8) aus Isc
//   3. VoutL/R = Signalverstärker (Fig. 5/6) mit Vx = Vbias - Vsc(t-1)
//
// Die Unit-Delay im Rückkopplungszweig löst die schleifenfreie Berechnung
// (Paper Abschnitt 5/6). Warmup mit Nulloingang stellt den DC-Arbeitspunkt ein.
//=============================================================================
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

#include "SignalAmplifier.h"
#include "Sidechain.h"
#include "TimeConstantNetwork.h"
#include "Oversampling.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fairklaus {

struct Fairchild670
{
	double fsBase = 44100.0;
	int oversampling = 8;

	void setSampleRate (double fs, int osFactor)
	{
		fsBase = fs;
		oversampling = osFactor < 1 ? 1 : osFactor;
		const double fsOs = fsBase * oversampling;
		osL.setup (oversampling);
		osR.setup (oversampling);
		hiL.resize ((size_t) oversampling);
		hiR.resize ((size_t) oversampling);
		grCounter = 0;
		scCounter = 0;
		ampL.setSampleRate (fsOs);
		ampR.setSampleRate (fsOs);
		scL.setSampleRate (fsOs);
		scR.setSampleRate (fsOs);
		tc.setPosition (tcPos);
		tc.setSampleRate (fsOs);
		scL.setPhiAC (phiAC);
		scR.setPhiAC (phiAC);
		vsc = 0.0;
		voutLPrev = 0.0;
		voutRPrev = 0.0;
		psuPhase = 0.0;
		peakDecay = std::exp (-1.0 / (0.020 * fsBase));   // ~20 ms Spitzenwert-Haltung
		peakIn = 0.0;
		peakOut = 0.0;
		warmup (0.5);
		grSmooth = 0.0;
	}

	void setTimeConstantPos (int pos) // 1..5 (Panel), intern Tabelle 3 Reihe pos
	{
		tcPos = pos < 1 ? 1 : (pos > 5 ? 5 : pos);
		tc.setPosition (tcPos);
		tc.setSampleRate (fsBase * oversampling);
	}

	// thrDb in [-20, +10] -> phiAC in [0,1] (AC-Threshold-Poti)
	void setThreshold (double thrDb)
	{
		double p = (thrDb + 20.0) / 30.0;
		phiAC.store (p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p));
		scL.setPhiAC (phiAC.load ());
		scR.setPhiAC (phiAC.load ());
	}

	void setMainsHz (double hz) { mainsHz = hz; }

	// Pegel-Anpassung Plugin <-> Schaltung: 0 dBFS (1.0) entspricht ~6 V in der
	// Schaltung. Damit greift die Sidechain (Schwelle phi'DC = 2.44 V an
	// Vpot = phiAC*V_Rload/2) schon bei mittleren Pegeln und der Threshold-Regler
	// hat einen weiten, hörbaren Wirkungsbereich. outputScale macht die
	// Kleinsignalverstärkung des Signalamps (~0.80) zu unity.
	double inputScale = 6.0;
	double outputScale = 0.2074;   // = 1 / (inputScale * 0.8035)

	// Ein Block bei Basisabtastrate verarbeiten
	void processBlock (const double* inL, const double* inR, double* outL, double* outR, int n)
	{
		const int N = oversampling;
		// Idle-Skip: Eingang exakt 0 und Modell im Ruhezustand -> DSP sparen
		double maxIn = 0.0;
		for (int i = 0; i < n; ++i)
		{
			const double a = std::fabs (inL[i]), b = std::fabs (inR[i]);
			if (a > maxIn) maxIn = a;
			if (b > maxIn) maxIn = b;
		}
		if (maxIn == 0.0 && std::fabs (vsc) < 1e-9
		    && std::fabs (voutLPrev) < 1e-9 && std::fabs (voutRPrev) < 1e-9)
		{
			for (int i = 0; i < n; ++i) { outL[i] = 0.0; outR[i] = 0.0; }
			grSmooth = 0.0;
			return;
		}
		const double ripple = 0.05;   // 50 mV Netzbrumm (2x mains) auf der 240V-Schiene
		for (int i = 0; i < n; ++i)
		{
			psuPhase += 2.0 * M_PI * (2.0 * mainsHz) / fsBase;
			ampL.push.setPsuRipple (psuPhase);
			ampL.pull.setPsuRipple (psuPhase);
			ampR.push.setPsuRipple (psuPhase);
			ampR.pull.setPsuRipple (psuPhase);
			osL.upsampleInto (inL[i], hiL);
			osR.upsampleInto (inR[i], hiR);
			for (int k = 0; k < N; ++k)
			{
				double oL, oR;
				processOs (hiL[k], hiR[k], oL, oR);
				osL.downsample (oL, [&] (double v) { outL[i] = v; });
				osR.downsample (oR, [&] (double v) { outR[i] = v; });
			}
		}
	}

	// Ein einzelner Modellschritt bei der überabgetasteten Rate.
	// Ein-/Ausgabe in Plugin-Pegeln (FS); die Pegelanpassung in die Schaltung
	// (inputScale/outputScale) passiert hier innen, damit processOs() direkt
	// mit Plugin-Pegeln arbeitet (Tests) wie processBlock().
	void processOs (double inL, double inR, double& outL, double& outR)
	{
		const double sL = inputScale * inL;
		const double sR = inputScale * inR;

		// 1)+2) Sidechain + Zeitkonstanten-Netzwerk laufen auf Basisrate
		// (Vsc ist durch das TC-Netzwerk ohnehin träge; spart 7/8 der Rechenlast)
		dbgVscUsed = vsc;
		if ((scCounter++ & (oversampling - 1)) == 0)
		{
			const double iscL = scL.process (voutLPrev, vsc);
			const double iscR = scR.process (voutRPrev, vsc);
			const double isc  = 0.5 * (iscL + iscR);       // Stereo-Linked
			dbgIsc = isc; dbgVoutPrev = voutLPrev;
			vsc = tc.process (isc);                        // Zeitkonstanten-Netzwerk (WDF)
		}

		// 3) Signalverstärker mit Vx = Vbias - Vsc(t-1)
		const double oL = ampL.process (sL, vsc);
		const double oR = ampR.process (sR, vsc);

		voutLPrev = oL;   // Schaltungs-Volt: Sidechain tastet Vout ab
		voutRPrev = oR;
		outL = outputScale * oL;   // zurück auf Plugin-Pegel
		outR = outputScale * oR;

		// Gain-Reduction für Anzeige nur auf Basisrate: echte Pegel-Hüllkurve
		// Ein/Aus (Plugin-Pegel) — GR = 20*log10(peakIn/peakOut), keine Fehl-Anzeige
		// durch Röhren-Grid-Strom bei hohem Pegel.
		if ((grCounter++ & (oversampling - 1)) == 0)
		{
			peakIn  = std::max (std::fabs (inL),  peakIn  * peakDecay);
			peakOut = std::max (std::fabs (outL), peakOut * peakDecay);
			double gr = 0.0;
			if (peakIn > 1e-4 && peakOut > 1e-9)
				gr = 20.0 * std::log10 (peakIn / peakOut);
			if (!(gr > 0.0)) gr = 0.0;
			if (gr > 60.0) gr = 60.0;
			const double c = gr > grSmooth ? std::exp (-1.0 / (0.005 * fsBase))   // Attack ~5 ms
			                               : std::exp (-1.0 / (0.300 * fsBase));  // Release ~300 ms
			grSmooth += (gr - grSmooth) * (1.0 - c);
		}
	}

	double gainReductionDb () const { return grSmooth; }   // für VU-Meter
	double latencySamples () const { return osL.latencyBase; }

private:
	void warmup (double seconds)
	{
		const double fsOs = fsBase * oversampling;
		const long n = (long) (seconds * fsOs);
		double a, b;
		for (long i = 0; i < n; ++i)
			processOs (0.0, 0.0, a, b);
	}

	SignalAmplifier ampL { 44100.0 }, ampR { 44100.0 };
	Sidechain scL { 44100.0 }, scR { 44100.0 };
	TimeConstantNetwork tc { 44100.0 };
	Oversampler osL, osR;
	std::vector<double> hiL, hiR;
	int grCounter = 0;
	int scCounter = 0;
	double vsc = 0.0;
public:
	double dbgVsc () const { return vsc; }
	double dbgIaL () const { return ampL.push.iaLast; }
	double dbgVakL () const { return ampL.push.vakLast; }
	double dbgVkL () const { return ampL.push.vkDelayedPub(); }
	double dbgIsc = 0.0, dbgVoutPrev = 0.0, dbgVscUsed = 0.0;
	double dbgVRloadL () const { return scL.dbgVRload; }
private:
	double voutLPrev = 0.0, voutRPrev = 0.0;
	double peakIn = 0.0, peakOut = 0.0, peakDecay = 0.999;
	double grSmooth = 0.0;
	std::atomic<double> phiAC { 0.0 };   // UI-Thread schreibt, Audio-Thread liest
	int tcPos = 5;
	double mainsHz = 60.0;
	double psuPhase = 0.0;
};

} // namespace fairklaus
