//=============================================================================
// fairklaus — host_sim: minimaler VST3-Host (dlopen) zum Offline-Test des
// gebauten PunishChild 670 .vst3: Factory -> Component -> IAudioProcessor,
// Parameter setzen, Blöcke prozessieren, Gain Reduction + Bypass prüfen.
//
// Aufruf: host_sim <Pfad zum PunishChild 670.vst3>
//=============================================================================
#include <cstdio>
#include <cstring>
#include <cmath>
#include <dlfcn.h>
#include <string>
#include <vector>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "Parameters.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/vsttypes.h"
#include "pluginterfaces/base/ibstream.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Steinberg;
using namespace Steinberg::Vst;

static int failures = 0;
static void expect (bool ok, const char* what)
{
	if (!ok) { ++failures; std::printf ("FAIL: %s\n", what); }
	else std::printf ("ok: %s\n", what);
}

int main (int argc, char** argv)
{
	if (argc < 2) { std::printf ("usage: host_sim <PunishChild 670.vst3>\n"); return 2; }
	std::string bundle = argv[1];
	std::string so = bundle + "/Contents/x86_64-linux/PunishChild 670.so";
	void* h = dlopen (so.c_str (), RTLD_NOW | RTLD_LOCAL);
	if (!h) { std::printf ("dlopen failed: %s\n", dlerror ()); return 2; }

	using ModuleEntryFn = bool (*) (void*);
	using ModuleExitFn = bool (*) ();
	using FactoryFn = IPluginFactory* (*) ();
	auto moduleEntry = (ModuleEntryFn) dlsym (h, "ModuleEntry");
	auto moduleExit  = (ModuleExitFn) dlsym (h, "ModuleExit");
	auto getFactory  = (FactoryFn) dlsym (h, "GetPluginFactory");
	expect (moduleEntry && moduleExit && getFactory, "exports ModuleEntry/ModuleExit/GetPluginFactory vorhanden");
	if (!getFactory) return 2;
	if (moduleEntry) moduleEntry (h);
	IPluginFactory* factory = getFactory ();
	expect (factory != nullptr, "GetPluginFactory liefert Factory");

	// Audio-Effect-Klasse finden
	TUID classID = { 0 };
	bool found = false;
	for (int32 i = 0; i < factory->countClasses (); ++i)
	{
		PClassInfo ci;
		if (factory->getClassInfo (i, &ci) == kResultOk && strcmp (ci.category, kVstAudioEffectClass) == 0)
		{
			memcpy (classID, ci.cid, sizeof (TUID));
			found = true;
			std::printf ("klasse: %s\n", ci.name);
			break;
		}
	}
	expect (found, "Audio-Effect-Klasse in Factory gefunden");

	IComponent* component = nullptr;
	if (factory->createInstance (classID, IComponent::iid, (void**) &component) != kResultOk || !component)
	{
		std::printf ("createInstance fehlgeschlagen\n");
		return 2;
	}
	IAudioProcessor* processor = nullptr;
	IEditController* controller = nullptr;
	expect (component->queryInterface (IAudioProcessor::iid, (void**) &processor) == kResultOk && processor, "IAudioProcessor");
	expect (component->queryInterface (IEditController::iid, (void**) &controller) == kResultOk && controller, "IEditController (Single-Component)");

	expect (component->initialize (nullptr) == kResultOk, "initialize");
	expect (processor->canProcessSampleSize (kSample32) == kResultTrue, "canProcessSampleSize 32");

	ProcessSetup setup { kRealtime, kSample32, 512, 44100.0 };
	expect (processor->setupProcessing (setup) == kResultOk, "setupProcessing");
	expect (component->setActive (true) == kResultOk, "setActive");
	expect (processor->getLatencySamples () >= 0, "getLatencySamples");

	// Parameter siehe Parameters.h (Enum, keine Magic-IDs)
	auto setP = [&] (Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue v) { return controller->setParamNormalized (id, v); };
	using namespace fairklaus;
	setP (kParamPower, 1.0);          // Power ON
	setP (kParamInputGain, 0.0);      // Input 0 dB
	setP (kParamThreshold, 1.0);      // Threshold +10 dB (max. Limiting)
	setP (kParamTimeConstant, 2.0 / 4.0); // Time Constant 3
	setP (kParamOutputGain, 0.0);     // Output 0 dB
	setP (kParamOversampling, 1.0);   // Oversampling 8x (index 3 -> normalized 1.0)
	setP (kParamStereoMode, 0.0);     // L/R

	const int32 N = 512;
	std::vector<Sample32> inL (N), inR (N), outL (N), outR (N);
	Sample32* inCh[2] = { inL.data (), inR.data () };
	Sample32* outCh[2] = { outL.data (), outR.data () };
	AudioBusBuffers inBusB, outBusB;
	inBusB.numChannels = 2; inBusB.silenceFlags = 0; inBusB.channelBuffers32 = inCh;
	outBusB.numChannels = 2; outBusB.silenceFlags = 0; outBusB.channelBuffers32 = outCh;
	AudioBusBuffers inArr[1] = { inBusB };
	AudioBusBuffers outArr[1] = { outBusB };

	auto runBlock = [&] (double inAmp, double freq, int blocks, int skipBlocks = 0) -> double {
		double maxOut = 0.0;
		for (int b = 0; b < blocks; ++b)
		{
			if (b < skipBlocks) { /* trotzdem prozessieren (zum Einschwingen), aber nicht messen */
			}
			const bool measure = b >= skipBlocks;
			for (int i = 0; i < N; ++i)
			{
				double x = inAmp * std::sin (2.0 * M_PI * freq * (b * N + i) / 44100.0);
				inL[i] = (Sample32) x;
				inR[i] = (Sample32) x;
			}
			ProcessData data {};
			data.processMode = kRealtime;
			data.symbolicSampleSize = kSample32;
			data.numSamples = N;
			data.numInputs = 1;
			data.numOutputs = 1;
			data.inputs = inArr;
			data.outputs = outArr;
			data.inputParameterChanges = nullptr;
			(void) setup;
			if (processor->process (data) != kResultOk) { std::printf ("process failed\n"); break; }
			for (int i = 0; i < N; ++i)
			{
				if (!std::isfinite (outL[i]) || !std::isfinite (outR[i])) { maxOut = 1e30; }
				if (measure) maxOut = std::max (maxOut, (double) std::fabs (outL[i]));
			}
		}
		return maxOut;
	};

	// 1) Limiting: gleicher Pegel bei thr=-20 (Referenz) und thr=+10 -> GR > 0.5 dB
	setP (kParamThreshold, 0.0);
	double maxRef = runBlock (0.9, 1000.0, 200, 100);
	setP (kParamThreshold, 1.0);
	double maxLoud = runBlock (0.9, 1000.0, 200, 100);
	std::printf ("in=0.9: thr=-20 -> %.4f, thr=+10 -> %.4f (GR=%.2f dB)\n",
	             maxRef, maxLoud, 20.0 * std::log10 (std::max (maxRef, 1e-9) / std::max (maxLoud, 1e-9)));
	expect (std::isfinite (maxLoud) && std::isfinite (maxRef), "Ausgabe endlich");
	expect (maxRef > 1e-6 && maxLoud < maxRef * std::pow (10.0, -0.5 / 20.0),
	        "Gain Reduction greift (> 0.5 dB Unterschied)");

	// 2) Power OFF -> Bypass (Ausgang ~ Eingang), nach Einschwingen messen
	setP (kParamPower, 0.0);
	double maxBypass = runBlock (0.5, 1000.0, 120, 60);
	std::printf ("maxOut bei Power OFF, in=0.5: %.4f (Bypass)\n", maxBypass);
	expect (std::fabs (maxBypass - 0.5) < 0.05, "Power OFF = Bypass (~0.5)");
	setP (0, 1.0);

	// 3) State roundtrip
	IBStream* st = nullptr;
	// MemoryStream aus dem SDK wäre komfortabel; hier eigene Mini-Implementierung:
	struct MemStream : IBStream {
		std::vector<uint8> buf; size_t pos = 0; uint32 ref = 1;
		tresult PLUGIN_API queryInterface (const TUID, void**) override { return kNoInterface; }
		uint32 PLUGIN_API addRef () override { return ++ref; }
		uint32 PLUGIN_API release () override { return --ref; }
		tresult PLUGIN_API read (void* d, int32 n, int32* r) override {
			int32 a = (int32) std::min ((size_t) n, buf.size () - pos);
			if (a <= 0) { if (r) *r = 0; return kResultFalse; }
			memcpy (d, buf.data () + pos, a); pos += a; if (r) *r = a; return kResultOk; }
		tresult PLUGIN_API write (void* d, int32 n, int32* w) override {
			buf.insert (buf.end (), (uint8*) d, (uint8*) d + n); if (w) *w = n; return kResultOk; }
		tresult PLUGIN_API seek (int64 p, int32 m, int64* np) override {
			int64 np2 = m == IBStream::kIBSeekSet ? p : m == IBStream::kIBSeekCur ? (int64) pos + p : (int64) buf.size () + p;
			if (np2 < 0) np2 = 0; pos = (size_t) np2; if (np) *np = (int64) pos; return kResultOk; }
		tresult PLUGIN_API tell (int64* p) override { if (p) *p = (int64) pos; return kResultOk; }
	};
	MemStream ms;
	tresult rGet = component->getState (&ms);
	std::printf ("getState -> %d (buf=%zu bytes)\n", (int) rGet, ms.buf.size ());
	expect (rGet == kResultOk, "getState");
	setP (kParamThreshold, 0.0);   // Threshold verstellen
	ms.pos = 0;
	tresult rSet = component->setState (&ms);
	std::printf ("setState -> %d\n", (int) rSet);
	expect (rSet == kResultOk, "setState");
	expect (controller->getParamNormalized (fairklaus::kParamThreshold) == 1.0, "State-Roundtrip: Threshold wieder +10 dB");

	// 4) setActive/terminate/initialize-Zyklen wie der SDK-Validator
	for (int i = 0; i < 4; ++i)
	{
		std::fprintf (stderr, "cycle %d: setupProcessing\n", i); processor->setupProcessing (setup);
		std::fprintf (stderr, "cycle %d: setActive true\n", i); component->setActive (true);
		std::fprintf (stderr, "cycle %d: setActive false\n", i); component->setActive (false);
		std::fprintf (stderr, "cycle %d: deactivate buses\n", i);
		component->activateBus (kAudio, kInput, 0, false);
		component->activateBus (kAudio, kOutput, 0, false);
		std::fprintf (stderr, "cycle %d: terminate\n", i); component->terminate ();
		std::fprintf (stderr, "cycle %d: initialize\n", i); component->initialize (nullptr);
		if (i < 2)
		{
			component->activateBus (kAudio, kInput, 0, true);
			component->activateBus (kAudio, kOutput, 0, true);
		}
		std::fprintf (stderr, "cycle %d: setActive true 2\n", i); component->setActive (true);
	}
	expect (true, "setActive/terminate/initialize Zyklen ohne Absturz");

	component->setActive (false);
	component->terminate ();
	if (processor) processor->release ();
	if (controller) controller->release ();
	component->release ();
	factory->release ();
	if (moduleExit) moduleExit ();
	dlclose (h);

	std::printf ("\n%s\n", failures == 0 ? "ALL HOST TESTS PASSED" : "HOST TESTS FAILED");
	return failures == 0 ? 0 : 1;
}
