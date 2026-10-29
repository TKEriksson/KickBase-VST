#pragma once

#include "../Core/PluginBase.h"
#include "../Core/LibraryIndex.h"
#include "../Core/WavetablePack.h"
#include "Theme.h"
#include "OptimizationsPanel.h"
#include "AboutPanel.h"

// Fliken SETTINGS: inställningar som gäller alla Kickbase-fönster och projekt (de sparas i Dokument\Kickbase\settings.xml):
// fönstrets storlek, AI-länken med egen åtkomstnyckel, mapparna för presets, exporter, wavetables och biblioteket, och installationen av
// vågformspaketet AKWF. OPTIMIZATIONS (hur ofta vyerna ritas) och ABOUT (licens och tredjepartsprogram) öppnas med varsin knapp och visas då över hela sidan.
class SettingsPage : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    explicit SettingsPage (PluginBase& processor);
    ~SettingsPage() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct FolderRow
    {
        juce::String key, title;
        juce::Label name, path;
        juce::TextButton change { "CHANGE" }, reset { "RESET" }, open { "OPEN" };
    };

    void timerCallback() override;
    void visibilityChanged() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { updateLibraryRow(); }
    void updateLibraryRow(); // LIBRARY-raden: mappen och hur många ljud och paket indexet fann
    void refresh (bool includeFiles);
    void chooseFolder (FolderRow& row);
    void setBridge (bool on);
    void updatePack();
    void updateGlowButton();
    void showPanel (juce::Component* panel); // OPTIMIZATIONS eller ABOUT över hela sidan; nullptr = tillbaka
    bool panelShown() const { return optimizations.isVisible() || about.isVisible(); } // sidan under en panel gör ingenting

    juce::SharedResourcePointer<LibraryIndex> libraryIndex; // först: förstörs sist (lyssnaren tas bort i destruktorn)
    PluginBase& processor;
    std::unique_ptr<juce::FileChooser> chooser;
    int tick = 0;
    bool filesShown = false; // mapparna och vågformerna har lästs sedan sidan senast blev synlig

    // FÖNSTER
    juce::Label scaleLabel, windowHint;
    juce::TextButton optimizationsButton { "OPTIMIZATIONS..." }, aboutButton { "ABOUT" }, glowButton;
    juce::TextButton helpButton;
    OptimizationsPanel optimizations;
    AboutPanel about;
    std::vector<juce::Component::SafePointer<juce::Component>> hiddenByPanel; // det som var synligt när en panel öppnades
    juce::Slider scaleSlider;
    std::array<juce::TextButton, 4> sizeButtons;

    // AI-LÄNKEN
    juce::TextButton aiToggle { "AI LINK" }, showKeyButton { "SHOW" }, generateButton { "GENERATE" }, copyButton { "COPY" }, clearKeyButton { "CLEAR" };
    juce::Label aiStatus, keyLabel, aiHint;
    juce::TextEditor keyEditor;

    // MAPPAR
    std::array<FolderRow, 4> folders; // PRESETS, EXPORTS, WAVETABLES, LIBRARY (de två sista bara i instrumentet)
    juce::Label folderHint;

    // VÅGFORMSPAKETET
    WavetablePack pack;
    juce::Label packStatus, packCount, packHint;
    juce::TextButton installButton { "INSTALL AKWF PACK" }, cancelButton { "CANCEL" }, packFolderButton { "OPEN FOLDER" };
    double packProgress = 0.0;
    int installedWaveforms = 0; // räknas när sidan visas och efter en installation (att räkna går igenom tusentals filer på disken)
    juce::ProgressBar progressBar { packProgress };

    juce::Rectangle<int> windowBox, aiBox, folderBox, packBox;
};
