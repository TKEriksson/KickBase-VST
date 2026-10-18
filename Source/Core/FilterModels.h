#pragma once

#include "Oversampler2x.h"
#include <memory>
#include <vector>

// Filtermodellerna Ladder, Formant och Comb (fas D2 i docs/content-plan.md): en kanal. Kickbase (SVF) och TB-303 ligger kvar i InstrumentSynth respektive
// Tb303Filter. Alla tre tar cutoff i Hz (efter keytrack och filter-envelope), resonans 0..1 och två generiska ratter P1 och P2 (0..1) vars betydelse beror
// på modellen. `filterType` är 0 LP 24, 1 LP 12, 2 Band, 3 High och gäller bara Ladder. Modellvalet är InstrumentParams::filterModel (2, 3, 4).
//
//  Ladder:  Moog-stege med fyra 1-poliga TPT-steg (Vadim Zavalishin, "The Art of VA Filter Design", Native Instruments, rev. 2.1.2: kapitlet om transistorstegen
//           ("Ladder filter"), den slutna lösningen för återkopplingen över fyra TPT-lågpass; kapitelnumret är inte kontrollerat mot boken, kapitelnamnet är det som gäller)
//           där återkopplingen löses i ett svep (ingen enkelsamplings-fördröjning) och en tanh-mättnad på ingången. Körs på 2x (olinjär). P1 = DRIVE (1..6x). P2 används inte.
//           JUCE:s egen dsp::LadderFilter kan inte användas: dess cutoff och resonans går genom en SmoothedValue på 50 ms (juce_LadderFilter.cpp:103, 166-171) som
//           inte går att ändra, så en filter-envelope som ändrar cutoff varje sampling skulle fördröjas och släta ut sig.
//  Formant: fem parallella bandpass (TPT-SVF) med vokalernas formanter A E I O U. CUTOFF förskjuter alla formanter (kvot mot 1 kHz), RESO smalnar bandbredden,
//           P1 = MORPH (vokal, kontinuerlig A-E-I-O-U), P2 = BLEND (torr/våt, 1 = bara filtret).
//  Comb:    kamfilter med återkoppling: en fördröjningslinje (Lagrange 3:e ordningen) med en lågpass i slingan. CUTOFF = kamfrekvensen, RESO = återkopplingen
//           (högst 0,97), P1 = DAMPING (lågpass i slingan), P2 = BLEND (1 = bara kamfiltret).
class ModelFilter
{
public:
    enum Model { ladder = 2, formant = 3, comb = 4 };

    ModelFilter();
    ~ModelFilter();
    ModelFilter (ModelFilter&&) noexcept;
    ModelFilter& operator= (ModelFilter&&) noexcept;

    void prepare (double baseSampleRate); // allokerar (kamfiltrets fördröjningslinje): aldrig på ljudtråden
    void reset();                         // nollställer allt tillstånd (vid modellbyte)

    // Anropas varje sampling (billigt: räknar bara om det som ändrats).
    void setParameters (Model model, double cutoffHz, double resonance, double p1, double p2, int filterType);

    float process (float input);

    // Hur många Comb-fördröjningslinjer minne som behövs (för rapporten): samplingar per kanal vid en samplingsfrekvens.
    static int combMaxDelaySamples (double sampleRate) { return (int) std::ceil (sampleRate / 20.0) + 8; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
