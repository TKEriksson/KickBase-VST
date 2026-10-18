#pragma once

#include <memory>

// TB-303-filtret (fas D1 i docs/content-plan.md): Robin Schmidts TeeBeeFilter (Open303, MIT, se Source/ThirdParty/open303) i läget TB_303,
// en kanal. Open303 kör filtret på 4x takten och polynomen i TB_303-läget är anpassade till det; här körs det på 2x (mätt med `tool tb303measure`:
// inom modellens egen känslighet för takten, medan 1x ger upp till 14 dB fel vid hög cutoff och resonans).
//
// Översamplingen är per sampling (synten spelar sample-exakt, så den kan inte köra ett block före filtret) och är JUCE:s egen halvbands-IIR
// (polyfas, kaskad av allpass): koefficienterna kommer ur FilterDesign::designIIRLowpassHalfBandPolyphaseAllpassMethod (juce_FilterDesign.cpp:621-701,
// samma konstruktion som dsp::Oversampling använder) och kedjorna är skrivna per sampling efter Oversampling2TimesPolyphaseIIR::processSamplesUp och
// processSamplesDown (juce_Oversampling.cpp:322-376 och 378-434). Allt är double, ingen allokering efter konstruktionen, tillståndet nollställs av reset().
//
// Open303:s headers ligger bara i Tb303Filter.cpp (pimpl): de definierar makron (PI, EPS, INF ...) och globala funktioner (exp2, round, sign ...) som
// annars skulle läcka in i varje fil som inkluderar InstrumentSynth.h och byta ut anrop där (det ändrade ljudet i ett instrument).
class Tb303Filter
{
public:
    Tb303Filter();
    ~Tb303Filter();
    Tb303Filter (Tb303Filter&&) noexcept;
    Tb303Filter& operator= (Tb303Filter&&) noexcept;

    void prepare (double baseSampleRate); // (den delade halvbandsdesignen skapas första gången, aldrig på ljudtråden)
    void reset();

    // Cutoff i Hz (TeeBee klämmer själv till 200-20 000 Hz) och resonans i procent (100 = självsvängning). Koefficienterna räknas bara om vid ändring.
    void setParameters (double cutoffHz, double resonancePercent);

    float process (float input);

    // Bara för mätning: upp- och nedsamplingen utan filter emellan (passbandet och fördröjningen för själva översamplingen).
    float processWithoutFilter (float input);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
