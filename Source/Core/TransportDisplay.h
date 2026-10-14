#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TransportStatus.h"

// Visar tempo, takt och slag så att man kan se att plugin:et följer DAW:en.
class TransportDisplay : public juce::Component, private juce::Timer
{
public:
    explicit TransportDisplay (const TransportStatus& statusIn, bool showNoteIn)
        : status (statusIn), showNote (showNoteIn)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff2a2c36));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

        const auto following = status.followingHost.load();

        auto lines = juce::StringArray {
            following ? "DAW spelar" : "DAW stoppad - egen klocka",
            juce::String (status.bpm.load(), 1) + " BPM",
            "Takt " + juce::String (status.bar.load()) + "  Slag " + juce::String (status.beat.load()),
        };

        if (showNote)
        {
            const auto note = status.heldNote.load();
            lines.add ("Ton: " + (note >= 0 ? juce::MidiMessage::getMidiNoteName (note, true, true, 5) : juce::String ("-")));
        }

        auto area = getLocalBounds().reduced (12, 8);
        const auto lineHeight = area.getHeight() / lines.size();

        g.setFont (juce::FontOptions (16.0f));
        for (int i = 0; i < lines.size(); ++i)
        {
            g.setColour (i == 0 ? (following ? juce::Colours::lightgreen : juce::Colours::orange) : juce::Colours::white);
            g.drawText (lines[i], area.removeFromTop (lineHeight), juce::Justification::centredLeft);
        }
    }

private:
    void timerCallback() override { repaint(); }

    const TransportStatus& status;
    const bool showNote;
};
