#pragma once

#include "../Core/PluginBase.h"

// Provlyssning som hålls: ljudet klingar så länge musen är nere (och startar om om det tar slut) och släpps när musen släpps.
// Ett snabbt klick hörs ändå en kort stund (`minimumMs`), så att det går att kolla ljudet med ett tryck. Anropas från mouseDown / mouseUp.
// Det försenade stoppet är en Timer-medlem: den dör med gränssnittet. Förstörs objektet medan något hålls eller väntar skickas stoppet direkt.
class HoldAudition : private juce::Timer
{
public:
    explicit HoldAudition (PluginBase& p) : processor (p) {}

    ~HoldAudition() override
    {
        stopTimer();
        if (holding || pendingStop)
            processor.requestAudition (PluginBase::auditionHoldStop);
    }

    // `request` är PluginBase::heldSoloAudition (...) eller heldLayerAudition (...).
    void start (int request)
    {
        stopTimer(); // ett försenat stopp från förra trycket ska inte stoppa det här
        pendingStop = false;
        downTime = juce::Time::getMillisecondCounterHiRes();
        holding = true;
        processor.requestAudition (request);
    }

    void stop()
    {
        if (! holding)
            return;
        holding = false;
        const auto remaining = minimumMs - (juce::Time::getMillisecondCounterHiRes() - downTime);
        if (remaining <= 0.0)
        {
            processor.requestAudition (PluginBase::auditionHoldStop);
            return;
        }
        pendingStop = true;
        startTimer ((int) remaining + 1);
    }

private:
    void timerCallback() override
    {
        stopTimer();
        pendingStop = false;
        processor.requestAudition (PluginBase::auditionHoldStop);
    }

    static constexpr double minimumMs = 300.0;
    PluginBase& processor;
    double downTime = 0.0;
    bool holding = false, pendingStop = false;
};
