#include "LibraryImport.h"

namespace Library
{
    double maxSecondsFor (const juce::String& category)
    {
        return category == "Kicks" ? 2.0 : 30.0;
    }

    std::vector<float> resampleToLibraryRate (const std::vector<float>& input, double sourceRate)
    {
        if (input.empty() || sourceRate <= 0.0)
            return {};
        if (juce::approximatelyEqual (sourceRate, libraryRate))
            return input;

        const auto ratio = sourceRate / libraryRate; // indatasamplingar per utsampling
        juce::WindowedSincInterpolator interpolator;
        // Latensen är getBaseLatency() indatasamplingar, alltså getBaseLatency() / ratio utsamplingar. Fyll ut med nollor efter signalen så
        // att svansen töms ur filtret, och kasta de första utsamplingarna så att attacken hamnar där den ska.
        const auto latencyOut = (int) std::lround (juce::WindowedSincInterpolator::getBaseLatency() / ratio);
        const auto padIn = (size_t) juce::WindowedSincInterpolator::getBaseLatency() + 64;
        std::vector<float> padded (input.size() + padIn, 0.0f);
        std::copy (input.begin(), input.end(), padded.begin());

        const auto wanted = (int) std::floor ((double) input.size() / ratio); // så många utsamplingar motsvarar indatan
        std::vector<float> out ((size_t) (wanted + latencyOut + 1), 0.0f);
        const auto producible = (int) std::floor ((double) padded.size() / ratio) - 1;
        const auto produce = juce::jmin ((int) out.size(), producible);
        interpolator.process (ratio, padded.data(), out.data(), produce);

        std::vector<float> result (out.begin() + juce::jmin (latencyOut, (int) out.size()), out.begin() + juce::jmin ((int) out.size(), produce));
        return result;
    }

    bool conditionAudio (std::vector<float>& audio, double maxSeconds, juce::StringArray& notes, juce::String& error)
    {
        for (auto& sample : audio)
            if (! std::isfinite (sample))
                sample = 0.0f;

        auto peak = 0.0f;
        for (const auto sample : audio)
            peak = juce::jmax (peak, std::abs (sample));
        if (peak < 1.0e-6f)
        {
            error = "the sound is silent";
            return false;
        }

        // Tystnad före attacken och efter slutet: under -60 dB av toppen.
        const auto threshold = peak * (float) juce::Decibels::decibelsToGain (silenceDb);
        size_t first = 0, last = audio.size() - 1;
        while (first < audio.size() && std::abs (audio[first]) <= threshold)
            ++first;
        while (last > first && std::abs (audio[last]) <= threshold)
            --last;
        const auto start = first > (size_t) attackPreRoll ? first - (size_t) attackPreRoll : (size_t) 0;
        std::vector<float> trimmed (audio.begin() + (std::ptrdiff_t) start, audio.begin() + (std::ptrdiff_t) last + 1);
        if (start > 0 || last + 1 < audio.size())
            notes.add ("silence trimmed (" + juce::String ((int) start) + " samples at the start, " + juce::String ((int) (audio.size() - last - 1)) + " at the end)");

        const auto maxSamples = (size_t) std::llround (maxSeconds * libraryRate);
        if (trimmed.size() > maxSamples)
        {
            trimmed.resize (maxSamples);
            notes.add ("shortened to " + juce::String (maxSeconds, 0) + " s");
        }

        // Fade-out i slutet, aldrig i början.
        const auto fadeLength = juce::jmin ((size_t) std::llround (endFadeSeconds * libraryRate), trimmed.size() / 2);
        for (size_t i = 0; i < fadeLength; ++i)
        {
            const auto position = (double) (i + 1) / (double) fadeLength; // 1/n ... 1
            const auto gain = 0.5 * (1.0 + std::cos (juce::MathConstants<double>::pi * position));
            trimmed[trimmed.size() - fadeLength + i] *= (float) gain;
        }

        // Normalisera till -1 dBFS sample peak, mätt efter fade-outen.
        peak = 0.0f;
        for (const auto sample : trimmed)
            peak = juce::jmax (peak, std::abs (sample));
        if (peak < 1.0e-6f)
        {
            error = "the sound is silent";
            return false;
        }
        const auto gain = (float) juce::Decibels::decibelsToGain (normaliseDb) / peak;
        for (auto& sample : trimmed)
            sample *= gain;
        audio = std::move (trimmed);
        return true;
    }

    bool convertFile (const juce::File& source, double maxSeconds, std::vector<float>& out, juce::StringArray& notes, juce::String& error)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (source));
        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0 || reader->numChannels == 0)
        {
            error = "not a readable audio file";
            return false;
        }
        if ((double) reader->lengthInSamples / reader->sampleRate > maxSourceSeconds)
        {
            error = "longer than " + juce::String ((int) (maxSourceSeconds / 60.0)) + " minutes";
            return false;
        }

        const auto length = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> audio ((int) reader->numChannels, length);
        reader->read (&audio, 0, length, 0, true, true);

        std::vector<float> mono ((size_t) length, 0.0f);
        const auto scale = 1.0f / (float) audio.getNumChannels();
        for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        {
            const auto* data = audio.getReadPointer (channel);
            for (int i = 0; i < length; ++i)
                mono[(size_t) i] += data[i] * scale;
        }

        auto resampled = resampleToLibraryRate (mono, reader->sampleRate);
        if (! juce::approximatelyEqual (reader->sampleRate, libraryRate))
            notes.add ("resampled from " + juce::String (reader->sampleRate, 0) + " Hz");
        if (reader->numChannels > 1)
            notes.add ("mixed " + juce::String ((int) reader->numChannels) + " channels to mono");
        if (! conditionAudio (resampled, maxSeconds, notes, error))
            return false;
        out = std::move (resampled);
        return true;
    }

    bool writeWav (const juce::File& file, const std::vector<float>& audio)
    {
        if (audio.empty() || ! file.getParentDirectory().createDirectory())
            return false;
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                       .withSampleRate (libraryRate)
                                                       .withNumChannels (1)
                                                       .withBitsPerSample (24));
        if (writer == nullptr)
            return false;
        const float* channels[] { audio.data() };
        const auto ok = writer->writeFromFloatArrays (channels, 1, (int) audio.size());
        writer.reset(); // skriver klart filen
        return ok;
    }

    bool isAudioFile (const juce::File& file)
    {
        const auto extension = file.getFileExtension().toLowerCase();
        return extension == ".wav" || extension == ".aif" || extension == ".aiff" || extension == ".flac" || extension == ".ogg" || extension == ".mp3";
    }

    juce::Array<juce::File> findAudioFiles (const juce::File& fileOrFolder)
    {
        juce::Array<juce::File> files;
        if (fileOrFolder.isDirectory())
        {
            for (const auto& file : fileOrFolder.findChildFiles (juce::File::findFiles, true))
                if (isAudioFile (file))
                    files.add (file);
        }
        else if (fileOrFolder.existsAsFile())
            files.add (fileOrFolder);
        std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) { return a.getFullPathName().compareNatural (b.getFullPathName()) < 0; });
        return files;
    }

    namespace
    {
        // Ett filnamn som alltid duger i ett manifest (se isSafeRelativePath): bara bokstäver, siffror, mellanslag och några skiljetecken.
        juce::String safeStem (const juce::String& text)
        {
            juce::String result;
            for (auto c : text)
                result += (juce::CharacterFunctions::isLetterOrDigit (c) || c == ' ' || c == '_' || c == '-' || c == '(' || c == ')') ? juce::String::charToString (c) : juce::String ("_");
            result = result.trim();
            while (result.endsWithChar ('.') || result.endsWithChar (' '))
                result = result.dropLastCharacters (1);
            if (result.isEmpty())
                result = "sound";
            if (! isSafeRelativePath (result + ".wav")) // t.ex. CON
                result = "_" + result;
            return result;
        }
    }

    ImportResult importFiles (const juce::Array<juce::File>& files, const ImportOptions& options, const juce::File& libraryRoot)
    {
        ImportResult result;
        if (! categories().contains (options.category))
            result.errors.add ("unknown category \"" + options.category + "\" (" + categories().joinIntoString (", ") + ")");
        if (options.license.isEmpty())
            result.errors.add ("a license is required (CC0-1.0, MIT, BSD-2-Clause, BSD-3-Clause or PERSONAL)");
        else if (! isAllowedLicense (options.license))
            result.errors.add ("license not allowed: \"" + options.license + "\" (CC0-1.0, MIT, BSD-2-Clause, BSD-3-Clause or PERSONAL)");
        if (files.isEmpty())
            result.errors.add ("no audio files to import");
        if (options.date.isNotEmpty() && ! isValidDate (options.date))
            result.errors.add ("the date must look like YYYY-MM-DD: " + options.date);
        if (! result.errors.isEmpty())
            return result;

        juce::String packageName = options.package;
        if (packageName.isEmpty() && ! files.isEmpty())
            packageName = files.size() == 1 ? files[0].getFileNameWithoutExtension() : files[0].getParentDirectory().getFileName();
        packageName = safeStem (packageName);

        const auto packageFolder = libraryRoot.getChildFile (options.category).getChildFile (packageName);
        const auto manifestFile = packageFolder.getChildFile (manifestName);

        Manifest manifest;
        if (manifestFile.existsAsFile())
        {
            const auto existing = parseFile (manifestFile, false);
            if (! existing.ok())
            {
                result.errors.add ("the package " + packageName + " exists but its manifest is not valid: " + existing.errors.joinIntoString (" / "));
                return result;
            }
            if (existing.manifest.license != options.license)
            {
                result.errors.add ("the package " + packageName + " already exists with the license " + existing.manifest.license + ", not " + options.license);
                return result;
            }
            manifest = existing.manifest;
        }
        else
        {
            manifest.package = packageName;
            manifest.source = options.source.isNotEmpty() ? options.source : options.url.isNotEmpty() ? options.url : juce::String ("local files");
            manifest.url = options.url;
            manifest.license = options.license;
            manifest.redistributable = ! isPersonal (options.license);
            manifest.date = options.date.isNotEmpty() ? options.date : juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
        }

        const auto maxSeconds = maxSecondsFor (options.category);
        for (const auto& file : files)
        {
            std::vector<float> audio;
            juce::StringArray notes;
            juce::String error;
            if (! convertFile (file, maxSeconds, audio, notes, error))
            {
                result.errors.add (file.getFileName() + ": " + error);
                continue;
            }

            // Ett namn som inte finns än (Windows skiljer inte på versaler).
            const auto stem = safeStem (file.getFileNameWithoutExtension());
            auto name = stem + ".wav";
            for (int n = 2; packageFolder.getChildFile (name).existsAsFile(); ++n)
                name = stem + " " + juce::String (n) + ".wav";
            if (! writeWav (packageFolder.getChildFile (name), audio))
            {
                result.errors.add (file.getFileName() + ": could not write " + name);
                continue;
            }

            Item item;
            item.file = name;
            item.name = file.getFileNameWithoutExtension();
            item.tags = options.tags;
            item.author = options.author;
            manifest.items.push_back (std::move (item));
            ++result.imported;
            for (const auto& note : notes)
                result.notes.add (file.getFileName() + ": " + note);
        }

        if (result.imported > 0 && ! write (manifestFile, manifest))
            result.errors.add ("could not write " + manifestFile.getFullPathName());
        result.packageFolder = packageFolder;
        return result;
    }
}
