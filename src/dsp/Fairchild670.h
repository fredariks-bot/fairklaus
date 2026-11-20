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
#include <cmath>
#include <vector>

#include "SignalAmplifier.h"
#include "Sidechain.h"
#include "TimeConstantNetwork.h"
#include "Oversampling.h"

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
		os.setup (oversampling);
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
		warmup (0.5);
		iaRestL = std::max (ampL.push.iaLast, 1e-9);
		iaRestR = std::max (ampR.push.iaLast, 1e-9);
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
		phiAC = p < 0.0 ? 0.0 : (p > 1.0 ? 1.0 : p);
		scL.setPhiAC (phiAC);
		scR.setPhiAC (phiAC);
	}

	void setMainsHz (double hz) { mainsHz = hz; }

	// Pegel-Anpassung Plugin <-> Schaltung: 0 dBFS (1.0) entspricht ~2 V in der
	// Schaltung (Arbeitspunkt des 670 bei ca. +4 dBm), damit das Limiting nahe
	// Vollaussteuerung greift. outputScale macht die Ruhverstärkung (~0.46) zu unity.
	double inputScale = 2.0;
	double outputScale = 1.087;   // = 1 / (inputScale * 0.46)

	// Ein Block bei Basisabtastrate verarbeiten
	void processBlock (const double* inL, const double* inR, double* outL, double* outR, int n)
	{
		const int N = oversampling;
		hiL.resize ((size_t) N);
		hiR.resize ((size_t) N);
		for (int i = 0; i < n; ++i)
		{
			os.upsampleInto (inputScale * inL[i], hiL);
			os.upsampleInto (inputScale * inR[i], hiR);
			for (int k = 0; k < N; ++k)
			{
				double oL, oR;
				processOs (hiL[k], hiR[k], oL, oR);
				os.downsample (outputScale * oL, [&] (double v) { outL[i] = v; });
				os.downsample (outputScale * oR, [&] (double v) { outR[i] = v; });
			}
		}
	}

	// Ein einzelner Modellschritt bei der überabgetasteten Rate
	void processOs (double inL, double inR, double& outL, double& outR)
	{
		const double fsOs = fsBase * oversampling;
		psuPhase += 2.0 * M_PI * (2.0 * mainsHz) / fsOs;  // Netzbrumm (2x mains) auf 240V
		const double ripple = 0.05;                        // 50 mV — unhörbar, nur Modell-Treue
		ampL.push.psuRippleAmp = ampL.pull.psuRippleAmp = ripple;
		ampR.push.psuRippleAmp = ampR.pull.psuRippleAmp = ripple;
		ampL.push.setPsuRipple (psuPhase);
		ampL.pull.setPsuRipple (psuPhase);
		ampR.push.setPsuRipple (psuPhase);
		ampR.pull.setPsuRipple (psuPhase);

		// 1) Sidechain tastet Vout(t-1) ab (Feedback, Gl. 13 mit z^-1 auf Vsc)
		dbgVscUsed = vsc;
		const double iscL = scL.process (voutLPrev, vsc);
		const double iscR = scR.process (voutRPrev, vsc);
		const double isc  = 0.5 * (iscL + iscR);           // Stereo-Linked
		dbgIsc = isc; dbgVoutPrev = voutLPrev;

		// 2) Zeitkonstanten-Netzwerk (WDF) -> Vsc
		const double vscNew = tc.process (isc);

		// 3) Signalverstärker mit Vx = Vbias - Vsc(t-1)
		outL = ampL.process (inL, vsc);
		outR = ampR.process (inR, vsc);

		voutLPrev = outL;
		voutRPrev = outR;
		vsc = vscNew;

		// Gain-Reduction für Anzeige (geglättet, Mittel beider Kanäle)
		const double gr = 0.5 * (ampL.gainReductionDb (iaRestL) + ampR.gainReductionDb (iaRestR));
		const double c = gr > grSmooth ? std::exp (-1.0 / (0.005 * fsOs))   // Attack ~5 ms
		                               : std::exp (-1.0 / (0.300 * fsOs));  // Release ~300 ms
		grSmooth += (gr - grSmooth) * (1.0 - c);
	}

	double gainReductionDb () const { return grSmooth; }   // für VU-Meter
	double latencySamples () const { return os.latencyBase; }

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
	Oversampler os;
	std::vector<double> hiL, hiR;
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
	double iaRestL = 1e-6, iaRestR = 1e-6;
	double grSmooth = 0.0;
	double phiAC = 0.0;
	int tcPos = 3;
	double mainsHz = 60.0;
	double psuPhase = 0.0;
};

} // namespace fairklaus
