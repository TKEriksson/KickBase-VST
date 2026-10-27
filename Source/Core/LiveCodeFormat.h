#pragma once

#include <juce_core/juce_core.h>

// D4 steg 3: autoformat för Live Code. Ren funktion (som LiveCodeComplete) så att den går att testa utan gränssnitt.
// Varje sats ($: ...) får en kedja per rad, indragen två steg, och s("...") hamnar först.
// Satser med kommentarer (//) eller obalanserade parenteser/strängar lämnas orörda.
namespace LiveCodeFormat
{
    // Delar en sats vid punkter på nivå 0 som följs av en bokstav. Tom lista = kunde inte tolkas.
    inline juce::StringArray splitChain (const juce::String& statement)
    {
        juce::StringArray parts;
        int depth = 0, start = 0;
        juce::juce_wchar quote = 0;
        for (int i = 0; i < statement.length(); ++i)
        {
            const auto c = statement[i];
            if (quote != 0)
            {
                if (c == '\\') ++i;
                else if (c == quote) quote = 0;
                continue;
            }
            if (c == '"' || c == '\'' || c == '`') quote = c;
            else if (c == '(' || c == '[' || c == '{') ++depth;
            else if (c == ')' || c == ']' || c == '}') --depth;
            else if (c == '.' && depth == 0 && i > start && juce::CharacterFunctions::isLetter (statement[i + 1]))
            {
                parts.add (statement.substring (start, i).trim());
                start = i + 1;
            }
            if (depth < 0) return {};
        }
        if (depth != 0 || quote != 0) return {};
        parts.add (statement.substring (start).trim());
        return parts;
    }

    inline juce::String formatStatement (const juce::String& statement)
    {
        if (statement.contains ("//") || statement.contains ("/*"))
            return statement;
        auto body = statement.trim();
        juce::String prefix;
        if (body.startsWith ("$:"))
        {
            prefix = "$: ";
            body = body.substring (2).trim();
        }
        auto parts = splitChain (body);
        if (parts.size() < 2)
            return statement;

        // s("...") först: flytta den första s-anropet om huvudet inte redan är ett.
        if (! parts[0].startsWith ("s("))
            for (int i = 1; i < parts.size(); ++i)
                if (parts[i].startsWith ("s("))
                {
                    const auto s = parts[i];
                    parts.remove (i);
                    parts.insert (0, s);
                    break;
                }

        juce::String out = prefix + parts[0];
        for (int i = 1; i < parts.size(); ++i)
            out << "\n  ." << parts[i];
        return out;
    }

    // Hela texten: en sats börjar på en rad som inte är indragen eller börjar med punkt; resten av satsen är fortsättningsrader.
    inline juce::String format (const juce::String& text)
    {
        const auto lines = juce::StringArray::fromLines (text);
        juce::StringArray out;
        juce::String statement;
        bool haveStatement = false;
        auto flush = [&]
        {
            if (haveStatement)
                out.add (formatStatement (statement));
            statement = {};
            haveStatement = false;
        };
        for (const auto& line : lines)
        {
            const auto trimmed = line.trim();
            const bool continuation = haveStatement && trimmed.isNotEmpty()
                                      && (line[0] == ' ' || line[0] == '\t' || trimmed.startsWith ("."));
            if (continuation)
            {
                statement << "\n" << trimmed; // radbrytningen behålls så att //-kommentarer inte slukar nästa rad
                continue;
            }
            flush();
            if (trimmed.isEmpty() || trimmed.startsWith ("//"))
                out.add (line);
            else
            {
                statement = line;
                haveStatement = true;
            }
        }
        flush();
        return out.joinIntoString ("\n");
    }
}
