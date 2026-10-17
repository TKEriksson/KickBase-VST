#pragma once

#include "../Core/PluginBase.h"
#include "../Core/WavetablePack.h"
#include "Theme.h"

// Fliken SETTINGS: inställningar som gäller alla Kickbase-fönster och projekt (de sparas i Dokument\Kickbase\settings.xml):
// fönstrets storlek, AI-länken med egen åtkomstnyckel, mapparna för presets, exporter och wavetables, och installationen av
// vågformspaketet AKWF.
class SettingsPage : public juce::Component, private juce::Timer
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
    void refresh (bool includeFiles);
    void chooseFolder (FolderRow& row);
    void setBridge (bool on);
    void updatePack();

    PluginBase& processor;
    std::unique_ptr<juce::FileChooser> chooser;
    int tick = 0;

    // FÖNSTER
    juce::Label scaleLabel, windowHint;
    juce::Slider scaleSlider;
    std::array<juce::TextButton, 4> sizeButtons;

    // AI-LÄNKEN
    juce::TextButton aiToggle { "AI LINK" }, showKeyButton { "SHOW" }, generateButton { "GENERATE" }, copyButton { "COPY" }, clearKeyButton { "CLEAR" };
    juce::Label aiStatus, keyLabel, aiHint;
    juce::TextEditor keyEditor;

    // MAPPAR
    std::array<FolderRow, 3> folders;
    juce::Label folderHint;

    // VÅGFORMSPAKETET
    WavetablePack pack;
    juce::Label packStatus, packCount, packHint;
    juce::TextButton installButton { "INSTALL AKWF PACK" }, cancelButton { "CANCEL" }, packFolderButton { "OPEN FOLDER" };
    double packProgress = 0.0;
    juce::ProgressBar progressBar { packProgress };

    juce::Rectangle<int> windowBox, aiBox, folderBox, packBox;
};
