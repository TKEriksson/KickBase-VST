#include "Help.h"
#include <map>

namespace Help
{
    juce::String forParameter (const juce::String& rawID)
    {
        // Baslagrens parametrar har samma förklaring i alla lager.
        auto paramID = rawID;
        if (paramID.startsWith ("bass") && paramID.length() > 4 && juce::CharacterFunctions::isDigit (paramID[4]))
            paramID = "bass" + paramID.substring (paramID.length() > 5 && juce::CharacterFunctions::isDigit (paramID[5]) ? 6 : 5); // bass1 ... bass10
        // OSC 2 (B) och OSC 3 (C) har samma förklaringar som OSC 1 (A).
        if ((paramID.startsWith ("bassB") || paramID.startsWith ("bassC")) && paramID.length() > 5 && juce::CharacterFunctions::isUpperCase (paramID[5]))
            paramID = "bassA" + paramID.substring (5);

        // Radernas längd och loop: samma förklaring för alla instrument.
        if (rawID.startsWith ("rowLoopBass"))
            return "On: this instrument's row repeats if it is shorter than the pattern. Off: it plays once and is then silent until the pattern starts over.";
        if (rawID.startsWith ("rowBarsBass"))
            return "This instrument's own pattern length in bars. Total = as long as the whole pattern.";
        if (rawID.startsWith ("macro"))
            return "A macro: a knob of your own. Turn it by hand (or automate it) and let it move other knobs: drag its handle in the MATRIX tab onto a knob, or set it up in the route list.";

        // Mixern: samma förklaring för alla källor.
        if (rawID.startsWith ("mix"))
        {
            if (rawID.endsWith ("Gain"))
                return "Gain of this sound source in the mix, in dB. Down to -inf (silent), up to +12 dB. Double-click to reset to 0 dB.";
            if (rawID.endsWith ("Pan"))
                return "Pans this source left or right. Centre (C) leaves it as it is.";
            if (rawID.endsWith ("Duck"))
                return "SC on: the sidechain ducks this source under the kick. Off: the kick leaves it alone.";
            if (rawID.endsWith ("Max"))
                return "MAX on: this source goes through the maximizer (drive, clip, limiter, width). Off: it bypasses the maximizer and is added after it.";
        }

        static const std::map<juce::String, juce::String> texts {
            // Mönster och mix
            { "patternBars", "TOTAL LENGTH: how many bars the whole pattern lasts before it starts over (1 to 64). It is at least as long as the longest row. Rows can be shorter: see the length box under each row's knobs." },
            { "rowLoopKick", "On: the kick row repeats if it is shorter than the pattern. Off: it plays once and is then silent until the pattern starts over." },
            { "rowLoopBass1", "On: bass 1 repeats if its row is shorter than the pattern. Off: it plays once and is then silent until the pattern starts over." },
            { "rowLoopBass2", "On: bass 2 repeats if its row is shorter than the pattern. Off: it plays once and is then silent until the pattern starts over." },
            { "rowLoopBass3", "On: bass 3 repeats if its row is shorter than the pattern. Off: it plays once and is then silent until the pattern starts over." },
            { "rowBarsKick", "The kick row's own length in bars. Total = as long as the whole pattern." },
            { "rowBarsBass1", "Bass 1's own length in bars. Total = as long as the whole pattern." },
            { "rowBarsBass2", "Bass 2's own length in bars. Total = as long as the whole pattern." },
            { "rowBarsBass3", "Bass 3's own length in bars. Total = as long as the whole pattern." },
            { "instrumentCount", "How many instruments are used (1-10). Each has its own pattern, oscillators, filter, envelopes, effect send and sidechain depth." },
            { "bassEnv2Attack", "ENV 2: an extra envelope that does not play the sound but moves other knobs (drag its handle, or use the MATRIX tab). Attack = how fast it rises." },
            { "bassEnv2Decay", "ENV 2: how fast it falls towards the sustain level." },
            { "bassEnv2Sustain", "ENV 2: the level it stays at while the note is held (0 % = it falls all the way back)." },
            { "bassEnv2Release", "ENV 2: how fast it falls when the note ends." },
            { "bassEnv3Attack", "ENV 3: a second extra envelope for modulation. Attack = how fast it rises." },
            { "bassEnv3Decay", "ENV 3: how fast it falls towards the sustain level." },
            { "bassEnv3Sustain", "ENV 3: the level it stays at while the note is held (0 % = it falls all the way back)." },
            { "bassEnv3Release", "ENV 3: how fast it falls when the note ends." },
            { "bassSubWave", "The sub oscillator's waveform: sine (pure), triangle, square (hollow, punchy) or saw (bright)." },
            { "bassNoiseTone", "NOISE TONE - how dark the noise is: 100 % = white noise (bright), lower = darker, like a rumble or breath." },
            { "bassPitchEnvAmount", "PITCH ENV AMOUNT - how many semitones the pitch starts above (or, negative, below) the note and then glides to it: a thump or a laser. 0 = off." },
            { "bassPitchEnvDecay", "PITCH ENV DECAY - how fast the pitch glides to the note." },
            { "bassDriveMode", "How the drive distorts: Soft (warm), Hard (clipped), Fold (wavefolder, metallic), Asym (even overtones, gritty) or Crush (stepped, digital)." },
            { "noteEnd", "What happens when the DAW note ends in the middle of the pattern: stop at once, finish the bar or finish the whole pattern." },
            { "bassGate", "NOTE LENGTH SCALE - scales the length of every note in this instrument at once (100 % = as long as written). Lower = shorter, punchier notes." },
            { "bassGateOn", "Turns this layer's GATE on: a separate on/off rhythm (its own grid) that chops the layer's sound. It does not change the notes." },
            { "bassGateGrid", "The grid of this layer's gate steps (independent of the note grid)." },
            { "bassGateDepth", "How much the gate cuts the sound in its closed steps: 0 % = nothing, 100 % = fully silent." },
            { "bassGateSmooth", "How soft the gate's edges are: short = hard and chopped, longer = smoother." },
            { "gain", "Output volume of the whole plugin." },
            { "bassDuck", "DUCK sets how deeply the kick pushes this layer's volume down. 0 % = no ducking, 100 % = the volume goes all the way down." },
            { "bassOn", "Turns this bass layer on or off." },
            { "kickFxChannel", "Which effect channel (FX1 to FX11) the kick is sent through. None = no effects on the kick." },
            { "kickFxAmount", "How much of the kick goes through its effect channel (the rest stays dry)." },
            { "bassFxChannel", "Which effect channel (FX1 to FX11) this instrument sends its sound through. None = no effects." },
            { "bassFxAmount", "How much of this layer goes through its effect channel (the rest stays dry)." },
            { "fxALevel", "Volume of this effect channel's output." }, { "fxBLevel", "Volume of this effect channel's output." },
            { "fxCLevel", "Volume of this effect channel's output." },
            { "fxADuck", "DUCK sets how deeply the kick pushes this channel's volume down (tails too, so reverb and echo don't blur the kick). 0 % = no ducking, 100 % = the volume goes all the way down." }, { "fxBDuck", "DUCK sets how deeply the kick pushes this channel's volume down (tails too, so reverb and echo don't blur the kick). 0 % = no ducking, 100 % = the volume goes all the way down." },
            { "fxCDuck", "DUCK sets how deeply the kick pushes this channel's volume down (tails too, so reverb and echo don't blur the kick). 0 % = no ducking, 100 % = the volume goes all the way down." },
            { "bassGrid", "This layer's step grid: 1/16 for rolling basses, 1/8 T for triplet (K-B-B) grooves, and more." },

            // Kick
            { "kickWave", "The waveform of the kick's body: sine is the classic, triangle and soft square are brighter, soft saw is the harshest." },
            { "kickSub", "A sine one octave below the body, for extra weight at the very bottom." },
            { "kickFm", "FM on the body: a second sine bends its phase at the start of the kick. A little makes it punchier, a lot makes it metallic." },
            { "kickFmRatio", "The pitch of the FM sine compared to the body (2x = an octave above). Whole numbers sound harmonic, others more metallic." },
            { "kickFmDecay", "How fast the FM fades away after the start of the kick." },
            { "kickNoise", "A noisy top layer on the kick: snap and air. Has its own decay and tone." },
            { "kickNoiseDecay", "How long the noisy top layer rings." },
            { "kickNoiseTone", "High-pass frequency of the noisy top layer: higher = airier, lower = fuller." },
            { "kickCrush", "Bit crushing after the drive: 0 = clean, up to 4-bit grit." },
            { "kickLowCut", "High-pass on the whole kick: removes rumble below the kick's tone (20 Hz = off)." },
            { "kickVelPitch", "How much a note's velocity changes the pitch sweep: 0 = none, 100 % = a soft hit gets almost no sweep." },
            { "kickSampleMode", "How a loaded sample is used: Click = only the first moments (CLICK level), Layer = the whole sample on top of the synth kick, Replace = the sample is the kick." },
            { "kickSampleLevel", "Level of the sample in Layer and Replace mode." },
            { "kickSampleTune", "Plays the sample faster or slower, in semitones (changes its pitch and length)." },
            { "kickKeyFollow", "Follow: the kick's tail is tuned to the note you play. Fixed: it stays on C1 + TUNE whatever the note." },
            { "kickTune", "Tunes the kick's tail (its low 'boom') in semitones. Psy kicks often sit 1-3 semitones above the bass note." },
            { "kickPitchAmount", "Scales the whole pitch envelope you draw in the display: 100 % = as drawn, 0 % = no pitch drop at all." },
            { "kickLength", "Stretches or shortens both envelopes in time: a longer or shorter kick with the same shape." },
            { "kickPhase", "Where the sine body starts in its cycle. 90 degrees starts at the peak: punchier, with more click." },
            { "kickHarmonics", "Adds overtones to the body (2nd to 9th harmonic) that fade away quickly: a harder, more present punch." },
            { "kickHarmDecay", "How fast the overtones fade out." },
            { "kickHarmTilt", "How quickly the overtones get weaker going up. Higher = darker, rounder." },
            { "kickClick", "Level of the short click at the start (synthesized, or your sample)." },
            { "kickClickLength", "How long the click lasts." },
            { "kickClickTone", "Brightness of the synthesized click." },
            { "kickClipMode", "Drive character: Soft (smooth tanh), Hard (clipping), Tube (asymmetric, adds even harmonics)." },
            { "kickDrive", "Saturation on the kick: rounder and louder, more harmonics." },
            { "kickHighCut", "Low-pass filter on the kick. Tames the click and the top end." },
            { "kickLevel", "Volume of the kick." },

            // Bas: oscillator 1
            { "bassOctave", "Moves the whole bass up or down in octaves." },
            { "bassTune", "Fine tunes the bass in semitones." },
            { "bassATable", "Which wavetable the oscillator plays: Basic (sine-triangle-saw-square morph), PWM, Sync, Harmonics, FM, Growl, Fold, Formant, Digital, Phase Dist, or a plain Classic Sine/Triangle/Saw/Square (POS does nothing for those, like an old-school oscillator)." },
            { "bassAPos", "Position in the wavetable: which of its waveforms plays. Drag an LFO here to sweep through them for movement." },
            { "bassALevel", "How loud this oscillator is. OSC B at 0 is silent but can still frequency-modulate OSC A (WARP: FM)." },
            { "bassASemi", "Pitch in semitones (+12 = an octave up, +7 = a fifth)." },
            { "bassAFine", "Fine tuning in cents." },
            { "bassAUnison", "Number of stacked, slightly detuned voices. More = wider and fatter (use carefully in the low end)." },
            { "bassADetune", "How far apart the unison voices are tuned." },
            { "bassABlend", "Level of the outer unison voices compared to the centre one." },
            { "bassAWarp", "Bends the waveform before it is played: Sync (screaming), Bend, PWM (narrower), Mirror, FM (B) = OSC B modulates OSC A, Flip (mirrors the second half), Quantize (stepped, digital) and AM (the wave is multiplied by a sine an octave up)." },
            { "bassAWarpAmount", "How much the WARP mode changes the waveform." },
            { "bassAPhase", "Where in the waveform each note starts. The same start every note gives a tight, punchy attack." },
            { "bassAPan", "Pans this oscillator left or right. Centre (C) leaves it in the middle." },
            { "bassAStereo", "How far the UNISON voices are spread across the stereo picture: 0 % = all in the middle (mono), 100 % = the outer voices go hard left and right. Does nothing with UNISON = 1. Keep the low end narrow." },
            { "bassARandom", "Randomises the start phase per note: looser and more analog, less punchy." },
            { "bassWarpEnv", "Extra warp on OSC A at the start of each note, following the filter envelope: a bright, digital 'knock' that fades." },
            { "bassSub", "Level of a pure sine sub under the bass, for weight in the low end." },
            { "bassSubOctave", "Sub in the same octave as the bass, or one octave lower (careful: very low notes get inaudible)." },
            { "bassNoise", "Adds white noise before the filter, for air and grit in the attack." },

            // Filter
            { "bassFilterType", "Filter type. LP 24 = smooth and dark (classic psy), LP 12 = brighter, Band and High for thin sounds." },
            { "bassAOn", "Turns this oscillator on or off. Off = it is not heard (and OSC 2 does not frequency-modulate OSC 1)." },
            { "bassSampleOn", "Turns the sample on or off. Turn the oscillators off (and SUB and NOISE down) to hear only the sample." },
            { "bassSampleLevel", "How loud this layer's sample is (the file you load with LOAD or drop on the page). It plays alongside the oscillators, through the filter and envelope." },
            { "bassSampleRoot", "The pitch the sample was recorded at. With KEY on, the sample is pitched so that this note becomes the note you play." },
            { "bassSampleLoop", "On: the sample repeats while the note is held. Off: it plays once." },
            { "bassSampleKey", "On: the sample follows the note you play (set ROOT to the pitch it was recorded at). Off: it always plays at its own pitch and speed." },
            { "bassSampleStart", "Where in the sample each note starts." },
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
            { "masterWidth", "Stereo width of the whole mix: 0 % = mono, 100 % = unchanged, 200 % = extra wide." },
            { "masterEnhance", "Stereo enhancer: adds width to mono sounds by putting a delayed copy of the highs in the sides. Sums back to the same thing in mono, so it is safe." },
            { "masterMonoBelow", "Makes everything below this frequency mono, so the low end stays tight and centred. Off = unchanged." },
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

    juce::String forEffectKnob (const juce::String& name)
    {
        static const std::map<juce::String, juce::String> texts {
            { "TYPE", "Which kind of filter: low pass keeps the lows, high pass the highs, band pass a band around the frequency." },
            { "SLOPE", "How steeply the filter cuts: 24 dB/oct is stronger than 12 dB/oct." },
            { "MODE", "The distortion's character: Soft, Hard, Fold (wavefolder) or Crush." },
            { "CUTOFF", "The filter's frequency." },
            { "RESO", "Boost around the filter frequency: squelchy at high values." },
            { "DRIVE", "How hard the signal is pushed into the distortion." },
            { "TONE", "Brightness after the distortion." },
            { "MIX", "How much of the effect you hear compared to the untouched signal." },
            { "LOW", "Boost or cut the low end (below ~120 Hz)." },
            { "MID FREQ", "The frequency the MID band works on." },
            { "MID", "Boost or cut the middle band." },
            { "HIGH", "Boost or cut the highs (above ~6 kHz)." },
            { "RATE", "Speed of the movement (or, on gates and auto filters, how often it repeats, synced to the tempo)." },
            { "DEPTH", "How much the effect moves." },
            { "DELAY", "Base delay of the effect in milliseconds." },
            { "FEEDBACK", "How much of the output is fed back: more repeats or a stronger effect (on the echo: how many repeats)." },
            { "FREQ", "The effect's frequency (phaser centre, comb pitch or ring modulator frequency)." },
            { "TIME", "Delay time, synced to the tempo." },
            { "OFFSET", "Moves the right echo earlier (minus) or later (plus) than the left one, for a wide, stereo echo." },
            { "IN PAN", "Pans the sound going into the echo. In ping-pong the echoes start on the side you pan towards (left if centred)." },
            { "IN VOL", "How loud the sound going into the echo is, so how loud the echoes are. The untouched sound is set by DRY VOL." },
            { "FB MODE", "What the feedback does: Normal repeats on the same side, Invert flips the sound upside down every repeat, Ping-pong bounces the echoes between left and right." },
            { "CUT", "Darkens every repeat: lowers the highs the echo is allowed to keep, so the echoes fade into the background." },
            { "DRY VOL", "Volume of the untouched sound (the echoes are set by IN VOL)." },
            { "LOW CUT", "Removes low end from the effect so the bass stays tight." },
            { "HIGH CUT", "Removes highs from the effect for a darker sound." },
            { "SIZE", "Size of the room." },
            { "DAMPING", "How fast the highs die out." },
            { "WIDTH", "Stereo width of the effect." },
            { "THRESHOLD", "The level above which the compressor starts reducing the volume." },
            { "RATIO", "How strongly the compressor reduces what is above the threshold." },
            { "ATTACK", "How fast the compressor reacts." },
            { "RELEASE", "How fast the compressor lets go." },
            { "MAKEUP", "Volume added after compression." },
            { "LENGTH", "How long the gate stays open in each repeat." },
            { "SMOOTH", "How soft the gate's edges are (longer = less clicky)." },
            { "SHIFT", "Moves the gate pattern within the beat." },
            { "GAIN", "Maximizer: how much the sound is pushed in before it is clipped and limited. More gain = louder and denser." },
            { "CLIP", "Maximizer: how much of a soft clipper is added before the limiter (round, warm loudness)." },
            { "CEILING", "Maximizer: the level nothing goes above. Peaks are held at this ceiling." },
            { "BITS", "Number of bits: fewer = rougher and more distorted." },
            { "DOWNSAMPLE", "Lowers the sample rate for a digital, aliased sound." },
        };
        const auto found = texts.find (name);
        return found != texts.end() ? found->second : name;
    }

    juce::String modulationHint()
    {
        return "Drag an LFO here to modulate it; drag the coloured ring to set how much. Right-click for more.";
    }
}
