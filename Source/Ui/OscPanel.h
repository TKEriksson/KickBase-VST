#pragma once

#include "VisualTimer.h"
#include <map>
#include <juce_dsp/juce_dsp.h>

#include "../Core/PluginBase.h"
#include "Components.h"
#include "Cursors.h"
#include "HoldAudition.h"
#include "../Core/OscPresets.h"

// Bilden av en wavetable, som i Serum: alla vågformer staplade bakåt i en svag 3D-vy, och den
// som spelas nu (positionen, med LFO och warp) ritad framför i lagrets färg.
class WavetableView : public juce::Component, public juce::SettableTooltipClient, private VisualTimer
{
public:
    WavetableView (PluginBase& processor, int layer, int osc, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { hold.stop(); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }

private:
    void timerCallback() override;
    float raw (InstrumentField field) const;

    enum class Display { wave, scope, spectrum };
    juce::Rectangle<float> modeRect() const { return { (float) getWidth() - 62.0f, 3.0f, 58.0f, 14.0f }; }
    void drawCurrent (juce::Graphics&, const Wavetable&, juce::Rectangle<float> area);
    Display display = Display::wave;
    juce::dsp::FFT fft { 9 };
    bool hover = false;
    juce::MouseCursor speakerCursor = Cursors::speaker();

    PluginBase& processor;
    HoldAudition hold { processor }; // klick och håll: oscillatorn klingar så länge musen är nere
    const int layer, osc;
    juce::Colour accent;
    int positionIndex = -1;
    float lastPosition = -1.0f, lastWarp = -1.0f;
    int lastTable = -1, lastWarpMode = -1;
    bool lastMorph = true;
    float lastDuckDb = 0.0f; // FUNDAMENTAL DUCK: hur många dB motorns grundton dämpas nu (0 = ingen dämpning)
    bool lastReady = false; // tabellen var byggd vid förra ritningen (byggs den i bakgrunden ritas vyn om när den blir klar)
};

// En oscillator (OSC 1, 2 eller 3) i ett baslager: tabell, warp, bilden och rattarna.
class OscPanel : public juce::Component, private juce::Timer
{
public:
    OscPanel (PluginBase& processor, int layer, int osc, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseTable (int index);
    void showTableMenu();
    void loadFromLibrary (const juce::File& fileOrFolder);
    void chooseWavFile();
    juce::String currentTableName() const;
    std::vector<juce::File> menuFiles; // biblioteksval i menyn (id 1000 +)
    // Fas S5: OSC-presets (.oscpreset) för just den här oscillatorn.
    void showPresetMenu();
    void stepPreset (int direction);
    void loadPreset (const juce::File& file);
    void askForNameAndSave();
    juce::TextButton prevPresetButton { "<" }, nextPresetButton { ">" }, presetButton { "PRESET" }, savePresetButton { "SAVE" }, resetOscButton { "RESET" };
    juce::File currentPreset;
    juce::TextButton helpButton;
    juce::Component::SafePointer<juce::AlertWindow> saveDialog;

    PluginBase& processor;
    const int layer, osc;
    juce::Colour accent;
    ChoiceComponent warp;
    juce::Label tableLabel;
    juce::TextButton tableButton;
    std::array<juce::TextButton, 4> shapeButtons; // SIN, TRI, SAW, SQR: de klassiska vågformerna
    juce::ToggleButton filterSendButton { "SEND THROUGH FILTER" }; // samma parameter som i FILTER-fliken (THROUGH FILTER-menyn)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filterSendAttachment;
    juce::TextButton wavButton { "WAV" };
    juce::TextButton morphButton { "MORPH" };
    std::unique_ptr<juce::ButtonParameterAttachment> morphAttachment;
    std::unique_ptr<juce::FileChooser> chooser;
    // RING (B) (bara OSC 1): visar vad OSC 1 multipliceras med och låter en välja; ersätter formknapparna så länge warp är RING (B).
    juce::ComboBox ringBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ringAttachment;
    bool ringShown = false;
    int lastTable = -1;
    WavetableView view;
    std::vector<std::unique_ptr<KnobComponent>> knobs;
    std::unique_ptr<KnobComponent> warpKnob; // WARP-mängden sitter bredvid WARP-valet (menyn), inte bland de övriga rattarna
};
