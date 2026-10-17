#pragma once

#include "Theme.h"

// Rotkomponenten i en editor: allt gränssnitt ligger i den, och den skalas så att hela gränssnittet följer fönstrets storlek (man kan
// dra i hörnet för att göra det större eller mindre). Editorn själv får inte ha en egen transform (värden skalar den), därför ligger
// skalningen här.
class EditorRoot : public juce::Component
{
public:
    void paint (juce::Graphics& g) override { Theme::paintBackground (g, getLocalBounds()); }

    // Passar gränssnittet (som är ritat i `baseWidth` x `baseHeight`) till editorns storlek. Ger skalan.
    float fitTo (const juce::Component& editor, int baseWidth, int baseHeight)
    {
        const auto scale = (float) editor.getWidth() / (float) baseWidth;
        setTransform (juce::AffineTransform::scale (scale));
        setBounds (0, 0, baseWidth, baseHeight);
        return scale;
    }
};
