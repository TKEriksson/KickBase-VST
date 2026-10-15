#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

// Mäter kick och bas i en ljudfil (t.ex. en referenslåt eller ett renderat preset):
// tempo, kickens pitch-svep, svans och envelope, basstegens tonhöjd, envelope och ljushet,
// och basgången sextondel för sextondel.
//
// startSeconds/endSeconds < 0 = välj automatiskt det parti där kick och bas spelar mest ensamma.
int analyzeFile (const juce::File& file, double startSeconds, double endSeconds);
