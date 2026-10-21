#include "TestUtils.h"

int main()
{
    runTransportClockTests();
    runEngineTests();
    runMasterTests();
    runModulationTests();
    runFmPresetTests();
    runPersistFmTests();

    if (testFailures() > 0)
    {
        std::printf ("\n%d test(er) misslyckades\n", testFailures());
        return 1;
    }

    std::puts ("\nAlla tester gick igenom");
    return 0;
}
