#include "InstrumentEditor.h"

namespace
{
    // Gränssnittet ritas i den här storleken (logiska punkter) och skalas sedan till fönstret. Standardskalan är lite under 1.
    constexpr int baseWidth = 1180, baseHeight = 1010;
    constexpr float defaultScale = 0.9f;
}

InstrumentEditor::InstrumentEditor (InstrumentProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setWantsKeyboardFocus (true); // Ctrl+Z / Ctrl+Y når editorn även när bara en ratt är klickad
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (root);
    for (auto* component : std::initializer_list<juce::Component*> { &header, &synths, &tools, &info, &keyboard })
        root.addAndMakeVisible (component);

    synths.addTab ("PATTERN", Theme::pattern, pattern, pattern.getGrooveBar(), PatternGrooveBar::width);
    synths.addTab ("KICK", Theme::kick, kick, kick.embedInTab(), kick.headerExtrasWidth());
    synths.addTab ("INSTRUMENT", Theme::bass, instruments, instruments.getHeader(), InstrumentHost::headerWidth);
    synths.addTab ("ANALYZER", Theme::lfo, analyzer);
    synths.addTab ("AI CHAT", Theme::gate, aiPage);
    synths.addTab ("LIVE CODE", Theme::gate, livePage);

    // EDIT på en rad i mönstret: till ljudfliken för just det instrumentet (kicken = KICK, annars INSTRUMENT med instrumentet valt).
    pattern.getSequencer().onEditInstrument = [this] (int layer)
    {
        if (layer == Pattern::kickLayer)
        {
            synths.setCurrentTab (1);
            return;
        }
        instruments.select (layer);
        synths.setCurrentTab (2);
    };
    pattern.getSequencer().onAddInstrument = [this] { instruments.addInstrument(); };

    // På/av direkt på kickens flik (instrumenten har sin på/av i flikraden, bredvid väljaren).
    synths.addPower (1, processor.apvts, ParamID::kickOn);

    tools.setSubTabs();
    tools.setStripLabel ("TOOLS");
    synths.crumbSuffix = [this] { return synths.getCurrentTab() == 3 ? analyzer.currentViewName() : juce::String(); };
    // B1: förhandsknapp per flik. Hela mönstret (PATTERN, ANALYZER, AI CHAT, LIVE CODE), bara kickraden (KICK), instrumentet utan mönster (INSTRUMENT).
    const auto wholePattern = [this] { processor.requestAudition (PluginBase::previewRequest (PluginBase::previewAllRows)); };
    const auto stopPattern = [this] { processor.requestAudition (PluginBase::previewStop); };
    synths.setPreview (0, wholePattern, stopPattern, "LISTEN - Plays the whole pattern. Hold to play, double-click to latch.");
    synths.setPreview (1, [this] { processor.requestAudition (PluginBase::previewRequest (Pattern::kickLayer)); }, stopPattern,
                       "LISTEN - Plays the kick row only. Hold to play, double-click to latch.");
    synths.setPreview (2, [this] { holdPreview.start (PluginBase::heldLayerAudition (instruments.getCurrent())); },
                       [this] { holdPreview.stop(); },
                       "LISTEN - Plays the selected instrument without the pattern. Hold to play, double-click to latch.");
    synths.setPreview (3, wholePattern, stopPattern, "LISTEN - Plays the whole pattern. Hold to play, double-click to latch.");
    synths.setPreview (4, wholePattern, stopPattern, "LISTEN - Plays the whole pattern. Hold to play, double-click to latch.");
    synths.setPreview (5, wholePattern, stopPattern, "LISTEN - Plays the whole pattern. Hold to play, double-click to latch.");

    tools.addTab ("EFFECTS", Theme::effects, effects);
    tools.addTab ("SIDECHAIN", Theme::mix, sidechain);
    tools.addTab ("LFO", Theme::lfo, lfo);
    tools.addTab ("ENV", Theme::lfo, env);
    tools.addTab ("MATRIX", Theme::lfo, matrix);
    tools.addTab ("MASTER", Theme::master, masterPage);
    tools.addTab ("SOUNDSTAGE", Theme::mix, soundstage);
    tools.addTab ("SETTINGS", Theme::textDim, settings);

    keyboard.setOctaveForMiddleC (5); // samma tonnamn som FL Studio (mittens C = C5)
    keyboard.setKeyWidth (14.0f);
    keyboard.setLowestVisibleKey (24);

    // Fönstret går att dra större och mindre; hela gränssnittet skalas med.
    setResizable (true, true);
    setResizeLimits (baseWidth * 6 / 10, baseHeight * 6 / 10, baseWidth * 16 / 10, baseHeight * 16 / 10);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    const auto scale = Settings::uiScale() > 0.0f ? Settings::uiScale() : defaultScale;
    setSize (juce::roundToInt ((float) baseWidth * scale), juce::roundToInt ((float) baseHeight * scale));
    sized = true;
    processor.applyEditorScale = [this] (float newScale) { setSize (juce::roundToInt ((float) baseWidth * newScale), juce::roundToInt ((float) baseHeight * newScale)); };
}

InstrumentEditor::~InstrumentEditor()
{
    processor.applyEditorScale = nullptr;
    if (sized && processor.editorScale > 0.0f)
        Settings::setUiScale (processor.editorScale); // storleken man dragit till gäller nästa gång
    setLookAndFeel (nullptr);
}

void InstrumentEditor::resized()
{
    const auto scale = root.fitTo (*this, baseWidth, baseHeight);
    if (sized)
        processor.editorScale = scale;

    auto area = juce::Rectangle<int> (0, 0, baseWidth, baseHeight).reduced (10);

    header.setBounds (area.removeFromTop (LogoPainter::height));
    area.removeFromTop (4);

    keyboard.setBounds (area.removeFromBottom (50));
    area.removeFromBottom (6);
    info.setBounds (area.removeFromBottom (22));
    area.removeFromBottom (6);

    synths.setBounds (area.removeFromTop (522));
    area.removeFromTop (6);
    tools.setBounds (area);
}
