#include "AiChat.h"

namespace
{
    using juce::var;

    var object() { return var (new juce::DynamicObject()); }
    void put (var& target, const char* name, const var& value) { target.getDynamicObject()->setProperty (name, value); }

    const char* systemPrompt =
        "You are the AI sound designer built into Kickbase, a psytrance kick and bass synth plugin. You work on the user's current sound live, "
        "through the tools: you set parameters, write patterns, build effects, render and measure. You cannot hear: use render_preview "
        "(and look at the spectrogram) and the metrics as your ears, and say so when it matters. Reply in the language the user writes in, keep replies short "
        "and concrete, say briefly what you are about to do before bigger changes, and afterwards tell what you changed and why. The user sees your tool calls "
        "and can undo your changes. Only change what was asked; ask if something is unclear. Read the guide below before designing sounds.\n\n# Kickbase guide\n\n";

    juce::String shorten (const juce::String& text, int length)
    {
        return text.length() > length ? text.substring (0, length) + " ..." : text;
    }
}

AiChat::AiChat (PluginBase& p) : juce::Thread ("Kickbase AI chat"), processor (p), tools (p) {}

AiChat::~AiChat()
{
    *alive = false;
    signalThreadShouldExit();
    stopThread (15000);
}

juce::StringArray AiChat::models()
{
    return { "claude-sonnet-5-5", "claude-opus-5-5", "claude-haiku-5-5", "claude-fable-5-1" };
}

juce::String AiChat::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return status;
}

void AiChat::setStatus (const juce::String& text)
{
    {
        const juce::ScopedLock sl (lock);
        status = text;
    }
    ++version;
}

void AiChat::addEntry (Entry entry)
{
    {
        const juce::ScopedLock sl (lock);
        log.push_back (std::move (entry));
    }
    ++version;
}

std::vector<AiChat::Entry> AiChat::getEntries() const
{
    const juce::ScopedLock sl (lock);
    return log;
}

juce::String AiChat::getUsage() const
{
    auto format = [] (juce::int64 tokens) { return tokens >= 1000 ? juce::String ((double) tokens / 1000.0, 1) + "k" : juce::String (tokens); };
    return tokensIn.load() + tokensOut.load() == 0 ? juce::String() : format (tokensIn.load()) + " in / " + format (tokensOut.load()) + " out";
}

void AiChat::newChat()
{
    if (busy)
        return;
    {
        const juce::ScopedLock sl (lock);
        log.clear();
        status = {};
    }
    messages.clear();
    tokensIn = 0;
    tokensOut = 0;
    ++version;
}

void AiChat::undo()
{
    if (busy)
        return;
    const auto result = tools.call ("undo", object());
    Entry entry;
    entry.kind = (bool) result["ok"] ? Entry::Kind::note : Entry::Kind::error;
    entry.text = (bool) result["ok"] ? "Undid the last change." : result["error"].toString();
    addEntry (std::move (entry));
}

void AiChat::stop()
{
    if (busy)
    {
        signalThreadShouldExit();
        setStatus ("Stopping ...");
    }
}

void AiChat::send (const juce::String& text)
{
    if (busy || text.trim().isEmpty())
        return;

    if (Settings::chatApiKey().isEmpty())
    {
        Entry entry;
        entry.kind = Entry::Kind::error;
        entry.text = "Add your API key first (the KEY button).";
        addEntry (std::move (entry));
        return;
    }

    Entry entry;
    entry.kind = Entry::Kind::user;
    entry.text = text.trim();
    addEntry (std::move (entry));

    auto message = object();
    put (message, "role", "user");
    put (message, "content", text.trim());
    messages.add (message);

    busy = true;
    startThread (juce::Thread::Priority::normal);
}

var AiChat::buildRequest() const
{
    auto body = object();
    put (body, "model", Settings::chatModel());
    put (body, "max_tokens", 8192);

    // Systemprompten med guiden. Den cachas (cache_control), så att varje anrop i samtalet kostar mindre.
    auto system = object();
    put (system, "type", "text");
    put (system, "text", juce::String (systemPrompt) + ToolLayer::guide ("agent"));
    auto cache = object();
    put (cache, "type", "ephemeral");
    put (system, "cache_control", cache);
    put (body, "system", juce::Array<var> { system });

    juce::Array<var> toolList;
    for (const auto& info : ToolLayer::tools())
    {
        auto tool = object();
        put (tool, "name", info.name);
        put (tool, "description", info.description);
        auto schema = juce::JSON::parse (info.schema);
        if (! schema.isObject())
        {
            schema = object();
            put (schema, "type", "object");
            put (schema, "properties", object());
        }
        put (tool, "input_schema", schema);
        toolList.add (tool);
    }
    if (! toolList.isEmpty())
        put (toolList.getReference (toolList.size() - 1), "cache_control", cache);
    put (body, "tools", toolList);
    put (body, "messages", messages);
    return body;
}

var AiChat::post (const juce::String& key, const var& body, int& status, juce::String& error)
{
    status = 0;
    juce::URL url (Settings::get ("chatEndpoint", "https://api.anthropic.com/v1/messages")); // (chatEndpoint: bara för tester)
    url = url.withPOSTData (juce::JSON::toString (body, true));
    const juce::String headers = "content-type: application/json\r\nx-api-key: " + key + "\r\nanthropic-version: 2023-06-01\r\n";

    std::unique_ptr<juce::InputStream> stream (url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                                                          .withExtraHeaders (headers)
                                                                          .withConnectionTimeoutMs (240000)
                                                                          .withStatusCode (&status)
                                                                          .withNumRedirectsToFollow (2)
                                                                          .withHttpRequestCmd ("POST")));
    if (stream == nullptr)
    {
        error = "Could not reach api.anthropic.com (no internet, a firewall, or a proxy?).";
        return {};
    }

    const auto text = stream->readEntireStreamAsString();
    const auto parsed = juce::JSON::parse (text);
    if (status >= 400 || parsed["type"].toString() == "error")
    {
        const auto message = parsed["error"]["message"].toString();
        error = status == 401 ? "The API key was not accepted (401). Check it with the KEY button."
              : status == 429 ? "Rate limit or no credit left on your Anthropic account (429): " + message
              : "The AI service answered with an error" + (status > 0 ? " (" + juce::String (status) + ")" : juce::String()) + ": " + (message.isNotEmpty() ? message : shorten (text, 300));
        return {};
    }
    if (! parsed.isObject())
    {
        error = "Unexpected answer from the AI service: " + shorten (text, 300);
        return {};
    }
    return parsed;
}

// Verktygen körs på message thread (som när MCP-bryggan använder dem); bakgrundstråden väntar på svaret.
var AiChat::callTool (const juce::String& name, const var& input)
{
    struct Pending
    {
        var result;
        juce::WaitableEvent done;
    };
    auto pending = std::make_shared<Pending>();
    auto* toolLayer = &tools;
    juce::MessageManager::callAsync ([pending, toolLayer, name, input, stillAlive = alive]
    {
        if (*stillAlive)
            pending->result = toolLayer->call (name, input);
        pending->done.signal();
    });

    auto finished = false;
    while (! (finished = pending->done.wait (200)))
        if (threadShouldExit())
            break; // fönstret stängs: släpp (resultatet läggs i den delade strukturen och försvinner)
    if (! finished)
    {
        auto failure = object();
        put (failure, "ok", false);
        put (failure, "error", "Cancelled.");
        return failure;
    }
    return pending->result;
}

void AiChat::run()
{
    const auto key = Settings::chatApiKey();
    const auto sizeAtStart = messages.size();
    auto failed = false;

    for (int step = 0; step < maxSteps && ! threadShouldExit(); ++step)
    {
        setStatus (step == 0 ? "Thinking ..." : "Reading the result ...");
        int httpStatus = 0;
        juce::String error;
        var response;
        const auto request = buildRequest();

        // Övergående fel (ingen anslutning, för många anrop, överbelastad tjänst) provas om efter en stund; felaktig nyckel eller dåligt anrop inte.
        for (int attempt = 0; attempt < 4; ++attempt)
        {
            response = post (key, request, httpStatus, error);
            if (! response.isVoid() || threadShouldExit())
                break;
            const auto transient = httpStatus == 0 || httpStatus == 429 || httpStatus >= 500;
            if (! transient || attempt == 3)
                break;
            const auto waitMs = 2000 << attempt; // 2, 4, 8 sekunder
            setStatus ("Trying again in " + juce::String (waitMs / 1000) + " s (" + juce::String (attempt + 1) + "/3) ...");
            for (int waited = 0; waited < waitMs && ! threadShouldExit(); waited += 100)
                juce::Thread::sleep (100);
            setStatus (step == 0 ? "Thinking ..." : "Reading the result ...");
        }
        if (threadShouldExit())
            break;
        if (response.isVoid())
        {
            Entry entry;
            entry.kind = Entry::Kind::error;
            entry.text = error;
            addEntry (std::move (entry));
            failed = true;
            break;
        }

        const auto& usage = response["usage"];
        tokensIn += (juce::int64) (int) usage["input_tokens"] + (int) usage["cache_creation_input_tokens"] + (int) usage["cache_read_input_tokens"];
        tokensOut += (juce::int64) (int) usage["output_tokens"];

        const auto& content = response["content"];
        if (! content.isArray())
        {
            Entry entry;
            entry.kind = Entry::Kind::error;
            entry.text = "The AI service gave no content.";
            addEntry (std::move (entry));
            failed = true;
            break;
        }

        auto assistant = object();
        put (assistant, "role", "assistant");
        put (assistant, "content", content);
        messages.add (assistant);

        juce::Array<var> results;
        for (const auto& block : *content.getArray())
        {
            const auto type = block["type"].toString();
            if (type == "text")
            {
                if (block["text"].toString().trim().isNotEmpty())
                {
                    Entry entry;
                    entry.kind = Entry::Kind::assistant;
                    entry.text = block["text"].toString().trim();
                    addEntry (std::move (entry));
                }
            }
            else if (type == "tool_use")
            {
                const auto name = block["name"].toString();
                setStatus ("Running " + name + " ...");

                const auto result = callTool (name, block["input"]);
                const auto ok = (bool) result["ok"];

                Entry entry;
                entry.kind = ok ? Entry::Kind::tool : Entry::Kind::error;
                entry.text = name + "  " + shorten (juce::JSON::toString (block["input"], true), 160) + (ok ? "" : "\n" + result["error"].toString());

                // Resultatet till AI:n: texten (data) och eventuella bilder (spektrogram).
                juce::Array<var> parts;
                auto textPart = object();
                put (textPart, "type", "text");
                const auto& data = result["data"];
                put (textPart, "text", ok ? shorten (data.isString() ? data.toString() : juce::JSON::toString (data, false, 4), 30000) : result["error"].toString());
                parts.add (textPart);
                if (ok)
                    if (auto* images = result["images"].getArray())
                        for (const auto& image : *images)
                        {
                            auto imagePart = object();
                            put (imagePart, "type", "image");
                            auto source = object();
                            put (source, "type", "base64");
                            put (source, "media_type", "image/png");
                            put (source, "data", image["data"]);
                            put (imagePart, "source", source);
                            parts.add (imagePart);

                            juce::MemoryOutputStream decoded;
                            if (juce::Base64::convertFromBase64 (decoded, image["data"].toString()))
                                if (auto loaded = juce::ImageFileFormat::loadFrom (decoded.getData(), decoded.getDataSize()); loaded.isValid())
                                    entry.images.push_back (loaded);
                        }
                addEntry (std::move (entry));

                auto toolResult = object();
                put (toolResult, "type", "tool_result");
                put (toolResult, "tool_use_id", block["id"]);
                put (toolResult, "content", parts);
                if (! ok)
                    put (toolResult, "is_error", true);
                results.add (toolResult);
            }
        }

        if (response["stop_reason"].toString() != "tool_use" || results.isEmpty())
        {
            if (response["stop_reason"].toString() == "max_tokens")
            {
                Entry entry;
                entry.kind = Entry::Kind::note;
                entry.text = "The answer was cut off (too long). Write 'continue' to go on.";
                addEntry (std::move (entry));
            }
            break;
        }

        auto resultMessage = object();
        put (resultMessage, "role", "user");
        put (resultMessage, "content", results);
        messages.add (resultMessage);

        if (step == maxSteps - 1)
        {
            Entry entry;
            entry.kind = Entry::Kind::note;
            entry.text = "Stopped after " + juce::String (maxSteps) + " steps. Write 'continue' to go on.";
            addEntry (std::move (entry));
        }
    }

    // Avbruten eller misslyckad mitt i ett verktygsanrop: ta bort halvfärdiga meddelanden, så att samtalet förblir giltigt.
    if (threadShouldExit() || failed)
    {
        while (messages.size() > sizeAtStart)
            messages.remove (messages.size() - 1);
        if (threadShouldExit() && ! failed)
        {
            Entry entry;
            entry.kind = Entry::Kind::note;
            entry.text = "Stopped.";
            addEntry (std::move (entry));
        }
    }
    else if (! messages.isEmpty())
    {
        // Samtalet måste sluta med en assistent-tur (annars två användarturer efter varandra): det gör det redan, utom efter stegtaket.
        const auto& last = messages.getReference (messages.size() - 1);
        if (last["role"].toString() == "user" && last["content"].isArray())
            messages.remove (messages.size() - 1), messages.remove (messages.size() - 1);
    }

    busy = false;
    setStatus ({});
}
