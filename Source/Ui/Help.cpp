#include "Help.h"
#include <map>

namespace Help
{
    juce::String forParameter (const juce::String& rawID)
    {
        // Baslagrens parametrar har samma förklaring i alla lager.
        auto paramID = rawID;
        if (paramID.startsWith ("bass") && paramID.length() > 4 && juce::CharacterFunctions::isDigit (paramID[4]))
            paramID = "bass" + paramID.substring (5);
        // OSC B har samma förklaringar som OSC A.
        if (paramID.startsWith ("bassB") && paramID.length() > 5 && juce::CharacterFunctions::isUpperCase (paramID[5]))
            paramID = "bassA" + paramID.substring (5);

        static const std::map<juce::String, juce::String> texts {
            // Mönster och mix
            { "patternBars", "How many bars the pattern lasts before it repeats." },
            { "noteEnd", "What happens when the DAW note ends in the middle of the pattern: stop at once, finish the bar or finish the whole pattern." },
            { "bassGate", "Scales the length of every step in this layer. Lower = shorter, punchier notes." },
            { "gain", "Output volume of the whole plugin." },
            { "bassDuck", "Sidechain depth for this layer: how far it is pushed down while the kick plays. 0 % = no ducking." },
            { "bassOn", "Turns this bass layer on or off." },
            { "bassGrid", "This layer's step grid: 1/16 for rolling basses, 1/8 T for triplet (K-B-B) grooves, and more." },

            // Kick
            { "kickKeyFollow", "Follow: the kick's tail is tuned to the note you play. Fixed: it stays on C1 + TUNE whatever the note." },
            { "kickTune", "Tunes the kick's tail (its low 'boom') in semitones. Psy kicks often sit 1-3 semitones above the bass note." },
            { "kickPitch", "How far above the tail the kick starts its pitch drop. More = harder, clickier punch." },
            { "kickPitchTime", "How long the fast pitch drop takes. Short = tight click, long = 'pew' laser drop." },
            { "kickPitchCurve", "Shape of the pitch drop. Higher = falls fast at first, then slowly." },
            { "kickGlide", "A second, slow pitch drop on top of the fast one: how far above the tail it starts." },
            { "kickGlideTime", "How long the slow pitch drop takes to reach the tail." },
            { "kickAttack", "Fade-in at the start of the kick. Keep it near 0 for a hard hit." },
            { "kickHold", "How long the kick stays at full volume before it decays." },
            { "kickDecay", "How long the kick's tail rings out. Long kicks need more ducking of the bass." },
            { "kickDecayCurve", "Shape of the decay. Higher = drops fast at first, shorter tail." },
            { "kickClick", "Level of the short click at the start (synthesized, or your sample)." },
            { "kickClickLength", "How long the click lasts." },
            { "kickClickTone", "Brightness of the synthesized click." },
            { "kickDrive", "Saturation on the kick: rounder and louder, more harmonics." },
            { "kickLevel", "Volume of the kick." },

            // Bas: oscillator 1
            { "bassOctave", "Moves the whole bass up or down in octaves." },
            { "bassTune", "Fine tunes the bass in semitones." },
            { "bassATable", "Which wavetable the oscillator plays: Basic (sine-triangle-saw-square), PWM, Sync, Harmonics, FM, Growl, Fold, Formant, Digital, Phase Dist." },
            { "bassAPos", "Position in the wavetable: which of its waveforms plays. Drag an LFO here to sweep through them for movement." },
            { "bassALevel", "How loud this oscillator is. OSC B at 0 is silent but can still frequency-modulate OSC A (WARP: FM)." },
            { "bassASemi", "Pitch in semitones (+12 = an octave up, +7 = a fifth)." },
            { "bassAFine", "Fine tuning in cents." },
            { "bassAUnison", "Number of stacked, slightly detuned voices. More = wider and fatter (use carefully in the low end)." },
            { "bassADetune", "How far apart the unison voices are tuned." },
            { "bassABlend", "Level of the outer unison voices compared to the centre one." },
            { "bassAWarp", "Bends the waveform before it is played: Sync (screaming), Bend, PWM (narrower), Mirror, FM (B) = OSC B modulates OSC A." },
            { "bassAWarpAmount", "How much the WARP mode changes the waveform." },
            { "bassAPhase", "Where in the waveform each note starts. The same start every note gives a tight, punchy attack." },
            { "bassARandom", "Randomises the start phase per note: looser and more analog, less punchy." },
            { "bassWarpEnv", "Extra warp on OSC A at the start of each note, following the filter envelope: a bright, digital 'knock' that fades." },
            { "bassSub", "Level of a pure sine sub under the bass, for weight in the low end." },
            { "bassSubOctave", "Sub in the same octave as the bass, or one octave lower (careful: very low notes get inaudible)." },
            { "bassNoise", "Adds white noise before the filter, for air and grit in the attack." },

            // Filter
            { "bassFilterType", "Filter type. LP 24 = smooth and dark (classic psy), LP 12 = brighter, Band and High for thin sounds." },
            { "bassCutoff", "Where the filter starts cutting the highs. Lower = darker bass." },
            { "bassResonance", "Boost at the cutoff frequency. High values give the squelchy, acid sound." },
            { "bassKeyTrack", "How much the cutoff follows the note: higher notes get a brighter filter." },
            { "bassEnvAmount", "How far (in octaves) the filter opens at the start of every note. The 'pluck' of a psy bass." },
            { "bassEnvDecay", "How fast the filter closes again after each note starts." },
            { "bassVelToEnv", "How much the step velocity changes the filter envelope. Accents get brighter." },
            { "bassDrive", "Saturation after the filter: warmer, louder, more harmonics." },

            // Amp
            { "bassAttack", "Fade-in of each bass note." },
            { "bassDecay", "How fast each note falls from full volume down to the sustain level." },
            { "bassSustain", "Volume while the note is held, after the decay." },
            { "bassRelease", "Fade-out after the note ends." },
            { "bassGlide", "Time to slide between notes on steps with SLIDE on." },
            { "bassLevel", "Volume of the bass." },

            // Gate (FX-versionen)
            { "fxKey", "The key of your song: the kick is tuned to it." },
            { "gateAttack", "How fast the gate opens on each step." },
            { "gateRelease", "How fast the gate closes after each step." },

            // Sidechain
            { "scMode", "Kick envelope: the bass ducks exactly as loud as the kick is. Curve: draw your own ducking shape." },
            { "scLength", "How long the drawn curve lasts after each kick." },
            { "scPosition", "Duck the bass after the effects (tails get ducked too) or before them." },

            // Master
            { "masterOn", "Turns the master section on or off." },
            { "masterDrive", "Pushes the level into the limiter: louder, but too much flattens the punch." },
            { "masterClip", "Soft clipping before the limiter: adds loudness and grit." },
            { "masterCeiling", "The highest peak level the output may reach." },
            { "masterRelease", "How fast the limiter lets go after a peak. Short = louder but can pump." },

            // LFO:er
            { "lfo1On", "Turns LFO 1 on or off." }, { "lfo2On", "Turns LFO 2 on or off." }, { "lfo3On", "Turns LFO 3 on or off." },
            { "lfo1Shape", "The LFO's waveform." }, { "lfo2Shape", "The LFO's waveform." }, { "lfo3Shape", "The LFO's waveform." },
            { "lfo1Rate", "How fast the LFO moves, synced to the song tempo." },
            { "lfo2Rate", "How fast the LFO moves, synced to the song tempo." },
            { "lfo3Rate", "How fast the LFO moves, synced to the song tempo." },
        };

        if (const auto found = texts.find (paramID); found != texts.end())
            return found->second;

        // Effekterna: förklaring efter rattens namn.
        static const std::map<juce::String, juce::String> effectTexts {
            { "On", "Turns the effect on or off." },
            { "Post", "PRE: before the gate (gets chopped by it). POST: after the gate (tails ring out)." },
            { "Mix", "How much of the effect you hear." },
            { "Cutoff", "Filter frequency." }, { "Reso", "Boost at the cutoff frequency." },
            { "Drive", "How hard the signal is pushed into the distortion." }, { "Tone", "Brightness after the distortion." },
            { "Feedback", "How much of the output is fed back: more repeats / stronger effect." },
            { "Rate", "Speed of the effect's movement." }, { "Depth", "How much the effect moves." },
            { "Size", "Size of the room." }, { "Damping", "How fast the highs die out in the room." },
            { "Width", "Stereo width of the effect." }, { "LowCut", "Removes low end from the effect, so the bass stays tight." },
            { "HighCut", "Removes highs from the effect, for a darker sound." }, { "Time", "Delay time, synced to the tempo." },
            { "PingPong", "Echoes bounce between left and right." },
        };
        if (paramID.startsWith ("fx"))
            for (const auto& [suffix, text] : effectTexts)
                if (paramID.endsWith (suffix))
                    return text;

        return {};
    }

    juce::String modulationHint()
    {
        return "Drag an LFO here to modulate it; drag the coloured ring to set how much. Right-click for more.";
    }
}
