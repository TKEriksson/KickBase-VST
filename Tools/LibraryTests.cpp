// `tool librarycheck [mapp]` och `tool librarytest`: biblioteksmanifesten (Source/Core/LibraryManifest.h). librarycheck kontrollerar alla paket i
// användarens biblioteksmapp (läser bara). librarytest provar reglerna i en tillfällig mapp (Scenarios::TemporaryUserFolders).

#include "../Source/Core/LibraryManifest.h"
#include "../Source/Core/LibraryImport.h"
#include "../Source/Core/LibraryIndex.h"
#include "../Source/Core/Settings.h"
#include "Scenarios.h"

// librarycheck [mapp]: validerar manifesten. Ger 1 vid fel, annars 0.
int libraryCheck (const juce::String& folderArgument)
{
    const auto root = folderArgument.isNotEmpty() ? juce::File::getCurrentWorkingDirectory().getChildFile (folderArgument) : Settings::libraryFolder();
    std::printf ("library: %s\n", root.getFullPathName().toRawUTF8());
    const auto report = Library::checkFolder (root, true);
    for (const auto& line : report.lines)
        std::printf ("%s\n", line.toRawUTF8());
    return report.errors.isEmpty() ? 0 : 1;
}

// importlibrary <mapp|fil> <kategori> <licens> [källa-url] [--package=namn] [--author=namn] [--source=text] [--tags=a,b] [--date=ÅÅÅÅ-MM-DD]
// Konverterar ljudfilerna (mono, 44,1 kHz, 24 bit, trimmade, normaliserade till -1 dBFS) och skriver paketet i biblioteksmappen. Vägrar utan godkänd licens.
int libraryImport (const juce::StringArray& args)
{
    juce::StringArray positional;
    Library::ImportOptions options;
    for (const auto& arg : args)
    {
        if (arg.startsWith ("--package="))     options.package = arg.fromFirstOccurrenceOf ("=", false, false);
        else if (arg.startsWith ("--author=")) options.author = arg.fromFirstOccurrenceOf ("=", false, false);
        else if (arg.startsWith ("--source=")) options.source = arg.fromFirstOccurrenceOf ("=", false, false);
        else if (arg.startsWith ("--date="))   options.date = arg.fromFirstOccurrenceOf ("=", false, false);
        else if (arg.startsWith ("--tags="))   options.tags.addTokens (arg.fromFirstOccurrenceOf ("=", false, false), ",", "");
        else                                   positional.add (arg);
    }
    if (positional.size() < 3)
    {
        std::printf ("usage: tool importlibrary <folder|file> <category> <license> [source-url] [--package=name] [--author=name] [--source=text] [--tags=a,b] [--date=YYYY-MM-DD]\n"
                     "  categories: %s\n  licenses: CC0-1.0, MIT, BSD-2-Clause, BSD-3-Clause, PERSONAL\n"
                     "  reads wav, aiff, flac and ogg (mp3 is not supported in this build: such files are reported as not readable)\n"
                     "  an existing package gets more files; its source, url and date are kept\n", Library::categories().joinIntoString (", ").toRawUTF8());
        return 1;
    }
    options.tags.trim();
    options.tags.removeEmptyStrings();
    options.category = positional[1];
    options.license = positional[2];
    options.url = positional.size() > 3 ? positional[3] : juce::String();
    const auto input = juce::File::getCurrentWorkingDirectory().getChildFile (positional[0]);
    const auto result = Library::importFiles (Library::findAudioFiles (input), options, Settings::libraryFolder());
    for (const auto& note : result.notes)
        std::printf ("  %s\n", note.toRawUTF8());
    for (const auto& error : result.errors)
        std::printf ("ERROR: %s\n", error.toRawUTF8());
    if (result.imported > 0)
        std::printf ("%d file(s) imported to %s\n", result.imported, result.packageFolder.getFullPathName().toRawUTF8());
    return result.ok() && result.imported > 0 ? 0 : 1;
}

namespace
{
    const char* validJson = R"({
        "package": "Test kicks",
        "source": "Test",
        "url": "https://example.com/test",
        "license": "CC0-1.0",
        "redistributable": true,
        "date": "2026-10-18",
        "items": [
            { "file": "kick1.wav", "name": "Kick 1", "tags": ["kick", "psy"], "rootNote": 36, "bpm": 140.5, "key": "C", "author": "Someone" },
            { "file": "sub/kick2.wav", "name": "Kick 2" }
        ]
    })";

    // Ger texten med ett fält ersatt (eller borttaget om `replacement` är tom): enkelt sätt att göra en trasig variant av validJson.
    juce::String variant (const juce::String& from, const juce::String& to)
    {
        return juce::String (validJson).replace (from, to);
    }

    bool hasError (const Library::ParseResult& result, const juce::String& needle)
    {
        for (const auto& error : result.errors)
            if (error.contains (needle))
                return true;
        return false;
    }

    // Ett paket på disk: manifest + tomma "ljudfiler".
    juce::File makePackage (const juce::File& root, const juce::String& category, const juce::String& name, const juce::String& json, const juce::StringArray& files)
    {
        const auto folder = root.getChildFile (category).getChildFile (name);
        folder.createDirectory();
        folder.getChildFile (Library::manifestName).replaceWithText (json);
        for (const auto& file : files)
        {
            folder.getChildFile (file).getParentDirectory().createDirectory();
            folder.getChildFile (file).replaceWithText ("x");
        }
        return folder;
    }

    // En testfil: `lead` sekunder tystnad, sedan en avklingande cosinus (börjar på toppen, som en kick). Kanal 2 har en annan nivå.
    juce::File writeTestAudio (const juce::File& file, double rate, int channels, int bits, double seconds, double lead, double hz, double decayPerSecond)
    {
        file.getParentDirectory().createDirectory();
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        juce::WavAudioFormat wav;
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (channels).withBitsPerSample (bits));
        const auto length = (int) (seconds * rate);
        juce::AudioBuffer<float> audio (channels, length);
        audio.clear();
        for (int i = (int) (lead * rate); i < length; ++i)
        {
            const auto t = (double) i / rate - lead;
            const auto value = std::cos (juce::MathConstants<double>::twoPi * hz * t) * std::exp (-decayPerSecond * t);
            for (int c = 0; c < channels; ++c)
                audio.setSample (c, i, (float) (value * (c == 0 ? 0.5 : 0.3)));
        }
        writer->writeFromAudioSampleBuffer (audio, 0, length);
        writer.reset();
        return file;
    }

    struct WavInfo { int channels = 0; double rate = 0.0; int bits = 0; int length = 0; std::vector<float> samples; };

    WavInfo readWav (const juce::File& file)
    {
        WavInfo info;
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr)
            return info;
        info.channels = (int) reader->numChannels;
        info.rate = reader->sampleRate;
        info.bits = (int) reader->bitsPerSample;
        info.length = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> audio (info.channels, info.length);
        reader->read (&audio, 0, info.length, 0, true, true);
        info.samples.assign (audio.getReadPointer (0), audio.getReadPointer (0) + info.length);
        return info;
    }

    int peakIndex (const std::vector<float>& audio)
    {
        int best = 0;
        for (size_t i = 1; i < audio.size(); ++i)
            if (std::abs (audio[i]) > std::abs (audio[(size_t) best]))
                best = (int) i;
        return best;
    }

    float peakOf (const std::vector<float>& audio)
    {
        auto peak = 0.0f;
        for (const auto sample : audio)
            peak = juce::jmax (peak, std::abs (sample));
        return peak;
    }
}

int libraryTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    // Det riktiga biblioteket (Dokument\Kickbase\Library) får inte röras: minns dess tillstånd före testet.
    const auto realLibrary = Settings::defaultFolder ("libraryFolder");
    const auto realExisted = realLibrary.exists();
    const auto realTime = realLibrary.getLastModificationTime();

    Scenarios::TemporaryUserFolders folders;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    // 1. Licenser: exakt de fem, inga jokertecken, inga andra skrivsätt.
    {
        for (const auto* license : { "CC0-1.0", "MIT", "BSD-2-Clause", "BSD-3-Clause", "PERSONAL" })
            check (Library::isAllowedLicense (license), juce::String ("licensen ") + license + " godtas");
        auto allRejected = true;
        for (const auto* license : { "", "cc0-1.0", "CC0", "CC0-1.0 ", "Personal", "MIT*", "BSD-*", "BSD-", "BSD-3-Clause-Clear", "Apache-2.0", "GPL-3.0", "CC-BY-4.0", "CC-BY-NC-4.0", "unknown" })
            allRejected &= ! Library::isAllowedLicense (license);
        check (allRejected, "andra licenser, tomma, andra skrivsätt och jokertecken avvisas");
    }

    // 2. Sökvägar i "file".
    {
        for (const auto* file : { "kick.wav", "sub/kick.wav", "sub\\kick.wav", "a/b/c.wav", "name with space.wav", "Kick (1).wav", "console.wav", "com10.wav", "nulled/x.wav" })
            check (Library::isSafeRelativePath (file), juce::String ("sökvägen godtas: ") + file);
        auto allRejected = true;
        juce::StringArray bad;
        for (const auto* file : { "", " ", "..", ".", "../a.wav", "a/../b.wav", "..\\a.wav", "sub/../../x.wav", "/a.wav", "\\a.wav", "C:\\a.wav", "C:a.wav", "c:/a.wav",
                                  "\\\\server\\share\\a.wav", "//server/share/a.wav", "a//b.wav", "a/", " a.wav", "a.wav ", "./a.wav", "a/./b.wav", "a:b.wav", "a.wav:stream",
                                  "nul.wav", "a/con.wav", "CON", "Aux.txt", "com1.wav", "LPT9.x.wav", "sub\\prn.wav" })
            if (Library::isSafeRelativePath (file))
            {
                allRejected = false;
                bad.add (file);
            }
        check (allRejected, "'..', absoluta, enhetsbokstav, UNC, tomma och osäkra sökvägar avvisas" + (bad.isEmpty() ? juce::String() : " (godtogs: " + bad.joinIntoString (" | ") + ")"));
    }

    // 3. Ett giltigt manifest, och rundtur: skriv, läs tillbaka, samma innehåll.
    const auto package = folders.root.getChildFile ("Library").getChildFile ("Kicks").getChildFile ("Test kicks");
    {
        auto result = Library::parse (validJson, "Test kicks", package);
        check (result.ok(), "ett giltigt manifest tolkas utan fel" + (result.ok() ? juce::String() : " (" + result.errors.joinIntoString (" / ") + ")"));
        check (result.manifest.items.size() == 2 && result.manifest.items[0].tags.size() == 2 && result.manifest.items[0].rootNote == 36
                   && juce::approximatelyEqual (result.manifest.items[0].bpm, 140.5) && result.manifest.items[1].rootNote == -1, "fälten läses");
        check (Library::write (package.getChildFile (Library::manifestName), result.manifest), "manifestet skrivs");
        const auto back = Library::parseFile (package.getChildFile (Library::manifestName));
        check (back.ok() && back.manifest == result.manifest, "ett skrivet manifest ger samma innehåll när det läses tillbaka");
        check (Library::parse (Library::toJson (result.manifest), "x", package).manifest == result.manifest, "toJson och parse ger samma innehåll");
    }

    // 4. redistributable: false om och endast om PERSONAL.
    {
        check (hasError (Library::parse (variant ("\"redistributable\": true", "\"redistributable\": false"), "p", {}), "redistributable"),
               "CC0 med redistributable false avvisas");
        check (hasError (Library::parse (variant ("\"CC0-1.0\"", "\"PERSONAL\""), "p", {}), "redistributable"), "PERSONAL med redistributable true avvisas");
        check (Library::parse (variant ("\"CC0-1.0\"", "\"PERSONAL\"").replace ("\"redistributable\": true", "\"redistributable\": false"), "p", {}).ok(),
               "PERSONAL med redistributable false godtas");
        check (hasError (Library::parse (variant ("\"CC0-1.0\"", "\"GPL-3.0\""), "p", {}), "license"), "en licens som inte står i listan avvisas, med fältet i meddelandet");
        check (hasError (Library::parse (variant ("\"license\": \"CC0-1.0\",", ""), "p", {}), "'license' is missing"), "saknad licens avvisas");
        check (hasError (Library::parse (variant ("\"redistributable\": true", "\"redistributable\": \"yes\""), "p", {}), "'redistributable' must be true or false"),
               "redistributable som text avvisas");
    }

    // 5. Trasig JSON, fel typer och saknade fält ger felmeddelanden med paket och fält, aldrig en krasch.
    {
        check (hasError (Library::parse ("{", "broken", {}), "broken: not valid JSON"), "trasig JSON ger ett fel med paketets namn");
        check (hasError (Library::parse ("", "empty", {}), "empty: not valid JSON"), "tom text ger ett fel");
        check (hasError (Library::parse (variant ("\"items\": [", "\"items\": [ [1], "), "p", {}), "items[0]: must be an object"), "en lista som post ger ett fel, ingen krasch");
        check (hasError (Library::parse ("[]", "arr", {}), "arr: the manifest must be a JSON object"), "en lista i stället för ett objekt ger ett fel");
        check (hasError (Library::parse ("{\"package\": 5}", "num", {}), "num: 'package' must be text"), "package som tal ger ett fel");
        check (hasError (Library::parse (variant ("\"package\": \"Test kicks\",", ""), "nopkg", {}), "nopkg: 'package' is missing"), "saknat package nämner mappnamnet");
        check (hasError (Library::parse (variant ("\"source\": \"Test\",", ""), "p", {}), "Test kicks: 'source' is missing"), "övriga fel nämner paketets namn och fältet");
        check (hasError (Library::parse (variant ("\"2026-10-18\"", "\"18 oktober\""), "p", {}), "'date' must look like"), "ett datum som inte är ÅÅÅÅ-MM-DD avvisas");
        check (hasError (Library::parse (variant ("\"items\": [", "\"items\": 5, \"x\": ["), "p", {}), "'items' must be a list"), "items som inte är en lista ger ett fel");
        check (hasError (Library::parse (variant ("{ \"file\": \"sub/kick2.wav\", \"name\": \"Kick 2\" }", "7"), "p", {}), "items[1]: must be an object"), "en post som inte är ett objekt ger ett fel");
        check (hasError (Library::parse (variant ("\"name\": \"Kick 2\"", "\"name\": 3"), "p", {}), "items[1]: 'name' must be text"), "name som tal ger ett fel med postens nummer");
        check (hasError (Library::parse (variant ("\"rootNote\": 36", "\"rootNote\": 200"), "p", {}), "'rootNote'"), "rootNote utanför 0-127 avvisas");
        check (hasError (Library::parse (variant ("\"rootNote\": 36", "\"rootNote\": 36.5"), "p", {}), "'rootNote'"), "rootNote som decimaltal avvisas");
        check (hasError (Library::parse (variant ("\"bpm\": 140.5", "\"bpm\": -1"), "p", {}), "'bpm'"), "negativt bpm avvisas");
        check (hasError (Library::parse (variant ("[\"kick\", \"psy\"]", "\"kick\""), "p", {}), "'tags'"), "tags som inte är en lista avvisas");
        check (hasError (Library::parse (variant ("\"file\": \"kick1.wav\"", "\"file\": \"../kick1.wav\""), "p", {}), "'file' must be a plain relative path"),
               "'..' i file avvisas, med fältet i meddelandet");
        check (hasError (Library::parse (variant ("\"file\": \"kick1.wav\"", "\"file\": \"\""), "p", {}), "'file' is empty"), "tomt file avvisas");
        check (hasError (Library::parse (variant ("\"file\": \"kick1.wav\"", "\"file\": \"C:\\\\kick1.wav\""), "p", {}), "'file' must be a plain relative path"), "enhetsbokstav i file avvisas");
        check (hasError (Library::parse (variant ("\"file\": \"sub/kick2.wav\"", "\"file\": \"KICK1.wav\""), "p", {}), "listed twice"), "samma fil två gånger avvisas (Windows skiljer inte på versaler)");
    }

    // 6. Filen ska ligga inuti paketmappen och finnas.
    {
        const auto folder = makePackage (folders.root.getChildFile ("Library"), "Kicks", "Disk", variant ("Test kicks", "Disk"), { "kick1.wav", "sub/kick2.wav" });
        check (Library::parseFile (folder.getChildFile (Library::manifestName), true).ok(), "ett paket med alla filer godkänns med filkontroll");
        folder.getChildFile ("sub/kick2.wav").deleteFile();
        check (hasError (Library::parseFile (folder.getChildFile (Library::manifestName), true), "'file' does not exist: sub/kick2.wav"), "en saknad fil ger ett fel med fältet");
        check (Library::parseFile (folder.getChildFile (Library::manifestName), false).ok(), "utan filkontroll räcker manifestet");
        check (Library::parseFile (folders.root.getChildFile ("finns inte").getChildFile (Library::manifestName)).errors.size() == 1, "en saknad manifest.json ger ett fel");
    }

    // 7. checkFolder över hela biblioteket: ett bra paket, ett med fel, en okänd kategori och en mapp utan manifest.
    {
        const auto library = folders.root.getChildFile ("CheckLibrary");
        makePackage (library, "Kicks", "Good", variant ("Test kicks", "Good"), { "kick1.wav", "sub/kick2.wav" });
        makePackage (library, "Bass", "Missing file", variant ("Test kicks", "Missing file"), { "kick1.wav" });
        makePackage (library, "Pads", "No manifest", {}, {});
        library.getChildFile ("Pads/No manifest").getChildFile (Library::manifestName).deleteFile();
        makePackage (library, "Weird", "Whatever", validJson, { "kick1.wav" });
        makePackage (library, "Synth", "Mine", variant ("\"CC0-1.0\"", "\"PERSONAL\"").replace ("\"redistributable\": true", "\"redistributable\": false"), { "kick1.wav", "sub/kick2.wav" });
        const auto report = Library::checkFolder (library, true);
        check (report.packages == 4, "checkFolder räknar paketen i kända kategorier (" + juce::String (report.packages) + ")");
        check (report.items == 4 && report.personalPackages == 1, "poster och PERSONAL-paket räknas (" + juce::String (report.items) + ", " + juce::String (report.personalPackages) + ")");
        check (report.errors.size() == 3, "tre fel: saknad fil, saknat manifest, okänd kategori (" + juce::String (report.errors.size()) + ")");
        check (Library::checkFolder (folders.root.getChildFile ("ingen mapp"), true).errors.isEmpty(), "en saknad biblioteksmapp är inget fel");
    }

    // 8a. Omsamplingen: latensen kompenseras (en topp hamnar där den ska, inom en sampling) och svansen töms.
    {
        for (const auto rate : { 48000.0, 96000.0, 22050.0, 88200.0 })
        {
            std::vector<float> impulse ((size_t) (rate * 0.1), 0.0f);
            const auto position = (int) (rate * 0.025);
            impulse[(size_t) position] = 1.0f;
            const auto out = Library::resampleToLibraryRate (impulse, rate);
            const auto expected = position * Library::libraryRate / rate;
            check (std::abs (peakIndex (out) - expected) <= 1.0, "en impuls vid " + juce::String (rate, 0) + " Hz hamnar vid " + juce::String (peakIndex (out))
                                                                   + " (väntat " + juce::String (expected, 2) + ")");
            check (std::abs ((double) out.size() - (double) impulse.size() * Library::libraryRate / rate) <= 2.0, "längden stämmer vid " + juce::String (rate, 0) + " Hz");

            // Svansen töms: en ton (5 kHz) som pågår ända till slutet ska ha full nivå i de sista utsamplingarna (klipptes svansen av filtrets latens
            // skulle de bli mycket svagare). En enstaka impuls duger inte här: JUCE:s interpolator filtrerar inte vid nedsampling.
            std::vector<float> tone ((size_t) (rate * 0.1));
            for (size_t i = 0; i < tone.size(); ++i)
                tone[i] = (float) std::sin (juce::MathConstants<double>::twoPi * 5000.0 * (double) i / rate);
            const auto toneOut = Library::resampleToLibraryRate (tone, rate);
            check (toneOut.size() > 40 && peakOf (std::vector<float> (toneOut.end() - 12, toneOut.end())) > 0.7f, "tonen har full nivå ända till slutet vid " + juce::String (rate, 0) + " Hz");
        }
        // Antialias vid nedsampling: en ton över 22,05 kHz dämpas minst 60 dB (annars viks den in), en ton på 1 kHz ändras högst 0,1 dB.
        auto rmsMiddle = [] (const std::vector<float>& audio)
        {
            auto sum = 0.0;
            const auto from = audio.size() / 4, to = audio.size() * 3 / 4;
            for (auto i = from; i < to; ++i)
                sum += (double) audio[i] * audio[i];
            return std::sqrt (sum / (double) (to - from));
        };
        auto sine = [] (double hz, double rate)
        {
            std::vector<float> tone ((size_t) (rate * 0.3));
            for (size_t i = 0; i < tone.size(); ++i)
                tone[i] = (float) std::sin (juce::MathConstants<double>::twoPi * hz * (double) i / rate);
            return tone;
        };
        for (const auto& [hz, rate] : { std::pair { 23000.0, 48000.0 }, std::pair { 30000.0, 96000.0 }, std::pair { 40000.0, 96000.0 }, std::pair { 25000.0, 88200.0 } })
        {
            const auto in = sine (hz, rate);
            const auto db = juce::Decibels::gainToDecibels (rmsMiddle (Library::resampleToLibraryRate (in, rate)) / rmsMiddle (in), -200.0);
            check (db < -60.0, juce::String (hz, 0) + " Hz vid " + juce::String (rate, 0) + " Hz dämpas till " + juce::String (db, 1) + " dB (kravet är under -60)");
        }
        for (const auto rate : { 48000.0, 96000.0, 192000.0, 22050.0 })
        {
            const auto in = sine (1000.0, rate);
            const auto db = juce::Decibels::gainToDecibels (rmsMiddle (Library::resampleToLibraryRate (in, rate)) / rmsMiddle (in));
            check (std::abs (db) < 0.1, "1 kHz vid " + juce::String (rate, 0) + " Hz ändras " + juce::String (db, 3) + " dB (högst 0,1)");
        }
        const std::vector<float> same { 0.1f, -0.2f, 0.3f };
        check (Library::resampleToLibraryRate (same, 44100.0) == same, "samma samplingsfrekvens kopieras oförändrad");
    }

    // 8b. Importen: kick (48 kHz stereo, 24 bit, 3 s med 0,3 s tystnad) → mono, 44,1 kHz, 24 bit, högst 2 s, -1 dBFS, tystnad trimmad, fade i slutet.
    const auto libraryRoot = Settings::libraryFolder();
    const auto source = folders.root.getChildFile ("source");
    {
        const auto kickFile = writeTestAudio (source.getChildFile ("kick.wav"), 48000.0, 2, 24, 3.0, 0.3, 55.0, 1.5);
        Library::ImportOptions options;
        options.category = "Kicks";
        options.package = "Imported kicks";
        options.license = "CC0-1.0";
        options.url = "https://example.com/kicks";
        options.author = "Tester";
        options.tags = { "kick", "test" };
        const auto result = Library::importFiles ({ kickFile }, options, libraryRoot);
        check (result.ok() && result.imported == 1, "en kick importeras" + (result.ok() ? juce::String() : " (" + result.errors.joinIntoString (" / ") + ")"));
        const auto wav = readWav (result.packageFolder.getChildFile ("kick.wav"));
        check (wav.channels == 1 && juce::approximatelyEqual (wav.rate, 44100.0) && wav.bits == 24, "mono, 44,1 kHz, 24 bit");
        check (wav.length > 0 && wav.length <= 2 * 44100, "en kick blir högst 2 s (" + juce::String (wav.length) + " samplingar)");
        const auto peakDb = juce::Decibels::gainToDecibels (peakOf (wav.samples));
        check (std::abs (peakDb + 1.0) < 0.05, "sample peak är -1 dBFS (" + juce::String (peakDb, 3) + ")");
        auto attack = 0; // första samplingen över 30 % av toppen: 0,3 s tystnad (13230 samplingar) ska vara borta, kvar är bara filtrets svaga förringning
        while (attack < wav.length && std::abs (wav.samples[(size_t) attack]) < 0.3f * peakOf (wav.samples))
            ++attack;
        check (attack < 600, "tystnaden före attacken är trimmad (attacken ligger vid sampling " + juce::String (attack) + ")");
        check (std::abs (wav.samples.back()) < 1.0e-3f, "det är en fade i slutet");
        check (peakIndex (wav.samples) < 1500, "toppen ligger i början (ingen fade-in, " + juce::String (peakIndex (wav.samples)) + ")");
        const auto manifest = Library::parseFile (result.packageFolder.getChildFile (Library::manifestName), true);
        check (manifest.ok() && manifest.manifest.items.size() == 1 && manifest.manifest.license == "CC0-1.0" && manifest.manifest.redistributable
                   && manifest.manifest.url == "https://example.com/kicks" && manifest.manifest.items[0].author == "Tester" && manifest.manifest.items[0].tags.size() == 2
                   && Library::isValidDate (manifest.manifest.date),
               "manifestet är giltigt och har källa, licens, upphovsperson och taggar");
        check (result.notes.joinIntoString ("|").contains ("shortened to 2 s") && result.notes.joinIntoString ("|").contains ("resampled from 48000"), "noterna säger vad som gjordes");
    }

    // 8c. Övriga kategorier får 30 s (en 31 s lång 8 kHz-fil kortas till exakt 30 s).
    {
        const auto longFile = writeTestAudio (source.getChildFile ("pad.wav"), 8000.0, 1, 16, 31.0, 0.0, 110.0, 0.01);
        Library::ImportOptions options;
        options.category = "Pads";
        options.license = "MIT";
        const auto result = Library::importFiles ({ longFile }, options, libraryRoot);
        const auto wav = readWav (result.packageFolder.getChildFile ("pad.wav"));
        check (result.ok() && wav.length == 30 * 44100, "en lång fil i Pads blir 30 s (" + juce::String (wav.length) + ")");
        check (result.packageFolder.getFileName() == "pad", "paketets namn blir filens namn när inget anges (" + result.packageFolder.getFileName() + ")");
    }

    // 8d. PERSONAL, och det som vägras (utan att skriva något).
    {
        const auto file = writeTestAudio (source.getChildFile ("mine.wav"), 44100.0, 1, 16, 0.5, 0.0, 60.0, 4.0);
        Library::ImportOptions options;
        options.category = "Synth";
        options.package = "Mine";
        options.license = "PERSONAL";
        const auto personal = Library::importFiles ({ file }, options, libraryRoot);
        const auto personalManifest = Library::parseFile (personal.packageFolder.getChildFile (Library::manifestName), true);
        check (personal.ok() && personalManifest.ok() && ! personalManifest.manifest.redistributable && personalManifest.manifest.license == "PERSONAL",
               "PERSONAL ger redistributable false");

        auto refused = [&] (const juce::String& category, const juce::String& license, const juce::String& what)
        {
            Library::ImportOptions bad;
            bad.category = category;
            bad.package = "Refused";
            bad.license = license;
            const auto result = Library::importFiles ({ file }, bad, libraryRoot);
            check (! result.ok() && result.imported == 0 && ! libraryRoot.getChildFile (category).getChildFile ("Refused").exists(), what + ": vägras och inget skrivs");
        };
        refused ("Kicks", "", "utan licens");
        refused ("Kicks", "GPL-3.0", "med en licens som inte står i listan");
        refused ("Kicks", "Apache-2.0", "med Apache-2.0");
        refused ("Kicks", "cc0-1.0", "med fel skrivsätt");
        refused ("Nope", "CC0-1.0", "med en okänd kategori");
        Library::ImportOptions badDate;
        badDate.category = "Kicks";
        badDate.package = "Refused";
        badDate.license = "CC0-1.0";
        badDate.date = "tomorrow";
        check (! Library::importFiles ({ file }, badDate, libraryRoot).ok(), "ett datum som inte är ÅÅÅÅ-MM-DD vägras");
        check (! Library::importFiles ({}, badDate, libraryRoot).ok(), "inga filer vägras");
    }

    // 8e. En tyst fil och en fil som inte är ljud ger fel, men de andra filerna importeras ändå.
    {
        writeTestAudio (source.getChildFile ("mixed/good.wav"), 44100.0, 1, 16, 0.5, 0.0, 60.0, 4.0);
        juce::AudioBuffer<float> silence (1, 4410);
        silence.clear();
        {
            std::unique_ptr<juce::OutputStream> stream (source.getChildFile ("mixed/silent.wav").createOutputStream());
            juce::WavAudioFormat wav;
            auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (44100.0).withNumChannels (1).withBitsPerSample (16));
            writer->writeFromAudioSampleBuffer (silence, 0, 4410);
        }
        source.getChildFile ("mixed/junk.wav").replaceWithText ("this is not audio");
        source.getChildFile ("mixed/readme.txt").replaceWithText ("ignored");
        Library::ImportOptions options;
        options.category = "Bass";
        options.license = "BSD-3-Clause";
        options.package = "Mixed";
        const auto files = Library::findAudioFiles (source.getChildFile ("mixed"));
        check (files.size() == 3, "findAudioFiles hittar ljudfilerna och hoppar över andra (" + juce::String (files.size()) + ")");
        const auto result = Library::importFiles (files, options, libraryRoot);
        check (result.imported == 1 && result.errors.size() == 2, "en bra fil importeras, två fel rapporteras (" + juce::String (result.imported) + ", " + juce::String (result.errors.size()) + ")");
        check (result.errors.joinIntoString ("|").contains ("silent") && result.errors.joinIntoString ("|").contains ("not a readable audio file"), "felen säger varför");
        check (Library::parseFile (result.packageFolder.getChildFile (Library::manifestName), true).ok(), "manifestet för paketet är giltigt");
    }

    // 8f. Samma ljud ger byte för byte samma fil, och ett befintligt paket byggs på (samma namn får ett nummer, en annan licens vägras).
    {
        const auto file = writeTestAudio (source.getChildFile ("again/tone.wav"), 48000.0, 2, 16, 1.0, 0.1, 80.0, 3.0);
        Library::ImportOptions first;
        first.category = "Synth";
        first.license = "CC0-1.0";
        first.package = "Det A";
        auto second = first;
        second.package = "Det B";
        const auto a = Library::importFiles ({ file }, first, libraryRoot);
        const auto b = Library::importFiles ({ file }, second, libraryRoot);
        juce::MemoryBlock dataA, dataB;
        a.packageFolder.getChildFile ("tone.wav").loadFileAsData (dataA);
        b.packageFolder.getChildFile ("tone.wav").loadFileAsData (dataB);
        check (dataA.getSize() > 0 && dataA == dataB, "samma ljud ger byte för byte samma fil (" + juce::String ((int) dataA.getSize()) + " byte)");

        const auto added = Library::importFiles ({ file }, first, libraryRoot);
        const auto grown = Library::parseFile (a.packageFolder.getChildFile (Library::manifestName), true);
        check (added.ok() && grown.ok() && grown.manifest.items.size() == 2 && grown.manifest.items[1].file == "tone 2.wav", "ett befintligt paket får fler filer, och samma namn får ett nummer");
        auto otherSource = first;
        otherSource.source = "Another source";
        check (Library::importFiles ({ file }, otherSource, libraryRoot).notes.joinIntoString ("|").contains ("already exists"), "ett befintligt paket behåller sina källfält och säger det");
        auto otherLicense = first;
        otherLicense.license = "MIT";
        check (! Library::importFiles ({ file }, otherLicense, libraryRoot).ok(), "ett befintligt paket med en annan licens vägras");
    }

    // 8g. Hela biblioteket efter importerna går igenom checkFolder utan fel.
    {
        libraryRoot.getChildFile ("Kicks").getChildFile ("Test kicks").deleteRecursively(); // från avsnitt 3 och 6: manifest utan filer
        libraryRoot.getChildFile ("Kicks").getChildFile ("Disk").deleteRecursively();
        const auto report = Library::checkFolder (libraryRoot, true);
        check (report.errors.isEmpty() && report.packages >= 6, "checkFolder över de importerade paketen: " + juce::String (report.packages) + " paket, "
                                                                     + juce::String (report.errors.size()) + " fel");
    }

    // 8h. Indexet: alla poster i giltiga paket, paket med fel döljs men syns i problems(), filter, bakgrundsskanning och ändringskontroll.
    {
        makePackage (libraryRoot, "Kicks", "BadLicense", variant ("\"CC0-1.0\"", "\"GPL-3.0\""), { "kick1.wav", "sub/kick2.wav" });
        makePackage (libraryRoot, "Kicks", "MissingFile", variant ("Test kicks", "MissingFile"), { "kick1.wav" });
        LibraryIndex index;
        const auto count = index.scanNow (libraryRoot);
        const auto list = index.entries();
        check (count == 8 && (int) list->size() == 8, "indexet har alla poster i de giltiga paketen (" + juce::String (count) + ")");
        auto allFiles = true;
        juce::StringArray ids;
        for (const auto& entry : *list)
        {
            allFiles = allFiles && entry.file.existsAsFile() && entry.file.isAChildOf (libraryRoot);
            ids.add (entry.id());
        }
        ids.removeDuplicates (false);
        check (allFiles && ids.size() == 8, "varje post har en fil i biblioteket och ett eget id");
        auto personalCount = 0;
        for (const auto& entry : *list)
            personalCount += entry.personal ? 1 : 0;
        check (personalCount == 1, "PERSONAL-posten är märkt");
        const auto problems = index.problems().joinIntoString ("|");
        check (index.problems().size() >= 2 && problems.contains ("BadLicense") && problems.contains ("license") && problems.contains ("MissingFile") && problems.contains ("does not exist"),
               "paket med okänd licens eller saknad fil döljs och syns i problems()");
        check (! problems.isEmpty() && index.folder() == libraryRoot, "indexet minns mappen");

        check (LibraryIndex::filter (*list, "Kicks", {}).size() == 1, "filter på kategori");
        check (LibraryIndex::filter (*list, {}, "kick test").size() == 1, "filter på ord i taggar (alla ord måste finnas)");
        check (LibraryIndex::filter (*list, {}, "TESTER").size() == 1, "filter är skiftlägesokänsligt och söker i upphovsperson");
        check (LibraryIndex::filter (*list, {}, "personal").size() == 1, "filter på licens");
        check (LibraryIndex::filter (*list, {}, "zzz").empty() && LibraryIndex::filter (*list, "Pads", "kick").empty(), "filter utan träff");
        check (LibraryIndex::filter (*list, {}, {}).size() == 8, "tomt filter ger allt");

        // Bakgrundstråden: rescan ger en ny lista, refreshIfChanged gör inget om inget ändrats, och märker en ändring.
        const auto v0 = index.version();
        index.rescan (libraryRoot);
        check (index.waitUntilIdle() && index.version() > v0 && index.entries()->size() == 8, "rescan på bakgrundstråden ger samma lista");
        const auto v1 = index.version();
        index.refreshIfChanged (libraryRoot);
        check (index.waitUntilIdle() && index.version() == v1, "refreshIfChanged skannar inte om inget ändrats");
        makePackage (libraryRoot, "Bass", "Extra", variant ("Test kicks", "Extra"), { "kick1.wav", "sub/kick2.wav" });
        index.refreshIfChanged (libraryRoot);
        check (index.waitUntilIdle() && index.version() > v1 && index.entries()->size() == 10, "refreshIfChanged märker ett nytt paket (" + juce::String ((int) index.entries()->size()) + " poster)");
        const auto v2 = index.version();
        {
            auto manifest = Library::parseFile (libraryRoot.getChildFile ("Bass/Extra").getChildFile (Library::manifestName), true).manifest;
            manifest.items[0].name = "A much longer name than before";
            Library::write (libraryRoot.getChildFile ("Bass/Extra").getChildFile (Library::manifestName), manifest);
        }
        index.refreshIfChanged (libraryRoot);
        check (index.waitUntilIdle() && index.version() > v2, "refreshIfChanged märker ett ändrat manifest");
        index.refreshIfChanged (folders.root.getChildFile ("Annan mapp"));
        check (index.waitUntilIdle() && index.entries()->empty() && index.folder() == folders.root.getChildFile ("Annan mapp"), "en annan mapp skannas om, en saknad mapp ger en tom lista");

        // Förstörs mitt i en beställning utan hängning eller krasch.
        for (int i = 0; i < 5; ++i)
        {
            LibraryIndex quick;
            quick.rescan (libraryRoot);
        }
        check (true, "indexet kan förstöras direkt efter en beställning");
        juce::SharedResourcePointer<LibraryIndex> a, b;
        check (&*a == &*b, "SharedResourcePointer ger samma index till alla");
    }

    // 8i. Stängning mitt i en stor skanning: ~2000 paket (manifest utan poster, inga ljud). Skanningen avbryts mellan paketen utan att publicera,
    // och stopThread når aldrig killThread (shutDown ger true) inom 100 ms.
    {
        const auto big = folders.root.getChildFile ("BigLibrary");
        const juce::String emptyManifest = R"({ "package": "P", "source": "Test", "url": "", "license": "CC0-1.0", "redistributable": true, "date": "2026-10-18", "items": [] })";
        for (int i = 0; i < 2000; ++i)
        {
            const auto folder = big.getChildFile ("Kicks").getChildFile ("P" + juce::String (i));
            folder.createDirectory();
            folder.getChildFile (Library::manifestName).replaceWithText (emptyManifest);
        }
        {
            LibraryIndex index;
            const auto before = index.version();
            index.rescan (big);
            const auto limit = juce::Time::getMillisecondCounter() + 3000;
            while (! index.isScanning() && juce::Time::getMillisecondCounter() < limit)
                juce::Thread::sleep (1);
            const auto wasScanning = index.isScanning();
            const auto start = juce::Time::getMillisecondCounter();
            const auto clean = index.shutDown (1000);
            const auto took = (int) (juce::Time::getMillisecondCounter() - start);
            check (clean && took < 100, "shutDown mitt i en skanning av 2000 paket: " + juce::String (took) + " ms, stoppades utan våld: " + (clean ? "ja" : "NEJ")
                                           + (wasScanning ? juce::String() : juce::String (" (skanningen hann bli klar före stoppet)")));
            check (! wasScanning || index.version() == before, "en avbruten skanning publicerar inget");
        }
        {
            LibraryIndex full;
            const auto start = juce::Time::getMillisecondCounter();
            const auto entries = full.scanNow (big);
            const auto scanMs = (int) (juce::Time::getMillisecondCounter() - start);
            check (entries == 0 && full.problems().isEmpty() && Library::checkFolder (big, false).packages == 2000,
                   "en hel skanning av 2000 paket ger inga fel (" + juce::String (scanMs) + " ms)");
        }
        {
            LibraryIndex idle;
            const auto start = juce::Time::getMillisecondCounter();
            idle.shutDown();
            check ((int) (juce::Time::getMillisecondCounter() - start) < 100, "ett index utan arbete stängs direkt (stopThread väcker tråden)");
        }
    }

    // 8. Inget skrivs i Dokument\Kickbase.
    {
        const auto libraryFolder = Settings::libraryFolder();
        check (libraryFolder.isAChildOf (juce::File::getSpecialLocation (juce::File::tempDirectory)), "libraryFolder() pekar på den tillfälliga mappen");
        check (libraryFolder != realLibrary && ! libraryFolder.isAChildOf (Settings::rootFolder()), "libraryFolder() ligger inte i Dokument\\Kickbase");
        check (realLibrary.exists() == realExisted && realLibrary.getLastModificationTime() == realTime, "Dokument\\Kickbase\\Library är orörd efter testet");
    }

    std::printf (failed ? "Biblioteksmanifestet: FEL\n" : "Biblioteksmanifestet gick igenom\n");
    return failed ? 1 : 0;
}
