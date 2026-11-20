//=============================================================================
// fairklaus — Oversampling (Polyphase-FIR) für das WDF-Modell
//
// Das Paper (Abschnitt 6) rechnet mit 8x Oversampling, damit die nichtlinearen
// Verzerrungsprodukte nicht als Aliasing in den Hörbereich fallen und die
// bilinear-verzerrte Frequenzantwort genauer wird.
//   Up:   y[n*N+k] = N * sum_p h[k + p*N] * x[n-p]        (Polyphase, Gain N)
//   Down: y[n]     = sum_m h[m] * x_high[n*N - m]         (FIR + Dezimierung)
// FIR: lineares Phase, Kaiser-Fenster (β=9), Grenzfrequenz 0.45/N (bzgl. der
// Hochabtastrate), N*48+1 Koeffizienten.
//=============================================================================
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fairklaus {

struct Oversampler
{
	int factor = 1;
	std::vector<double> h;      // FIR bei der hohen Abtastrate, DC-Gain = 1
	int tapsPerPhase = 48;
	std::vector<double> upHist;   // Eingangshistorie (Basisrate), Länge tapsPerPhase
	std::vector<double> dnHist;   // Historie bei hoher Abtastrate, Länge h.size()
	int upCount = 0;
	double latencyBase = 0.0;     // Gruppenlaufzeit in Basis-Samples

	void setup (int newFactor)
	{
		factor = newFactor < 1 ? 1 : newFactor;
		const int L = factor * tapsPerPhase + 1;              // ungerade für lineares Phase? -> +1
		const int M = (L - 1) / 2;
		const double wc = 0.9 * M_PI / factor;                // 0.45 * 2π/factor
		h.assign (L, 0.0);
		double beta = 9.0, i0 = 1.0;
		// modified Bessel I0
		{
			double sum = 1.0, term = 1.0;
			for (int k = 1; k < 40; ++k)
			{
				double t = (beta / 2.0) / k;
				term *= t * t;
				sum += term;
			}
			i0 = sum;
		}
		double dc = 0.0;
		for (int m = 0; m < L; ++m)
		{
			const double x = m - M;
			const double sinc = (std::fabs (x) < 1e-12) ? wc / M_PI
			                    : std::sin (wc * x) / (M_PI * x);
			// Kaiser-Fenster
			const double r = (2.0 * m) / (L - 1.0) - 1.0;
			double w = 1.0, term = 1.0;
			const double arg = beta * std::sqrt (std::max (0.0, 1.0 - r * r));
			for (int k = 1; k < 40; ++k)
			{
				double t = arg / k / 2.0 * 2.0; // (arg/2)/k
				t = (arg / 2.0) / k;
				term *= t * t;
				w += term;
			}
			w /= i0;
			h[m] = sinc * w;
			dc += h[m];
		}
		for (auto& v : h) v /= dc;                            // DC-Gain = 1
		upHist.assign (tapsPerPhase, 0.0);
		dnHist.assign (L, 0.0);
		upCount = 0;
		latencyBase = (double) M / (2.0 * factor);            // Gruppenlaufzeit in Basissamples
	}

	// ein Basissample -> factor Hochsamples (Polyphase), out(k) wird pro Hochsample gerufen
	template <typename F>
	void upsample (double x, F&& out)
	{
		for (int p = tapsPerPhase - 1; p > 0; --p) upHist[p] = upHist[p - 1];
		upHist[0] = x;
		for (int k = 0; k < factor; ++k)
		{
			double acc = 0.0;
			for (int p = 0; p < tapsPerPhase; ++p)
				acc += h[k + p * factor] * upHist[p];
			out (factor * acc);
		}
	}

	// Variante mit Puffer: ein Basissample -> factor Hochsamples in buf[0..factor-1]
	void upsampleInto (double x, std::vector<double>& buf)
	{
		int k = 0;
		upsample (x, [&] (double v) { buf[(size_t) k++] = v; });
	}

	// ein Hochsample -> bei jedem factor-ten Sample ein Basissample
	template <typename F>
	void downsample (double x, F&& out)
	{
		for (int m = (int) dnHist.size () - 1; m > 0; --m) dnHist[m] = dnHist[m - 1];
		dnHist[0] = x;
		if (upCount == 0)
		{
			double acc = 0.0;
			for (int m = 0; m < (int) dnHist.size (); ++m) acc += h[m] * dnHist[m];
			out (acc);
		}
		upCount = (upCount + 1) % factor;
	}

	void reset ()
	{
		std::fill (upHist.begin (), upHist.end (), 0.0);
		std::fill (dnHist.begin (), dnHist.end (), 0.0);
		upCount = 0;
	}
};

} // namespace fairklaus
