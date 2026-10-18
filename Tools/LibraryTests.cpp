// `tool librarycheck [mapp]` och `tool librarytest`: biblioteksmanifesten (Source/Core/LibraryManifest.h). librarycheck kontrollerar alla paket i
// användarens biblioteksmapp (läser bara). librarytest provar reglerna i en tillfällig mapp (Scenarios::TemporaryUserFolders).

#include "../Source/Core/LibraryManifest.h"
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
        for (const auto* file : { "kick.wav", "sub/kick.wav", "sub\\kick.wav", "a/b/c.wav", "name with space.wav", "Kick (1).wav" })
            check (Library::isSafeRelativePath (file), juce::String ("sökvägen godtas: ") + file);
        auto allRejected = true;
        juce::StringArray bad;
        for (const auto* file : { "", " ", "..", ".", "../a.wav", "a/../b.wav", "..\\a.wav", "sub/../../x.wav", "/a.wav", "\\a.wav", "C:\\a.wav", "C:a.wav", "c:/a.wav",
                                  "\\\\server\\share\\a.wav", "//server/share/a.wav", "a//b.wav", "a/", " a.wav", "a.wav ", "./a.wav", "a/./b.wav", "a:b.wav", "a.wav:stream" })
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
