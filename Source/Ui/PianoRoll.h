#pragma once

#include "VisualTimer.h"

#include <map>
#include <set>
#include "../Core/PluginBase.h"
#include "Theme.h"
#include "Cursors.h"
#include "ListenButton.h"

// Rullningslist där man även kan dra i tummens ändar för att zooma (som i FL Studio): dra den ena kanten för att se mer eller mindre.
class ZoomScrollBar : public juce::ScrollBar
{
public:
    using ScrollBar::ScrollBar;
    // start och storlek i listans enhet; ändrar vyn utan att tummen hoppar
    std::function<void (double start, double size)> onZoom;

    void mouseMove (const juce::MouseEvent& e) override { setMouseCursor (edgeAt (e) != 0 ? edgeCursor() : juce::MouseCursor::NormalCursor); ScrollBar::mouseMove (e); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        edge = edgeAt (e);
        if (edge == 0)
            return ScrollBar::mouseDown (e);
        startAtDown = getCurrentRangeStart();
        sizeAtDown = getCurrentRangeSize();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (edge == 0)
            return ScrollBar::mouseDrag (e);
        const auto length = (double) std::max (1, isVertical() ? getHeight() : getWidth());
        const auto total = getMaximumRangeLimit() - getMinimumRangeLimit();
        const auto d = (double) (isVertical() ? e.getDistanceFromDragStartY() : e.getDistanceFromDragStartX()) * total / length;
        auto start = startAtDown, size = sizeAtDown;
        if (edge < 0) { start += d; size -= d; }
        else          { size += d; }
        size = juce::jlimit (total * 0.02, total, size);
        if (edge < 0)
            start = startAtDown + sizeAtDown - size;
        start = juce::jlimit (getMinimumRangeLimit(), getMaximumRangeLimit() - size, start);
        if (onZoom)
            onZoom (start, size);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (edge == 0) ScrollBar::mouseUp (e); edge = 0; }

private:
    // -1 = början, 1 = slutet av tummen, 0 = annat
    int edgeAt (const juce::MouseEvent& e) const
    {
        const auto total = getMaximumRangeLimit() - getMinimumRangeLimit();
        const auto length = (double) (isVertical() ? getHeight() : getWidth());
        if (total <= 0.0 || length <= 0.0)
            return 0;
        const auto pos = (double) (isVertical() ? e.position.y : e.position.x);
        const auto from = (getCurrentRangeStart() - getMinimumRangeLimit()) / total * length;
        const auto to = (getCurrentRangeStart() + getCurrentRangeSize() - getMinimumRangeLimit()) / total * length;
        const auto grab = std::min (7.0, (to - from) * 0.35);
        if (std::abs (pos - from) <= grab) return -1;
        if (std::abs (pos - to) <= grab) return 1;
        return 0;
    }
    juce::MouseCursor edgeCursor() const { return isVertical() ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::LeftRightResizeCursor; }

    int edge = 0;
    double startAtDown = 0.0, sizeAtDown = 0.0;
};

// Pianorullen för ett baslager, som i FL Studio: tonerna ligger fritt i tiden (1/4 ... 1/64 och
// trioler) och i höjd, ±96 halvtoner från tonarten.
//
// Tre verktyg (knapparna överst): PENNA (klicka i tomt: ny ton; dra för att flytta den; dra en tons högerkant: längd),
// MARKERA (dra en ruta i tomt, som på skrivbordet; klicka på en ton) och SUDD (klicka eller dra över toner för att ta bort dem).
// Skift + klick/dra gör att MARKERA-knappen trycks in och markerar med en ruta, vilket verktyg som än är valt.
// - Skift + klick på en ton: lägg till/ta bort i markeringen.
//   Ctrl + A markerar alla. Markerade toner flyttas, förlängs och tas bort tillsammans.
// - Kopiera (Ctrl + C eller COPY, syns när något är markerat) och klistra in (Ctrl + V eller PASTE): de inklistrade tonerna visas
//   med annan färg och röd kant, först där de kopierade slutar. Klicka eller dra för att välja var; OK (Enter) lägger dem dit,
//   CANCEL (Esc) avbryter. Ctrl + X klipper ut.
// - Högerklicka på en ton: ta bort. GLIDE-knappen: de toner man ritar blir glide-toner (som slide-toner i FL Studio):
//   en glide-ton slår inte an utan glider tonen som klingar till sin tonhöjd under sin egen längd, och tonen blir kvar
//   där tills den slutar. Dubbelklicka på en ton: glide på/av för den.
// - Piltangenter flyttar de markerade (upp/ner = halvton, skift = oktav, vänster/höger = ett snap-steg).
// - Skroll: tonhöjd. Ctrl + skroll: zooma i tid. Shift + skroll: flytta i tid.
// - Verktygsraden: SNAP av/på, rutnätet (snap) 1/4 ... 1/64, radhöjd, markera alla och ta bort, GLIDE, samt
//   PRESET (spara och ladda pianorullar) och MIDI (exportera och importera MIDI-filer).
// - Remsan längst ner är velocity: dra i en tons stapel.
// - Mellanslag startar och stoppar förhandsspelningen av hela synthen; Skift + mellanslag spelar bara den här pianorullen (som LISTEN).
//   MIDI-in-rutan (uppe till höger) väljer vad tangentbordet/MIDI spelar medan pianorullen är öppen: bara den här raden (standard) eller alla.
//   Tipsraden allra längst ner visar genvägarna.
class PianoRoll : public juce::Component, public juce::SettableTooltipClient, private VisualTimer, private juce::ScrollBar::Listener
{
public:
    PianoRoll (PluginBase& processor, std::vector<juce::Colour> layerColours);
    ~PianoRoll() override;

    void setLayer (int newLayer);

    // Anropas när pianorullens toner tagits bort (lagret går tillbaka till stegmönstret).
    std::function<void()> onDeleted;

    // D5: POP OUT-knappen (pianorullen i eget fönster). Texten byts till DOCK medan rullen är utbruten.
    std::function<void()> onPopOut;
    void setPoppedOut (bool popped) { popOutButton.setButtonText (popped ? "DOCK" : "POP OUT"); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;
    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override { repaint(); }

private:
    enum class Drag { none, move, resize, velocity, box, erase, paste, ruler, keys, pan, stroke };
    enum class Tool { draw, select, erase };

    struct NoteData
    {
        int length = 17, pitch = 0;
        float velocity = 0.8f;
        bool slide = false;
        Pattern::Chord chord {};
    };
    // En ton i en stapel (polyfoni): starttick + index (0 = huvudtonen med ackordet, 1..3 = staplade toner; index kan ha hål).
    struct NoteKey
    {
        int tick = -1, index = 0;
        bool valid() const { return tick >= 0; }
        friend bool operator< (const NoteKey& a, const NoteKey& b) { return a.tick != b.tick ? a.tick < b.tick : a.index < b.index; }
        friend bool operator== (const NoteKey& a, const NoteKey& b) { return a.tick == b.tick && a.index == b.index; }
        friend bool operator!= (const NoteKey& a, const NoteKey& b) { return ! (a == b); }
    };
    using NoteMap = std::map<NoteKey, NoteData>;
    using KeySet = std::set<NoteKey>;
    struct Relocated { NoteMap target; std::map<NoteKey, NoteKey> where; bool full = false; }; // var varje flyttad ton hamnade

    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar* bar, double newRangeStart) override;
    void updateScrollBars();

    int bars() const;
    int totalTicks() const { return bars() * Pattern::ticksPerBar; }
    // Redigerbart område: raden plus takten efter slutet (ett klick där förlänger raden: längden följer innehållet).
    int editTicks() const { return std::min (Pattern::maxTicks, (bars() + extraViewBar()) * Pattern::ticksPerBar); }
    int extraViewBar() const { return processor.pattern.isLengthAuto (layer) ? 1 : 0; } // en låst rad slutar där den slutar: ingen tom takt efter
    // Vyn visar en takt till efter radens slut (tills 64 takter): klicka där, eller + BAR, för att förlänga raden.
    int viewTicks() const { return std::min (Pattern::maxBars, bars() + extraViewBar()) * Pattern::ticksPerBar; }
    void extendRow();                         // + BAR: raden blir en takt längre (tills innehållet nått dit; se Pattern::setExtraBars)
    InstrumentGrid grid() const;
    int snapTicks() const;
    int snapped (float tick) const;           // närmaste steg (nedåt) i ticks
    int rootNote() const;                     // basens grundton (midi)
    int rootPitchClass() const;

    float ticksToX (float tick) const;
    float xToTicks (float x) const;
    float pitchToY (int pitch) const;         // överkanten av tonens rad
    int yToPitch (float y) const;
    float rowHeight() const { return rowH; }
    int visibleRows() const { return std::max (1, (int) (gridArea.getHeight() / rowHeight())); }
    void clampView();
    void fitToWidth();

    juce::Rectangle<float> noteBounds (NoteKey key) const;
    NoteKey noteAtPoint (juce::Point<float> p, bool& onRightEdge) const;
    NoteKey noteForVelocity (float x) const;
    float velocityX (NoteKey key) const;
    juce::String describe (NoteKey key) const;
    bool liveNote (NoteKey key, NoteData& out) const;           // läser tonen i mönstret nu, false om den inte är på
    template <typename F> void forEachLive (int firstTick, int lastTick, F&& visit) const; // visit (NoteKey, const NoteData&), huvudtonen före de staplade
    void setLiveVelocity (NoteKey key, float velocity);
    void setLiveSlide (NoteKey key, bool slide);
    static void normalise (NoteMap& notes, std::map<NoteKey, NoteKey>* remap = nullptr);  // en stapel utan huvudton: lägsta staplade tonen blir huvudton
    static Relocated relocate (const NoteMap& source, const KeySet& moving, int deltaTicks, int deltaPitch); // full målstack eller samma tonhöjd på målticken avbryter hela flytten
    void removeKeys (const KeySet& keys);
    int voices() const;                              // lagrets VOICES (ackord hörs bara med 2 eller fler)
    void setChordOnSelection (const Pattern::Chord& chord);
    void changeVoices (int delta);                   // VOICES -/+ i verktygsraden (varnar om det blir färre än mönstret behöver)
    void setVoices (int count);
    void checkVoicesNeeded();                        // efter en redigering: behöver mönstret fler VOICES än lagret har? Fråga om att öka
    void setHover (NoteKey key);

    // Spolning och loop (ticks i radens tid): klick på linjalen flyttar startmarkören, drag eller dubbelklick markerar ett parti (rött) som förhandsspelningen loopar.
    void syncPreviewWindow();
    void clearPreviewWindow();
    void playThisRow();                              // förhandsspela bara den här raden från markören (eller loopen); igen = stopp
    void clearLoop();
    bool hasLoop() const { return loopStart >= 0 && loopEnd > loopStart; }

    // Provlyssning av en ton (klick på en tangent eller en ritad ton): hålls så länge musen hålls, minst tonens längd.
    void auditionStart (int pitch, float velocity);
    void auditionStop (int minimumTicks);
    int auditionPitch = -1;
    double auditionStartedAt = 0.0;
    int auditionVelocity = 100;
    int cursorTick = 0, loopStart = -1, loopEnd = -1, rulerAnchor = 0;
    bool rulerDragged = false;

    NoteMap readNotes() const;
    void applyNotes (const NoteMap& target); // skriver bara det som ändrats, så att ljudtråden aldrig ser ett tomt mönster
    void pruneSelection();
    void selectAll();
    void deleteSelected();
    void moveSelected (int deltaTicks, int deltaPitch);
    void changeRowHeight (float delta);

    // Verktyg, urklipp och inklistring.
    Tool effectiveTool() const;                      // skift gör att MARKERA gäller
    void setTool (Tool newTool);
    void updateToolButtons();
    void updateToolbarVisibility();
    void updateCursor (juce::Point<float> position, bool onNoteEdge);
    void eraseBetween (juce::Point<float> from, juce::Point<float> to);
    void copySelection (bool cut);
    void startPaste();
    void movePasteTo (juce::Point<float> position);
    void commitPaste();
    void cancelPaste();
    juce::Rectangle<float> ghostBounds() const;
    void flash (const juce::String& text);
    void showPresetMenu();
    void showMidiMenu();
    void askForNameAndSave();
    void afterLoad (InstrumentGrid newGrid); // sätter rutnätet efter att en pianorulle laddats
    int soundingRoot() const;
    juce::String gridParameterId() const; // lagrets rutnät (kickens eget för kicken)
    int playTickNow() const;              // uppspelningsmarkören i radens ticks, eller -1
    juce::String layerName() const; // basens grundton med lagrets oktav (tonhöjden 0), som MIDI-tonnummer
    void commit(); // markerar lagret som pianorulle och talar om att mönstret ändrats
    int pitchBefore (int tick) const;
    void attachGridBox();

    PluginBase& processor;
    std::vector<juce::Colour> layerColours;
    int lastColourVersion = 0; // processor.layerStructureVersion
    std::pair<uint64_t, uint64_t> liveNow { 0, 0 }; // tonerna som spelas live (MIDI IN: INSTRUMENT): MIDI-ton 0..63 och 64..127
    int layer = 0;

    juce::Rectangle<float> toolbarArea, keyArea, rulerArea, gridArea, velocityArea;

    // Verktygsraden.
    juce::TextButton snapButton { "SNAP" }, smallerButton { "-" }, biggerButton { "+" }, allButton { "SELECT ALL" }, deleteButton { "DEL" },
                     glideButton { "GLIDE" }, followButton { "FOLLOW" }, ghostButton { "GHOST" }, presetButton { "PRESET" }, midiButton { "MIDI" },
                     drawButton { "PENCIL" }, selectButton { "SELECT" }, eraseButton { "ERASER" }, copyButton { "COPY" },
                     pasteButton { "PASTE" }, pasteOkButton { "OK" }, pasteCancelButton { "CANCEL" }, popOutButton { "POP OUT" }, helpButton;
    ListenButton listenButton; // högtalare: håll för att höra pianorullen (loopar), släpp för att sluta
    Tool tool = Tool::draw;
    bool shiftHeld = false, altHeld = false;
    juce::MouseCursor pencilCursor = Cursors::pencil(), eraserCursor = Cursors::eraser();

    // Inklistring: de kopierade tonerna visas som ett spöke som man flyttar innan man bekräftar.
    bool pasting = false;
    int pasteStart = 0, pasteShift = 0, pasteGrabRow = 0, pasteGrabShift = 0;
    float pasteGrabTicks = 0.0f;
    juce::Point<float> lastErasePoint;
    juce::String flashText;
    double flashUntil = 0.0;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Component::SafePointer<juce::AlertWindow> saveDialog;
    juce::Rectangle<float> infoArea;
    juce::ComboBox gridBox;                                 // rutnät
    juce::ComboBox midiInBox;                               // MIDI in medan pianorullen är öppen: alla instrument eller bara den här raden
    juce::Rectangle<float> hintArea;                        // tipsraden längst ner
    void applyMidiIn();
    void showGhostMenu();                                   // GHOST NOTES: multival över raderna (menyn öppnas igen efter varje val)
    juce::TextButton lengthButton, extendButton { "+ BAR" };  // radens längd (AUTO n / valt antal: klick öppnar menyn) och förläng med en takt
    juce::TextButton voicesDownButton { "-" }, voicesUpButton { "+" };
    juce::Label voicesLabel;                                // "VOICES n"
    int askedVoicesNeed = 0;                                // största behov vi redan frågat om (så att samma nej inte upprepas)
    ZoomScrollBar hBar { false }, vBar { true }; // tid och tonhöjd
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gridAttachment;
    bool snapEnabled = true;

    float rowH = 13.0f;
    float pixelsPerTick = 1.0f;
    float scrollTicks = 0.0f;   // tiden längst till vänster
    juce::Point<float> panStart;  // mittenknapp: dra för att panorera
    float panScrollTicks = 0.0f, panTopPitch = 0.0f;
    // Glidestreck (D1): högerdrag på tomt rutnät. Ett högerklick utan drag visar menyn som förut.
    juce::Point<float> strokeStart, strokeEnd;
    bool strokeMenuNow = false;
    // Glide-plus: plus-ikon vid en glide-ton som startar ett glidestreck från notens slut; strecket fastnar på en ton under pekaren.
    NoteKey plusKey;
    bool plusBelow = true;
    bool strokeFromPlus = false;
    juce::Rectangle<float> plusRect() const;
    void updatePlus (juce::Point<float> position, NoteKey hovered);
    void addStroke (const juce::Point<float>& from, const juce::Point<float>& to);
    float topPitch = 12.0f;     // översta synliga raden (halvtoner från tonarten)
    NoteKey justCreated;        // tonen som det första klicket i ett dubbelklick skapade
    int lastLength = 0;         // längden på den senast ritade tonen: nästa ton får samma

    KeySet selection;           // de markerade tonerna (starttick + stackindex)

    Drag drag = Drag::none;
    NoteKey dragKey;            // tonen som greps (dess nuvarande nyckel)
    NoteKey dragStartKey;       // och när draget började
    int dragStartPitch = 0;
    float grabOffsetTicks = 0.0f;
    NoteMap dragSnapshot;       // alla toner när draget började
    KeySet dragSelection;       // de markerade när draget började
    juce::Point<float> boxStart, boxEnd;
    KeySet boxBase;             // markeringen innan rutan (skift lägger till)
    bool edited = false;
    juce::ValueTree undoBase;   // raden som den var före nästa ändring (tas vid musnedtryck/tangent, nollställs av commit)
    NoteKey hoverKey;
    juce::Point<float> hoverPoint; // var musen är (för pennans hårkors och förhandsvisningen av tonen som skulle läggas)
    bool hoverPointValid = false;
    bool hoverEdge = false;

    int lastVersion = -1, lastPlayTick = -2, lastBars = 0, lastGrid = -1;
};
