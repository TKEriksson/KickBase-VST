#include "LibraryManifest.h"

namespace Library
{
    const juce::StringArray& categories()
    {
        static const juce::StringArray names { "Kicks", "Bass", "Synth", "Pads", "BassLoops", "SynthLoops", "Multi" };
        return names;
    }

    bool isAllowedLicense (const juce::String& license)
    {
        static const juce::StringArray allowed { "CC0-1.0", "MIT", "BSD-2-Clause", "BSD-3-Clause", "PERSONAL" };
        return allowed.contains (license, false); // exakt och skiftlägeskänsligt
    }

    bool isSafeRelativePath (const juce::String& file)
    {
        if (file.isEmpty() || file != file.trim())
            return false;
        if (file.startsWithChar ('/') || file.startsWithChar ('\\'))
            return false; // absolut, och UNC (\\server)
        for (auto c : file)
            if (c < 32 || c == ':' || c == '<' || c == '>' || c == '"' || c == '|' || c == '?' || c == '*')
                return false; // enhetsbokstav (C:), alternativa dataströmmar och tecken som Windows inte tillåter
        juce::StringArray parts;
        parts.addTokens (file.replaceCharacter ('\\', '/'), "/", "");
        for (const auto& part : parts) // addTokens utan citat: tomma delar blir tomma strängar
            if (part.isEmpty() || part == "." || part == ".." || part.endsWithChar ('.') || part.endsWithChar (' '))
                return false;
        static const juce::StringArray reserved { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                                  "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9" };
        for (const auto& part : parts) // Windows reserverade enhetsnamn, också med filändelse ("nul.wav")
            if (reserved.contains (part.upToFirstOccurrenceOf (".", false, false).trimEnd().toUpperCase()))
                return false;
        return true;
    }

    bool Item::operator== (const Item& other) const
    {
        return file == other.file && name == other.name && tags == other.tags && rootNote == other.rootNote
            && juce::approximatelyEqual (bpm, other.bpm) && key == other.key && author == other.author;
    }

    bool Manifest::operator== (const Manifest& other) const
    {
        return package == other.package && source == other.source && url == other.url && license == other.license
            && redistributable == other.redistributable && date == other.date && items == other.items;
    }

    bool isValidDate (const juce::String& text) // ÅÅÅÅ-MM-DD
    {
        if (text.length() != 10 || text[4] != '-' || text[7] != '-')
            return false;
        for (int i = 0; i < 10; ++i)
            if (i != 4 && i != 7 && ! juce::CharacterFunctions::isDigit (text[i]))
                return false;
        const auto month = text.substring (5, 7).getIntValue();
        const auto day = text.substring (8, 10).getIntValue();
        return month >= 1 && month <= 12 && day >= 1 && day <= 31;
    }

    namespace
    {
        bool isNumber (const juce::var& value) { return value.isInt() || value.isInt64() || value.isDouble(); }

        // Läser ett obligatoriskt textfält. Fel läggs i `errors` och ger false.
        bool requiredString (const juce::var& object, const char* field, const juce::String& where, juce::String& out, juce::StringArray& errors, bool mayBeEmpty = false)
        {
            const auto* dynamic = object.getDynamicObject();
            if (dynamic == nullptr || ! dynamic->hasProperty (field))
            {
                errors.add (where + ": '" + field + "' is missing");
                return false;
            }
            const auto& value = dynamic->getProperty (field);
            if (! value.isString())
            {
                errors.add (where + ": '" + field + "' must be text");
                return false;
            }
            out = value.toString();
            if (! mayBeEmpty && out.trim().isEmpty())
            {
                errors.add (where + ": '" + field + "' is empty");
                return false;
            }
            return true;
        }

        void readOptionalString (const juce::var& object, const char* field, const juce::String& where, juce::String& out, juce::StringArray& errors)
        {
            const auto* dynamic = object.getDynamicObject();
            if (dynamic == nullptr || ! dynamic->hasProperty (field))
                return;
            const auto& value = dynamic->getProperty (field);
            if (value.isString())
                out = value.toString();
            else if (! value.isVoid())
                errors.add (where + ": '" + field + "' must be text");
        }

        void parseItem (const juce::var& value, int index, const juce::String& label, const juce::File& packageFolder, bool checkFilesExist,
                        std::vector<Item>& items, juce::StringArray& seenFiles, juce::StringArray& errors)
        {
            const auto where = label + ": items[" + juce::String (index) + "]";
            if (value.getDynamicObject() == nullptr) // (var::isObject() är också sant för en lista)
            {
                errors.add (where + ": must be an object");
                return;
            }
            Item item;
            auto ok = requiredString (value, "file", where, item.file, errors);
            ok = requiredString (value, "name", where, item.name, errors) && ok;
            if (ok && ! isSafeRelativePath (item.file))
            {
                errors.add (where + ": 'file' must be a plain relative path inside the package (no '..', no drive letter, no absolute or UNC path): " + item.file);
                ok = false;
            }
            else if (ok && packageFolder != juce::File())
            {
                if (! packageFolder.getChildFile (item.file).isAChildOf (packageFolder))
                {
                    errors.add (where + ": 'file' resolves outside the package folder: " + item.file);
                    ok = false;
                }
                else if (checkFilesExist && ! packageFolder.getChildFile (item.file).existsAsFile())
                {
                    errors.add (where + ": 'file' does not exist: " + item.file);
                    ok = false;
                }
            }
            if (ok)
            {
                const auto normalised = item.file.replaceCharacter ('\\', '/').toLowerCase(); // Windows skiljer inte på versaler
                if (seenFiles.contains (normalised))
                {
                    errors.add (where + ": 'file' is listed twice: " + item.file);
                    ok = false;
                }
                seenFiles.add (normalised);
            }

            const auto* dynamic = value.getDynamicObject();
            if (dynamic->hasProperty ("tags"))
            {
                const auto& tags = dynamic->getProperty ("tags");
                if (const auto* array = tags.getArray())
                {
                    for (const auto& tag : *array)
                    {
                        if (tag.isString() && tag.toString().isNotEmpty())
                            item.tags.add (tag.toString());
                        else
                            errors.add (where + ": 'tags' must be a list of non-empty text");
                    }
                }
                else if (! tags.isVoid())
                    errors.add (where + ": 'tags' must be a list of text");
            }
            if (dynamic->hasProperty ("rootNote"))
            {
                const auto& note = dynamic->getProperty ("rootNote");
                if (isNumber (note) && juce::approximatelyEqual ((double) note, std::floor ((double) note)) && (double) note >= 0.0 && (double) note <= 127.0)
                    item.rootNote = (int) note;
                else
                    errors.add (where + ": 'rootNote' must be a whole number 0-127");
            }
            if (dynamic->hasProperty ("bpm"))
            {
                const auto& bpm = dynamic->getProperty ("bpm");
                if (isNumber (bpm) && (double) bpm > 0.0 && (double) bpm <= 999.0)
                    item.bpm = (double) bpm;
                else
                    errors.add (where + ": 'bpm' must be a number above 0 and up to 999");
            }
            readOptionalString (value, "key", where, item.key, errors);
            readOptionalString (value, "author", where, item.author, errors);
            if (ok)
                items.push_back (std::move (item));
        }
    }

    ParseResult parse (const juce::String& json, const juce::String& label, const juce::File& packageFolder, bool checkFilesExist)
    {
        ParseResult result;
        juce::var root;
        if (const auto parsed = juce::JSON::parse (json, root); parsed.failed())
        {
            result.errors.add (label + ": not valid JSON (" + parsed.getErrorMessage() + ")");
            return result;
        }
        if (root.isVoid())
        {
            result.errors.add (label + ": not valid JSON (empty)");
            return result;
        }
        if (root.getDynamicObject() == nullptr) // var::isObject() är också sant för en lista
        {
            result.errors.add (label + ": the manifest must be a JSON object");
            return result;
        }

        auto& manifest = result.manifest;
        auto& errors = result.errors;
        requiredString (root, "package", label, manifest.package, errors);
        const auto name = manifest.package.isNotEmpty() ? manifest.package : label; // felen för övriga fält nämner paketet
        requiredString (root, "source", name, manifest.source, errors);
        requiredString (root, "url", name, manifest.url, errors, true); // en egen inspelning har ingen webbadress
        requiredString (root, "date", name, manifest.date, errors);
        if (manifest.date.isNotEmpty() && ! isValidDate (manifest.date))
            errors.add (name + ": 'date' must look like YYYY-MM-DD");

        const auto licenseOk = requiredString (root, "license", name, manifest.license, errors);
        if (licenseOk && ! isAllowedLicense (manifest.license))
            errors.add (name + ": 'license' is not allowed: \"" + manifest.license + "\" (allowed: CC0-1.0, MIT, BSD-2-Clause, BSD-3-Clause, PERSONAL)");

        const auto* object = root.getDynamicObject();
        if (! object->hasProperty ("redistributable"))
            errors.add (name + ": 'redistributable' is missing");
        else if (! object->getProperty ("redistributable").isBool())
            errors.add (name + ": 'redistributable' must be true or false");
        else
        {
            manifest.redistributable = (bool) object->getProperty ("redistributable");
            if (licenseOk && isAllowedLicense (manifest.license) && manifest.redistributable == isPersonal (manifest.license))
                errors.add (name + ": 'redistributable' must be " + (isPersonal (manifest.license) ? "false" : "true")
                            + " for the license " + manifest.license + " (false if and only if PERSONAL)");
        }

        if (! object->hasProperty ("items"))
            errors.add (name + ": 'items' is missing");
        else if (const auto* items = object->getProperty ("items").getArray())
        {
            juce::StringArray seenFiles;
            for (int i = 0; i < items->size(); ++i)
                parseItem (items->getReference (i), i, name, packageFolder, checkFilesExist, manifest.items, seenFiles, errors);
        }
        else
            errors.add (name + ": 'items' must be a list");
        return result;
    }

    ParseResult parseFile (const juce::File& manifestFile, bool checkFilesExist)
    {
        const auto label = manifestFile.getParentDirectory().getFileName();
        if (! manifestFile.existsAsFile())
        {
            ParseResult result;
            result.errors.add (label + ": " + manifestName + " is missing");
            return result;
        }
        return parse (manifestFile.loadFileAsString(), label, manifestFile.getParentDirectory(), checkFilesExist);
    }

    juce::var toVar (const Manifest& manifest)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty ("package", manifest.package);
        object->setProperty ("source", manifest.source);
        object->setProperty ("url", manifest.url);
        object->setProperty ("license", manifest.license);
        object->setProperty ("redistributable", manifest.redistributable);
        object->setProperty ("date", manifest.date);
        juce::Array<juce::var> items;
        for (const auto& item : manifest.items)
        {
            auto* entry = new juce::DynamicObject();
            entry->setProperty ("file", item.file);
            entry->setProperty ("name", item.name);
            juce::Array<juce::var> tags;
            for (const auto& tag : item.tags)
                tags.add (tag);
            entry->setProperty ("tags", tags);
            if (item.rootNote >= 0)
                entry->setProperty ("rootNote", item.rootNote);
            if (item.bpm > 0.0)
                entry->setProperty ("bpm", item.bpm);
            if (item.key.isNotEmpty())
                entry->setProperty ("key", item.key);
            if (item.author.isNotEmpty())
                entry->setProperty ("author", item.author);
            items.add (juce::var (entry));
        }
        object->setProperty ("items", items);
        return juce::var (object);
    }

    juce::String toJson (const Manifest& manifest)
    {
        return juce::JSON::toString (toVar (manifest), false);
    }

    bool write (const juce::File& manifestFile, const Manifest& manifest)
    {
        if (! manifestFile.getParentDirectory().createDirectory())
            return false;
        return manifestFile.replaceWithText (toJson (manifest));
    }

    CheckReport checkFolder (const juce::File& root, bool checkFilesExist)
    {
        CheckReport report;
        if (! root.isDirectory())
        {
            report.lines.add ("no library folder: " + root.getFullPathName());
            return report;
        }
        for (const auto& categoryFolder : root.findChildFiles (juce::File::findDirectories, false))
        {
            const auto category = categoryFolder.getFileName();
            if (category.startsWithChar ('.'))
                continue;
            if (! categories().contains (category))
            {
                report.errors.add (category + ": not a library category (" + categories().joinIntoString (", ") + ")");
                continue;
            }
            for (const auto& packageFolder : categoryFolder.findChildFiles (juce::File::findDirectories, false))
            {
                if (packageFolder.getFileName().startsWithChar ('.'))
                    continue;
                ++report.packages;
                const auto result = parseFile (packageFolder.getChildFile (manifestName), checkFilesExist);
                const auto id = category + "\\" + packageFolder.getFileName();
                if (result.ok())
                {
                    report.items += (int) result.manifest.items.size();
                    report.personalPackages += isPersonal (result.manifest.license) ? 1 : 0;
                    report.lines.add (id + ": ok, " + juce::String (result.manifest.items.size()) + " items, " + result.manifest.license);
                }
                else
                {
                    report.lines.add (id + ": " + juce::String (result.errors.size()) + " error(s)");
                    for (const auto& error : result.errors)
                    {
                        report.lines.add ("    " + error);
                        report.errors.add (id + ": " + error);
                    }
                }
            }
        }
        report.lines.add (juce::String (report.packages) + " package(s), " + juce::String (report.items) + " items, "
                          + juce::String (report.personalPackages) + " personal, " + juce::String (report.errors.size()) + " error(s)");
        return report;
    }
}
