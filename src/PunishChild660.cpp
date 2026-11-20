//=============================================================================
// fairklaus — PunishChild 660: Implementierung + Factory
//=============================================================================
#include "PunishChild660.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/base/ibstream.h"

#if defined(FAIRKLAUS_WITH_EDITOR)
#include "editor/Editor.h"
#endif

#include <cmath>
#include <cstring>
#include <cstdio>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace fairklaus {

//------------------------------------------------------------------------
// Klass-ID des Plugins (stabil halten!)
static const FUID kPunishChild660ProcessorID (0xF41A6670, 0x66660001, 0x4B1A55C0, 0xDEADBEEF);

//------------------------------------------------------------------------
PunishChild660::PunishChild660 ()
{
	// Busse: 1x Stereo In, 1x Stereo Out
	addAudioInput (STR ("Audio In"), SpeakerArr::kStereo, kMain, BusInfo::kDefaultActive);
	addAudioOutput (STR ("Audio Out"), SpeakerArr::kStereo, kMain, BusInfo::kDefaultActive);

	// Parameter (Reihenfolge = Panel von links nach rechts, dann Advanced)
	auto* power = new RangeParameter (STR ("Power"), kParamPower, nullptr,
	                                  0., 1., 1., 1,
	                                  ParameterInfo::kCanAutomate | ParameterInfo::kIsList);
	parameters.addParameter (power);

	auto* mains = new RangeParameter (STR ("Mains"), kParamMains, STR ("Hz"),
	                                  50., 60., 60., 1,
	                                  ParameterInfo::kCanAutomate | ParameterInfo::kIsList);
	parameters.addParameter (mains);

	parameters.addParameter (new RangeParameter (STR ("Input Gain"), kParamInputGain, STR ("dB"),
	                                             0., 20., 0., kStepCountContinuous,
	                                             ParameterInfo::kCanAutomate));
	parameters.addParameter (new RangeParameter (STR ("Threshold"), kParamThreshold, STR ("dB"),
	                                             -20., 10., 0., kStepCountContinuous,
	                                             ParameterInfo::kCanAutomate));
	parameters.addParameter (new RangeParameter (STR ("Time Constant"), kParamTimeConstant, nullptr,
	                                             1., 5., 3., 4,
	                                             ParameterInfo::kCanAutomate | ParameterInfo::kIsList));
	parameters.addParameter (new RangeParameter (STR ("Output Gain"), kParamOutputGain, STR ("dB"),
	                                             0., 20., 0., kStepCountContinuous,
	                                             ParameterInfo::kCanAutomate));
	auto* os = new RangeParameter (STR ("Oversampling"), kParamOversampling, nullptr,
	                               0., 3., 3., 3,
	                               ParameterInfo::kCanAutomate | ParameterInfo::kIsList);
	parameters.addParameter (os);
	auto* sm = new RangeParameter (STR ("Stereo Mode"), kParamStereoMode, nullptr,
	                               0., 1., 0., 1,
	                               ParameterInfo::kCanAutomate | ParameterInfo::kIsList);
	parameters.addParameter (sm);
}

//------------------------------------------------------------------------
PunishChild660::~PunishChild660 () = default;

//------------------------------------------------------------------------
tresult PLUGIN_API PunishChild660::initialize (FUnknown* context)
{
	tresult res = SingleComponentEffect::initialize (context);
	if (res != kResultOk) return res;
	applyParameters ();
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API PunishChild660::terminate ()
{
	return SingleComponentEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API PunishChild660::setActive (TBool state)
{
	active = state;
	if (state)
	{
		model.setSampleRate (sampleRate, osFactor);
		applyParameters ();
		powerMix = powerTarget;
	}
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API PunishChild660::setupProcessing (ProcessSetup& setup)
{
	sampleRate = setup.sampleRate;
	model.setSampleRate (sampleRate, 8);
	return SingleComponentEffect::setupProcessing (setup);
}

//------------------------------------------------------------------------
uint32 PLUGIN_API PunishChild660::getLatencySamples ()
{
	return (uint32) std::ceil (model.latencySamples ());
}

//------------------------------------------------------------------------
tresult PLUGIN_API PunishChild660::setParamNormalized (Steinberg::Vst::ParamID id, ParamValue value)
{
	tresult res = SingleComponentEffect::setParamNormalized (id, value);
	if (res == kResultOk)
	{
		applyParameter (id, value);
	}
	return res;
}

//------------------------------------------------------------------------
void PunishChild660::applyParameter (Steinberg::Vst::ParamID id, double normalized)
{
	auto* p = parameters.getParameter (id);
	if (!p) return;
	const double plain = p->toPlain (normalized);
	switch (id)
	{
		case kParamPower: powerTarget = plain > 0.5 ? 1.0 : 0.0; break;
		case kParamMains: model.setMainsHz (plain); break;
		case kParamInputGain: inputGainLin = std::pow (10.0, plain / 20.0); break;
		case kParamThreshold: model.setThreshold (plain); break;
		case kParamTimeConstant: model.setTimeConstantPos ((int) plain + 0.5); break;
		case kParamOutputGain: outputGainLin = std::pow (10.0, plain / 20.0); break;
		case kParamOversampling:
		{
			static const int factors[4] = { 1, 2, 4, 8 };
			int idx = (int) plain;
			if (idx < 0) idx = 0;
			if (idx > 3) idx = 3;
			osFactor = factors[idx];
			model.setSampleRate (sampleRate, osFactor);
			break;
		}
		case kParamStereoMode: break; // wird in process() gelesen
		default: break;
	}
}

//------------------------------------------------------------------------
void PunishChild660::applyParameters ()
{
	for (uint32 i = 0; i < parameters.getParameterCount (); ++i)
		applyParameter ((Steinberg::Vst::ParamID) parameters.getParameterByIndex (i)->getInfo ().id,
		                parameters.getParameterByIndex (i)->getNormalized ());
}

//------------------------------------------------------------------------
tresult PLUGIN_API PunishChild660::process (ProcessData& data)
{
	if (!active || data.numOutputs == 0 || data.numInputs == 0) return kResultOk;

	// Parameter-Änderungen einsammeln (letzter Wert pro Block)
	if (data.inputParameterChanges)
	{
		int32 numQueues = data.inputParameterChanges->getParameterCount ();
		for (int32 q = 0; q < numQueues; ++q)
		{
			auto* queue = data.inputParameterChanges->getParameterData (q);
			if (!queue) continue;
			int32 numPoints = queue->getPointCount ();
			if (numPoints > 0)
			{
				int32 offset = 0; ParamValue value = 0;
				queue->getPoint (numPoints - 1, offset, value);
				setParamNormalized (queue->getParameterId (), value);
			}
		}
	}

	auto* inBus = &data.inputs[0];
	auto* outBus = &data.outputs[0];
	const int32 numSamples = data.numSamples;
	const int32 numChannels = std::min (inBus->numChannels, outBus->numChannels);
	if (numChannels < 1 || numSamples <= 0) return kResultOk;

	const bool sample64 = (data.symbolicSampleSize == kSample64);

	// Eingabepuffer (double)
	const int32 nCh = std::min (numChannels, 2);
	std::vector<double> bufL (numSamples), bufR (numSamples);
	auto readCh = [&] (int ch, int i) -> double {
		if (sample64) return ((Sample64*) inBus->channelBuffers64[ch])[i];
		return ((Sample32*) inBus->channelBuffers32[ch])[i];
	};
	for (int i = 0; i < numSamples; ++i)
	{
		bufL[i] = nCh > 0 ? readCh (0, i) : 0.0;
		bufR[i] = nCh > 1 ? readCh (1, i) : bufL[i];
	}

	// M/S wenn gewünscht
	const bool midside = parameters.getParameter (kParamStereoMode)->toPlain (
		parameters.getParameter (kParamStereoMode)->getNormalized ()) > 0.5;
	if (midside)
	{
		const double s = 1.0 / std::sqrt (2.0);
		for (int i = 0; i < numSamples; ++i)
		{
			double l = bufL[i], r = bufR[i];
			bufL[i] = (l + r) * s;
			bufR[i] = (l - r) * s;
		}
	}

	// Eingangsverstärkung (geglättet) anwenden
	std::vector<double> procL (numSamples), procR (numSamples);
	const double gIn = inputGainLin;
	for (int i = 0; i < numSamples; ++i) { procL[i] = bufL[i] * gIn; procR[i] = bufR[i] * gIn; }

	// Modell
	std::vector<double> outL (numSamples), outR (numSamples);
	model.processBlock (procL.data (), procR.data (), outL.data (), outR.data (), numSamples);

	// Ausgangsverstärkung + Power-Bypass-Mischung (geglättet, ~5 ms)
	const double gOut = outputGainLin;
	const double ramp = 1.0 / std::max (1.0, 0.005 * sampleRate);
	for (int i = 0; i < numSamples; ++i)
	{
		powerMix += (powerTarget - powerMix) * ramp;
		if (powerMix < 0.0) powerMix = 0.0;
		if (powerMix > 1.0) powerMix = 1.0;
		double l = outL[i] * gOut;
		double r = outR[i] * gOut;
		if (midside)
		{
			const double s = 1.0 / std::sqrt (2.0);
			double m = l, d = r;
			l = (m + d) * s;
			r = (m - d) * s;
		}
		l = powerMix * l + (1.0 - powerMix) * bufL[i];
		r = powerMix * r + (1.0 - powerMix) * bufR[i];
		if (sample64)
		{
			((Sample64*) outBus->channelBuffers64[0])[i] = nCh > 0 ? l : 0.0;
			if (nCh > 1) ((Sample64*) outBus->channelBuffers64[1])[i] = r;
		}
		else
		{
			((Sample32*) outBus->channelBuffers32[0])[i] = nCh > 0 ? (Sample32) l : 0.0f;
			if (nCh > 1) ((Sample32*) outBus->channelBuffers32[1])[i] = (Sample32) r;
		}
	}
	// zusätzliche Kanäle (falls >2) stumm oder kopiert — hier: 1:1 durchreichen im Bypass
	for (int ch = 2; ch < outBus->numChannels; ++ch)
	{
		for (int i = 0; i < numSamples; ++i)
		{
			double v = ch < inBus->numChannels
				? (sample64 ? ((Sample64*) inBus->channelBuffers64[ch])[i] : (Sample32) inBus->channelBuffers32[ch][i])
				: 0.0;
			if (sample64) ((Sample64*) outBus->channelBuffers64[ch])[i] = v;
			else ((Sample32*) outBus->channelBuffers32[ch])[i] = (Sample32) v;
		}
	}

	grForMeter.store ((float) model.gainReductionDb ());
	return kResultOk;
}

//------------------------------------------------------------------------
// State (Preset/Session): Magic + Version + alle Parameter (normalisiert)
//------------------------------------------------------------------------
static const int32 kStateMagic = 0x464B3636;   // "FK66"
static const int32 kStateVersion = 1;

tresult PLUGIN_API PunishChild660::getState (IBStream* state)
{
	int32 magicOut = kStateMagic, versionOut = kStateVersion;
	if (state->write (&magicOut, sizeof (magicOut)) != kResultOk) return kResultFalse;
	if (state->write (&versionOut, sizeof (versionOut)) != kResultOk) return kResultFalse;
	for (uint32 i = 0; i < parameters.getParameterCount (); ++i)
	{
		auto* p = parameters.getParameterByIndex (i);
		Steinberg::Vst::ParamID id = p->getInfo ().id;
		double v = p->getNormalized ();
		if (state->write (&id, sizeof (id)) != kResultOk) return kResultFalse;
		if (state->write (&v, sizeof (v)) != kResultOk) return kResultFalse;
	}
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API PunishChild660::setState (IBStream* state)
{
	int32 magic = 0, version = 0;
	if (state->read (&magic, sizeof (magic)) != kResultOk || magic != kStateMagic) return kResultFalse;
	if (state->read (&version, sizeof (version)) != kResultOk || version != kStateVersion) return kResultFalse;
	for (;;)
	{
		Steinberg::Vst::ParamID id = 0;
		double v = 0.0;
		if (state->read (&id, sizeof (id)) != kResultOk) break;
		if (state->read (&v, sizeof (v)) != kResultOk) break;
		auto* p = parameters.getParameter (id);
		if (p) setParamNormalized (id, p->toNormalized (p->toPlain (v)));
	}
	applyParameters ();
	return kResultOk;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API PunishChild660::createView (FIDString name)
{
#if defined(FAIRKLAUS_WITH_EDITOR)
	if (strcmp (name, ViewType::kEditor) == 0)
	{
		auto* e = new PanelEditor (this);
		registerEditor (e);
		return e;
	}
#else
	(void) name;
#endif
	return nullptr;
}

//------------------------------------------------------------------------
FUnknown* createPunishChild660Instance (void*)
{
	return static_cast<Steinberg::Vst::IComponent*> (new PunishChild660 ());
}

} // namespace fairklaus

//------------------------------------------------------------------------
// Factory
//------------------------------------------------------------------------
BEGIN_FACTORY_DEF (stringCompanyName, stringCompanyWeb, stringCompanyEmail)

DEF_CLASS2 (INLINE_UID_FROM_FUID (fairklaus::kPunishChild660ProcessorID),
            Steinberg::PClassInfo::kManyInstances,
            kVstAudioEffectClass,
            stringPluginName,
            Steinberg::Vst::kDistributable,
            "Fx|Compressor",
            FULL_VERSION_STR,
            kVstVersionString,
            fairklaus::createPunishChild660Instance)

END_FACTORY
