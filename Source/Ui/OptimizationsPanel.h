#pragma once

#include "../Core/PluginBase.h"
#include "Theme.h"

// SETTINGS > OPTIMIZATIONS: hur ofta varje vy ritas om (fem steg, 5-40 bilder per sekund), om vyer med fyllda ytor fylls, hur ofta
// live-spektrumet analyseras, och REDUCE ANIMATIONS (GLOBAL: halverar allt). Allt sparas i settings.xml och gäller alla Kickbase-fönster;
// vyerna följer en ändring direkt (VisualTimer). Visas över hela SETTINGS-sidan; BACK går tillbaka.
class OptimizationsPanel : public juce::Component
{
public:
    OptimizationsPanel (PluginBase& processor, std::function<void()> onBack);

    void refresh(); // läser inställningarna (de kan ha ändrats någon annanstans, t.ex. SPEED på ANALYZER-fliken)
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        Settings::Visual visual;
        juce::Label name;
        juce::ComboBox fps;
        std::unique_ptr<juce::TextButton> fill;   // bara vyer som ritar fyllda ytor
        std::unique_ptr<juce::ComboBox> analyses; // bara ANALYZER-spektrumet
    };

    juce::TextButton backButton { "BACK" }, reduceAnimationsToggle { "REDUCE ANIMATIONS" }, collisionGradientToggle { "COLLISION GRADIENT" };
    juce::Label globalLabel, globalHint, viewsLabel;
    std::vector<std::unique_ptr<Row>> rows;
    int compact = -1; // smala kolumner (Kickbase FX): kortare texter i menyerna ("30/s"), -1 = inte bestämt än
    void setTexts (bool shortTexts);
};
