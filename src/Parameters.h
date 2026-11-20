//=============================================================================
// fairklaus — Parameter des PunishChild 670 (Panel siehe Referenzbild)
//=============================================================================
#pragma once

#include "pluginterfaces/vst/vsttypes.h"

namespace fairklaus {

enum ParamID : Steinberg::Vst::ParamID
{
	kParamPower = 0,       // Netzschalter ON/OFF (oben/unten im Panel)
	kParamInputGain,       // INPUT GAIN  0..20 dB
	kParamThreshold,       // THRESHOLD  -20..+10 dB
	kParamTimeConstant,    // TIME CONSTANT 1..5 (Tabelle 3, Pos. 1..5)
	kParamOutputGain,      // OUTPUT GAIN 0..20 dB
	kParamOversampling,    // 1x/2x/4x/8x (Paper: 8x) — nicht auf dem Panel
	kParamStereoMode,      // L/R oder M/S (Lateral/Vertical) — nicht auf dem Panel
	kNumParams
};

} // namespace fairklaus
