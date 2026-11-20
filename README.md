# PunishChild 670 — VST3-Kompressor

VST3-Audio-Plugin mit dem **kompletten WDF-Schaltungsmodell (Wave Digital Filter)**
des Fairchild 670 Limiters nach:

> John Raffensperger, *"Toward a Wave Digital Filter Model of the Fairchild 670 Limiter"*,
> Proc. 12th Int. Conference on Digital Audio Effects (DAFx-12), York, UK, 2012.

Das Panel („PunishChild 670") entspricht der Referenzgrafik: VU-Meter (Gain Reduction),
vier Drehregler (INPUT GAIN, THRESHOLD, TIME CONSTANT, OUTPUT GAIN), Netzschalter mit
Kontrolllampe.
Die Oberfläche ist skalierbar (50–400 %, Host-Fenster einfach in der Ecke ziehen).

## Modell → Code (Paper-Abbildungen)

| Paper (DAFx-12) | Datei in `src/dsp/` |
|---|---|
| WDF-Grundlagen (Adaptoren, Ein-Tor-Elemente) | `Wdf.h` |
| Triode 6386, Gl. (1) + Tabelle 1 (Black-Box-Modell, per LM an Datenblatt gefittet) | `Triode6386.h` |
| Übertrager, Fig. 3/4 + Tabelle 2 (nicht-ideal: Rp, Lp, Rc, Lm, Rs, Ls, Cw) | `WdfTransformer.h` |
| Eine Gegentakt-Hälfte mit Ausgangsübertrager, Fig. 6 + Gl. (7)–(9), Kathoden-Netzwerk | `TubeStage.h` |
| Signalverstärker eines Kanals, Fig. 5 (Eingangsschaltung Fig. 4, Vx = V_BIAS − Vsc(t−1)) | `SignalAmplifier.h` |
| Sidechain-Steuerverstärker, Fig. 7 + Gl. (10)–(14) (Class-B-Gleichrichter, Softplus, Stromquelle Isc) | `Sidechain.h` |
| Zeitkonstanten-Netzwerk als WDF, Fig. 8 + Tabelle 3 (6 Stellungen, hier 1–5) | `TimeConstantNetwork.h` |
| Gesamttopologie (Feedback), Fig. 1: Sidechain tastet Ausgang ab, speist Zeitkonstanten-Netzwerk, steuert Gittervorspannung | `Fairchild670.h` |
| 8×-Oversampling (Paper: Polyphase-FIR, Kaiser-Fenster) | `Oversampling.h` |

Verarbeitungsreihenfolge pro (oversampled) Sample (Fig. 1):
1. Sidechain misst den Ausgang des **vorherigen** Samples → Strom `Isc` (Stereo: Mittelwert L/R),
2. Zeitkonstanten-Netzwerk (WDF) rechnet daraus die Steuerspannung `Vsc`,
3. Signalverstärker beider Kanäle mit `Vx = V_BIAS (−7,2 V) − Vsc(t−1)`,
4. `Vsc` wird für den nächsten Schritt zwischengespeichert.

## Parameter

| Panel | Parameter | Bereich | Bedeutung |
|---|---|---|---|
| ON/OFF + Lampe | `Power` | 0/1 | Bypass (gerampt) bei OFF |
| INPUT GAIN | `Input Gain` | 0…20 dB | Eingangsverstärkung vor dem Eingangsübertrager |
| THRESHOLD | `Threshold` | −20…+10 dB | Schwelle → Potentiometer-Einstellung φAC = (thr+20)/30 |
| TIME CONSTANT | `Time Constant` | 1…5 | Stellung des Zeitkonstanten-Netzwerks (Tabelle 3, Pos. 1–5) |
| OUTPUT GAIN | `Output Gain` | 0…20 dB | Ausgangstrim/Make-up-Gain |
| (nicht auf Panel) | `Oversampling` | 1×/2×/4×/8× | Standard 8× wie im Paper |
| (nicht auf Panel) | `Stereo Mode` | L/R, M/S | gekoppelter Stereo-Betrieb oder Mitten/Seite |

VU-Meter zeigt Gain Reduction in dB: `GR = (20/3)·log10(Ia_Ruhe / Ia_aktuell)`
mit Ballistik ~5 ms Angriff / 300 ms Abfall (Paper Abschnitt 6).

## Bauen

Voraussetzung: CMake ≥ 3.25, C++17-Compiler, Git-Submodul des VST3 SDK:

```bash
git submodule update --init --recursive   # vst3sdk (Steinberg VST3 SDK, Tag v3.8.1_build_84)
```

**Mit grafischem Editor (Linux)** – GUI-Abhängigkeiten installieren:

```bash
sudo apt install build-essential cmake pkg-config libx11-dev libxcb1-dev libxcb-util-dev \
                 libxcb-cursor-dev libxcb-keysyms1-dev libxcb-xkb-dev libxkbcommon-dev \
                 libxkbcommon-x11-dev libglib2.0-dev libcairo2-dev libpango1.0-dev \
                 libfreetype-dev libfontconfig-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

**Headless (ohne GUI-Abhängigkeiten):** Der Editor wird automatisch abgeschaltet
(`FAIRKLAUS_WITH_EDITOR=OFF` bzw. fehlende X11/cairo/freetype); Hosts zeigen dann die
generische Parameter-UI. Das DSP bleibt identisch.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFAIRKLAUS_WITH_EDITOR=OFF
cmake --build build -j
```

Ergebnis: `build/VST3/Release/PunishChild 670.vst3`
(auf macOS/Windows analog unter dem jeweiligen Build-Ordner; Installation durch Kopieren
in den VST3-Ordner des Hosts).

**Windows (x64):** lokal mit Visual Studio 2022 + CMake (`cmake -B build -A x64
-DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`), Ergebnis unter
`build/VST3/Release/PunishChild 670.vst3` (enthält `Contents/x86_64-win/`). Fertige
Windows-Builds liefert außerdem die GitHub-Actions-CI als Artefakt
`PunishChild-670-vst3-windows-x64` (siehe Reiter „Actions" bzw. PR #1).

## Tests

```bash
cd build && ctest --output-on-failure
```

- `dsp` — 23 Unit-Checks des WDF-Modells (Adaptoren gegen analytische Lösungen, RC-Glied,
  Übertrager, Trioden-Kennlinie + Ableitung, statische Kennlinie des Gesamtmodells mit
  weichem Knie, Stabilität über 5 s Rauschen für alle Zeitkonstanten/Schwellen).
- `host_sim` — lädt das gebaute `.vst3` per `dlopen`, testet Factory, Component/
  AudioProcessor/EditController, Offline-Verarbeitung inkl. messbarer Gain Reduction,
  Power-Bypass, State-Save/Restore und Activate/Terminate-Zyklen.

SDK-Validator (im Build-Verzeichnis des SDK, falls mitgebaut):

```bash
./build/bin/Release/validator "build/VST3/Release/PunishChild 670.vst3"
```

## Verzeichnisstruktur

```
CMakeLists.txt          Top-Level (Optionen, SDK, Tests)
src/                    Plugin-Quellcode
  Parameters.h          Parameter-IDs
  PunishChild670.*      VST3 SingleComponentEffect (Prozessor + Controller + State)
  dsp/                  WDF-Schaltungsmodell (siehe Tabelle oben)
  editor/               VSTGUI-Panel „PunishChild 670" (nur mit GUI-Deps)
tests/                  test_dsp.cpp (DSP), host_sim.cpp (Plugin-Host-Simulation)
vst3sdk/                Git-Submodul: Steinberg VST3 SDK v3.8.1_build_84
```

## Lizenz / Hinweise

- Der VST3 SDK steht unter der **GPL-3.0** (Steinberg). Entsprechend müssen
  abgeleitete Binär-Plugins ebenfalls unter GPL-3.0 veröffentlicht werden.
- „Fairchild" ist ein eingetragenes Warenzeichen der Fairchild Recording Equipment Corp.
  Dieses Projekt ist nicht mit Fairchild verbunden, unterstützt oder lizenziert;
  die Schaltung wird ausschließlich zu Bildungszwecken nach dem DAFx-12-Paper modelliert.
