#include "OscPresets.h"
#include "Settings.h"
#include "Params.h"

namespace OscPresets
{
    namespace
    {
        using F = InstrumentField;

        // Allt oscillatorn äger utom aOn (av/på hör till instrumentet, inte till ljudet).
        const std::array<F, 14> fields { F::aTable, F::aPos, F::aLevel, F::aSemi, F::aFine, F::aUnison, F::aDetune, F::aBlend,
                                         F::aWarp, F::aWarpAmount, F::aPhase, F::aRandom, F::aPan, F::aStereo };

        juce::RangedAudioParameter* parameter (juce::AudioProcessorValueTreeState& apvts, int layer, int osc, F field)
        {
            return apvts.getParameter (ParamID::bass (layer, ParamID::oscField (osc, field)));
        }

        juce::RangedAudioParameter* morph (juce::AudioProcessorValueTreeState& apvts, int layer, int osc)
        {
            return apvts.getParameter (ParamID::bass (layer, ParamID::oscMorphField (osc)));
        }

        void setPlain (juce::RangedAudioParameter* p, float plain)
        {
            if (p == nullptr)
                return;
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
            p->endChangeGesture();
        }

        void setNormalised (juce::RangedAudioParameter* p, float value)
        {
            if (p == nullptr)
                return;
            p->beginChangeGesture();
            p->setValueNotifyingHost (value);
            p->endChangeGesture();
        }
    }

    juce::File folder()
    {
        return Settings::presetsFolder().getChildFile ("Oscillators");
    }

    void ensureFactory()
    {
        if (folder().exists())
            return; // mappen finns: användaren har redan egna (eller har raderat våra), vi skriver aldrig över
        struct Factory { const char* category; const char* name; const char* table; double pos, level, semi, fine; int unison; double detune, blend; int warp; double warpAmount, phase, random, pan, stereo; };
        // Warp-index 0-4 (Off, Sync, Bend, PWM, Mirror) är samma i alla tre oscillatorerna, så presetsen fungerar var de än laddas.
        static const Factory presets[] {
            { "Sub",  "Sub Sine",        "Basic",          0.0,  1.0,   0.0, 0.0, 1,  0.0, 0.5, 0, 0.0, 0.0, 0.0, 0.0, 0.0 },
            { "Sub",  "Sub Soft Tri",    "Basic",          0.34, 1.0, -12.0, 0.0, 1,  0.0, 0.5, 0, 0.0, 0.0, 0.0, 0.0, 0.0 },
            { "Bass", "Rolling Saw",     "Saw Soft",       0.5,  0.9,   0.0, 0.0, 1,  0.0, 0.5, 0, 0.0, 0.0, 0.0, 0.0, 0.5 },
            { "Bass", "Rolling Square",  "Square Soft",    0.5,  0.9,   0.0, 0.0, 1,  0.0, 0.5, 3, 0.25, 0.0, 0.0, 0.0, 0.5 },
            { "Bass", "Psy Roll",        "Psy Roll",       0.5,  0.9,   0.0, 0.0, 1,  0.0, 0.5, 0, 0.0, 0.0, 0.0, 0.0, 0.5 },
            { "Bass", "Psy Sub Punch",   "Psy Sub Punch",  0.3,  1.0,   0.0, 0.0, 1,  0.0, 0.5, 0, 0.0, 0.0, 0.0, 0.0, 0.0 },
            { "Reese","Reese Saw",       "Saw Soft",       0.5,  0.8,   0.0, 0.0, 5, 18.0, 0.5, 0, 0.0, 0.0, 1.0, 0.0, 0.9 },
            { "Reese","Reese Detuned",   "Saw Bright",     0.6,  0.8,   0.0, 0.0, 7, 30.0, 0.5, 0, 0.0, 0.0, 1.0, 0.0, 1.0 },
            { "Acid", "Acid Saw",        "Saw Bright",     0.7,  0.9,   0.0, 0.0, 1,  0.0, 0.5, 0, 0.0, 0.0, 0.0, 0.0, 0.3 },
            { "Acid", "Psy Acid",        "Psy Acid",       0.5,  0.9,   0.0, 0.0, 1,  0.0, 0.5, 0, 0.0, 0.0, 0.0, 0.0, 0.3 },
            { "Growl","Growl Table",     "Growl",          0.5,  0.8,   0.0, 0.0, 3,  8.0, 0.5, 0, 0.0, 0.0, 0.5, 0.0, 0.7 },
            { "Growl","Fold Metal",      "Fold",           0.6,  0.7,   0.0, 0.0, 1,  0.0, 0.5, 4, 0.4, 0.0, 0.0, 0.0, 0.5 },
            { "Layer","Fifth Up",        "Basic",          0.5,  0.5,   7.0, 0.0, 1,  0.0, 0.5, 0, 0.0, 0.0, 0.0, 0.0, 0.5 },
            { "Layer","Octave Up Comb",  "Comb Sweep",     0.4,  0.5,  12.0, 0.0, 1,  0.0, 0.5, 0, 0.0, 0.0, 0.0, 0.0, 0.5 },
        };
        for (const auto& p : presets)
        {
            juce::XmlElement root ("KickbaseOsc");
            root.setAttribute ("version", 1);
            root.setAttribute ("Table", p.table);
            root.setAttribute ("Pos", p.pos);
            root.setAttribute ("Level", p.level);
            root.setAttribute ("Semi", p.semi);
            root.setAttribute ("Fine", p.fine);
            root.setAttribute ("Unison", p.unison);
            root.setAttribute ("Detune", p.detune);
            root.setAttribute ("Blend", p.blend);
            root.setAttribute ("Warp", p.warp);
            root.setAttribute ("WarpAmount", p.warpAmount);
            root.setAttribute ("Phase", p.phase);
            root.setAttribute ("Random", p.random);
            root.setAttribute ("Pan", p.pan);
            root.setAttribute ("Stereo", p.stereo);
            const auto file = folder().getChildFile (p.category).getChildFile (juce::String (p.name) + ".oscpreset");
            file.getParentDirectory().createDirectory();
            root.writeTo (file);
        }
    }

    juce::Array<juce::File> list()
    {
        juce::Array<juce::File> result;
        auto top = folder().findChildFiles (juce::File::findFiles, false, "*.oscpreset");
        top.sort();
        auto categories = folder().findChildFiles (juce::File::findDirectories, false);
        categories.sort();
        for (const auto& category : categories)
        {
            auto files = category.findChildFiles (juce::File::findFiles, false, "*.oscpreset");
            files.sort();
            result.addArray (files);
        }
        result.addArray (top);
        return result;
    }

    bool save (juce::AudioProcessorValueTreeState& apvts, int layer, int osc, const juce::File& file)
    {
        juce::XmlElement root ("KickbaseOsc");
        root.setAttribute ("version", 1);
        for (const auto field : fields)
        {
            auto* p = parameter (apvts, layer, osc, field);
            if (p == nullptr)
                return false;
            const auto name = juce::String (ParamID::instrumentFieldName (ParamID::oscField (0, field))).substring (1); // "aPos" -> "Pos"
            if (field == F::aTable)
                root.setAttribute ("Table", p->getCurrentValueAsText()); // vid namn: tabellistan kan växa
            else
                root.setAttribute (name, (double) p->convertFrom0to1 (p->getValue()));
        }
        if (auto* m = morph (apvts, layer, osc))
            root.setAttribute ("Morph", m->getValue() >= 0.5f ? 1 : 0);

        file.getParentDirectory().createDirectory();
        return root.writeTo (file);
    }

    bool load (juce::AudioProcessorValueTreeState& apvts, int layer, int osc, const juce::File& file)
    {
        const auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("KickbaseOsc"))
            return false;

        for (const auto field : fields)
        {
            auto* p = parameter (apvts, layer, osc, field);
            if (p == nullptr)
                continue;
            const auto name = juce::String (ParamID::instrumentFieldName (ParamID::oscField (0, field))).substring (1);
            if (! xml->hasAttribute (field == F::aTable ? "Table" : name))
                continue; // ett fält som filen saknar lämnas orört
            if (field == F::aTable)
            {
                const auto choices = p->getAllValueStrings();
                const auto index = choices.indexOf (xml->getStringAttribute ("Table"));
                if (index >= 0)
                    setNormalised (p, p->convertTo0to1 ((float) index));
            }
            else
                setPlain (p, (float) xml->getDoubleAttribute (name));
        }
        if (xml->hasAttribute ("Morph"))
            setNormalised (morph (apvts, layer, osc), xml->getIntAttribute ("Morph") != 0 ? 1.0f : 0.0f);
        return true;
    }

    void reset (juce::AudioProcessorValueTreeState& apvts, int layer, int osc)
    {
        for (const auto field : fields)
            if (auto* p = parameter (apvts, layer, osc, field))
                setNormalised (p, p->getDefaultValue());
        if (auto* m = morph (apvts, layer, osc))
            setNormalised (m, m->getDefaultValue());
    }
}
