#pragma once

// Förkompilerad header för plugin-målen: JUCE:s tunga headers läses en gång per mål i stället för i varje .cpp-fil.
// Lägg bara hit sådant som nästan alla filer ändå inkluderar. Se CMakeLists.txt (target_precompile_headers).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
