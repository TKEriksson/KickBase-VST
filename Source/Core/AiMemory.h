#pragma once

#include <juce_core/juce_core.h>

namespace AiMemory
{
    constexpr int allEntries = -1;
    constexpr int promptTokenLimit = 8000;

    juce::File file();
    juce::File workFile();
    juce::var readRecent (const juce::File& file, int limit, juce::String& error);
    juce::var readRecent (int limit, juce::String& error);
    juce::var readRecentWork (const juce::File& file, int limit, juce::String& error);
    juce::var readRecentWork (int limit, juce::String& error);
    juce::String promptContext();
    juce::String promptContext (const juce::File& file, int limit);
    juce::String promptContext (const juce::File& file, int limit, bool work);
    bool appendWorkLesson (const juce::File& file, const juce::var& lesson, juce::String& error);
    bool appendWorkLesson (const juce::var& lesson, juce::String& error);
    bool appendFeedback (const juce::File& file, const juce::var& feedback, juce::String& error);
    bool appendFeedback (const juce::var& feedback, juce::String& error);
    bool trimToLimit (const juce::File& file, int maxMegabytes, juce::String& error);
    bool trimToSize (const juce::File& file, juce::int64 maxBytes, juce::String& error);
    bool trimWorkToLimit (const juce::File& file, int maxMegabytes, juce::String& error);
    int estimatedPromptTokens();
    bool clear (const juce::File& file, juce::String& error);
    bool clear (juce::String& error);
    bool clearWork (const juce::File& file, juce::String& error);
    bool clearWork (juce::String& error);
}
