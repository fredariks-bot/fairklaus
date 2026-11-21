//=============================================================================
// fairklaus — PunishChild 670: VST3-Kompressor
// komplettes WDF-Schaltungsmodell des Fairchild 670 nach
// Raffensperger, "Toward a Wave Digital Filter Model of the Fairchild 670
// Limiter", Proc. DAFx-12, York, 2012.
//
// Single-Component-Plugin (IComponent + IAudioProcessor + IEditController).
//=============================================================================
#pragma once

#include <atomic>
#include <vector>

#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"

#include "Parameters.h"
#include "dsp/Fairchild670.h"

#if defined(FAIRKLAUS_WITH_EDITOR)
namespace fairklaus { class PanelEditor; }
#endif

namespace fairklaus {

//------------------------------------------------------------------------
class PunishChild670 : public Steinberg::Vst::SingleComponentEffect
{
public:
	PunishChild670 ();
	~PunishChild670 () override;

	// IPluginBase
	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;

	// IComponent (State heißt hier setState/getState, siehe Makro in vstsinglecomponenteffect.h)
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;

	// IAudioProcessor
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
	Steinberg::uint32 PLUGIN_API getLatencySamples () SMTG_OVERRIDE;

	// IEditController
	Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setParamNormalized (Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;

	// für den Editor: Gain-Reduction (dB) für das VU-Meter
	double currentGainReductionDb () const { return grForMeter.load (); }

#if defined(FAIRKLAUS_WITH_EDITOR)
	void registerEditor (PanelEditor* e) { editor = e; }
	void unregisterEditor (PanelEditor* e) { if (editor == e) editor = nullptr; }
#endif

private:
	void applyParameters ();                    // Parameter -> Modell
	void applyParameter (Steinberg::Vst::ParamID id, double normalized);

	Fairchild670 model;
	double sampleRate = 44100.0;
	bool active = false;

	// geglättete Werte (Vermeidung von Clicks bei Parameterwechseln)
	double inputGainLin = 1.0, outputGainLin = 1.0, trimLin = 1.0;
	double inputGainSmooth = 1.0, trimSmooth = 1.0;   // geglaettet gegen Clicks
	int osFactor = 4;   // Default 4x (CPU); ueber Parameter 1/2/4/8x
	// OS/TC duerfen das Modell nicht vom UI-Thread umschalten (setSampleRate
	// allokiert, TC rewired die WDF-Adaptoren) -> process() wendet sie an:
	std::atomic<int> pendingTcPos { -1 };
	double powerMix = 1.0;      // 1 = an, 0 = Bypass
	double powerTarget = 1.0;

	std::atomic<float> grForMeter { 0.0f };

	// wiederverwendete Block-Puffer (keine Allokation im Audio-Thread)
	std::vector<double> bufL, bufR, procL, procR, outL, outR;

#if defined(FAIRKLAUS_WITH_EDITOR)
	PanelEditor* editor = nullptr;
#endif
};

//------------------------------------------------------------------------
Steinberg::FUnknown* createPunishChild670Instance (void*);

} // namespace fairklaus
