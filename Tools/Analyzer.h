#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

// Mäter kick och bas i en ljudfil (t.ex. en referenslåt eller ett renderat preset):
// tempo, kickens pitch-svep, svans och envelope, basstegens tonhöjd, envelope och ljushet,
// och basgången sextondel för sextondel.
//
// startSeconds/endSeconds < 0 = välj automatiskt det parti där kick och bas spelar mest ensamma.
int analyzeFile (const juce::File& file, double startSeconds, double endSeconds);

// Mäter basens klang och envelope (eller kickens svep och envelope) i ett parti av en låt och
// söker de syntinställningar som ger närmast samma mätvärden. Skriver ut inställningarna.
// stepsPerBeat: basens rutnät (4 = sextondelar, 3 = åttondelstrioler).
int fitBass (const juce::File& file, double startSeconds, double endSeconds, int stepsPerBeat);
int fitKick (const juce::File& file, double startSeconds, double endSeconds);

// Anpassar kick och bas tillsammans mot hela slaget: renderar vår kick + bas (med ducking)
// och jämför volym i tre band, kickens svep och basens övertoner med referensens medelslag.
// pattern: "rolling", "galloping", "triplet" eller "offbeat" (basens steg i varje slag).
int fitBeat (const juce::File& file, double startSeconds, double endSeconds, const juce::String& pattern);

// Mäter en rendering (t.ex. ett preset) på samma sätt som fitBeat och jämför med referensen.
int compareBeat (const juce::File& file, double startSeconds, double endSeconds, const juce::String& pattern,
                 const juce::File& rendered);
