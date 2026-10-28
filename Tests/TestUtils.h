#pragma once

#include <cstdio>

inline int& testFailures()
{
    static int failures = 0;
    return failures;
}

template <typename Detail = double>
void check (bool condition, const char* what, Detail detail = {})
{
    if (! condition)
    {
        std::printf ("  FEL: %s (%g)\n", what, static_cast<double> (detail));
        ++testFailures();
    }
}

void runTransportClockTests();
void runEngineTests();
void runMasterTests();
void runModulationTests();
void runFmPresetTests();
void runPersistFmTests();
void runAutoFixTests();
void runFxTests();
