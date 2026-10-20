#pragma once

#include "../Core/PluginBase.h"

// Provlyssning som hålls: ljudet klingar så länge musen är nere (och startar om om det tar slut) och släpps när musen släpps.
// Ett snabbt klick hörs ändå en kort stund (`minimumMs`), så att det går att kolla ljudet med ett tryck. Anropas från mouseDown / mouseUp.
class HoldAudition
{
public:
    explicit HoldAudition (PluginBase& p) : processor (p) {}
    ~HoldAudition() { stop(); }

    // `request` är PluginBase::heldSoloAudition (...) eller heldLayerAudition (...).
    void start (int request)
    {
        ++*epoch; // ett försenat stopp från förra trycket ska inte stoppa det här
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
        // Processorn lever längre än gränssnittet; epoken skyddar mot ett nytt tryck under tiden.
        auto& target = processor;
        auto counter = epoch;
        const auto expected = *epoch;
        juce::Timer::callAfterDelay ((int) remaining, [&target, counter, expected]
        {
            if (*counter == expected)
                target.requestAudition (PluginBase::auditionHoldStop);
        });
    }

private:
    static constexpr double minimumMs = 300.0;
    PluginBase& processor;
    std::shared_ptr<int> epoch = std::make_shared<int> (0);
    double downTime = 0.0;
    bool holding = false;
};
