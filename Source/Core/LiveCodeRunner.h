#pragma once

#include "LiveCode.h"

// Live-läget: kör Strudel-koden i en bakgrundstråd (aldrig på ljudtråden eller message thread) och
// lämnar resultatet på message thread. Mönstret spelas av den vanliga schemaläggaren som en loop på
// `cycles` takter, så "live" betyder att varje kodändring räknas om och ersätter loopen utan att
// transporten stannar. Nyare begäran ersätter äldre: ett gammalt svar som kommer försent kastas.
class LiveCodeRunner
{
public:
    // list = set_pattern-argument, mods = .lfo/.env, writes = {id, value} från .set; error är tomt om körningen lyckades.
    using Done = std::function<void (const juce::var& list, const juce::String& error, const juce::var& mods, const juce::var& writes, const juce::StringArray& console)>;

    ~LiveCodeRunner() { generation.fetch_add (1); pool.removeAllJobs (true, 2000); }

    void run (const juce::String& code, int cycles, Done done, std::map<int, double> sliders = {}, std::map<juce::String, double> params = {}, juce::var effects = {})
    {
        const auto gen = generation.fetch_add (1) + 1;
        pool.removeAllJobs (false, 0);
        pool.addJob ([this, code, cycles, gen, sliders = std::move (sliders), params = std::move (params), effects = std::move (effects), done = std::move (done)]
        {
            LiveCode live;
            live.setSliderValues (sliders);
            live.setParams (params);
            live.setEffects (effects);
            juce::Result res = juce::Result::ok();
            auto list = live.create (code, cycles, res);
            const auto mods = live.getMods();
            auto writes = live.takeParamWrites();
            auto console = live.takeConsole();
            juce::MessageManager::callAsync ([this, gen, list, res, mods, writes, console, done]
            {
                if (gen == generation.load()) done (list, res.failed() ? res.getErrorMessage() : juce::String(), mods, writes, console);
            });
        });
    }

    void cancel() { generation.fetch_add (1); }

private:
    std::atomic<int> generation { 0 };
    juce::ThreadPool pool { 1 };
};
