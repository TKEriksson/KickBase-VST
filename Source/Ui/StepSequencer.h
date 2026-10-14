#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"

// Stegsekvenseraren: en rad för kicken, en för basen och ett "lane" under där basstegens
// velocity, gate-längd, tonhöjd eller slide ritas och dras i.
//
// Raderna är tidsjusterade: kicken har alltid 16 steg per takt, basen 16, 12 eller 24
// beroende på rutnät, men en takt tar lika stor bredd i båda.
class StepSequencer : public juce::Component, private juce::Timer
{
public:
    enum class Lane { velocity, gate, pitch, slide };

    StepSequencer (PluginBase& processor, juce::Colour bassAccent, const juce::String& bassName);

    void setLane (Lane newLane) { lane = newLane; repaint(); }
    Lane getLane() const        { return lane; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class Target { none, kick, bass, lane };

    void timerCallback() override;

    int bars() const;
    BassGrid grid() const;
    int kickSteps() const { return bars() * Pattern::kickStepsPerBar; }
    int bassSteps() const { return bars() * Pattern::bassStepsPerBar (grid()); }

    Target targetAt (juce::Point<int> point) const;
    int stepAt (int x, int numSteps) const;
    juce::Rectangle<float> stepBounds (juce::Rectangle<int> row, int step, int numSteps) const;

    void paintGrid (juce::Graphics&, juce::Rectangle<int> area) const;
    void paintToggleRow (juce::Graphics&, juce::Rectangle<int> row, int numSteps, juce::Colour colour,
                         int playingStep, const std::function<bool (int)>& isOn) const;
    void paintLane (juce::Graphics&) const;

    void editAt (juce::Point<int> point, bool firstEvent);
    float laneValueAt (int y) const;
    juce::String laneName() const;
    juce::String describeStep (int step) const;

    PluginBase& processor;
    juce::Colour bassColour;
    juce::String bassName;
    Lane lane = Lane::velocity;

    juce::Rectangle<int> labelColumn, kickRow, bassRow, laneArea;

    Target dragTarget = Target::none;
    bool paintOn = true;        // vad ett drag över av/på-steg sätter
    int dragStartPitch = 0, dragStartY = 0, dragPitch = 0;
    bool edited = false;
    int hoverStep = -1;

    int lastKickStep = -2, lastBassStep = -2, lastBars = 0, lastGrid = -1, lastVersion = -1;
};
