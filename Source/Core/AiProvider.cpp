#include "AiProvider.h"

#include <map>

namespace
{
    using juce::var;

    var object() { return var (new juce::DynamicObject()); }
    void put (var& target, const char* name, const var& value) { target.getDynamicObject()->setProperty (name, value); }

    bool hasFields (const var& schema)
    {
        auto* properties = schema["properties"].getDynamicObject();
        return properties != nullptr && properties->getProperties().size() > 0;
    }

    // Ett objekt utan fasta fält (t.ex. en karta från parameternamn till värde): Gemini kan inte beskriva det, så det skickas som JSON-text.
    bool isFreeForm (const var& schema)
    {
        return schema.isObject() && schema["type"].isString() && schema["type"].toString() == "object" && ! hasFields (schema);
    }

    void restore (const var& schema, var& value)
    {
        if (! schema.isObject() || ! schema["type"].isString())
            return;
        const auto type = schema["type"].toString();
        if (type == "object")
        {
            if (isFreeForm (schema))
            {
                if (value.isString())
                    if (auto parsed = juce::JSON::parse (value.toString()); parsed.isObject())
                        value = parsed;
                return;
            }
            if (auto* properties = schema["properties"].getDynamicObject(); properties != nullptr && value.isObject())
                for (const auto& property : properties->getProperties())
                    if (value.getDynamicObject()->hasProperty (property.name))
                    {
                        auto child = value[property.name];
                        restore (property.value, child);
                        value.getDynamicObject()->setProperty (property.name, child);
                    }
        }
        else if (type == "array" && value.isArray())
            for (auto& item : *value.getArray())
                restore (schema["items"], item);
    }

    // Texten i ett tool_result-innehåll (sträng eller delar) och dess bilder som Gemini-delar.
    juce::String resultText (const var& content, juce::Array<var>& images)
    {
        if (content.isString())
            return content.toString();
        juce::String text;
        if (auto* parts = content.getArray())
            for (const auto& part : *parts)
            {
                const auto type = part["type"].toString();
                if (type == "text")
                    text += (text.isEmpty() ? "" : "\n") + part["text"].toString();
                else if (type == "image")
                {
                    auto inlineData = object();
                    put (inlineData, "mimeType", part["source"]["media_type"].toString().isEmpty() ? juce::String ("image/png") : part["source"]["media_type"].toString());
                    put (inlineData, "data", part["source"]["data"]);
                    auto image = object();
                    put (image, "inlineData", inlineData);
                    images.add (image);
                }
            }
        return text;
    }

    juce::Array<var> convertMessages (const juce::Array<var>& messages)
    {
        juce::Array<var> contents;
        std::map<juce::String, juce::String> toolNames; // tool_use-id -> verktygsnamn (Gemini vill ha namnet i svaret)

        for (const auto& message : messages)
        {
            const auto isUser = message["role"].toString() == "user";
            const auto& content = message["content"];
            juce::Array<var> parts;

            if (content.isString())
            {
                auto part = object();
                put (part, "text", content);
                parts.add (part);
            }
            else if (auto* blocks = content.getArray())
            {
                juce::Array<var> images;
                for (const auto& block : *blocks)
                {
                    const auto type = block["type"].toString();
                    if (type == "text" && block["text"].toString().isNotEmpty())
                    {
                        auto part = object();
                        put (part, "text", block["text"]);
                        if (block.hasProperty ("thoughtSignature"))
                            put (part, "thoughtSignature", block["thoughtSignature"]);
                        parts.add (part);
                    }
                    else if (type == "tool_use")
                    {
                        toolNames[block["id"].toString()] = block["name"].toString();
                        auto call = object();
                        put (call, "name", block["name"]);
                        if (! block["id"].toString().startsWith ("gemini-")) // Geminis eget anrops-id skickas tillbaka oförändrat
                            put (call, "id", block["id"]);
                        put (call, "args", block["input"].isObject() ? block["input"] : object());
                        auto part = object();
                        put (part, "functionCall", call);
                        if (block.hasProperty ("thoughtSignature"))
                            put (part, "thoughtSignature", block["thoughtSignature"]);
                        parts.add (part);
                    }
                    else if (type == "tool_result")
                    {
                        auto response = object();
                        put (response, (bool) block["is_error"] ? "error" : "output", resultText (block["content"], images));
                        auto result = object();
                        put (result, "name", toolNames[block["tool_use_id"].toString()]);
                        if (! block["tool_use_id"].toString().startsWith ("gemini-"))
                            put (result, "id", block["tool_use_id"]);
                        put (result, "response", response);
                        auto part = object();
                        put (part, "functionResponse", result);
                        parts.add (part);
                    }
                }
                parts.addArray (images); // bilderna (spektrogram) efter svaren
            }

            if (parts.isEmpty())
                continue;
            auto entry = object();
            put (entry, "role", isUser ? "user" : "model");
            put (entry, "parts", parts);
            contents.add (entry);
        }
        return contents;
    }

    juce::String shortened (const juce::String& text, int length)
    {
        return text.length() > length ? text.substring (0, length) + " ..." : text;
    }
}

namespace GeminiApi
{
    juce::String endpoint (const juce::String& model)
    {
        return "https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent";
    }

    namespace
    {
        juce::CriticalSection fetchedLock;
        juce::StringArray fetchedModels; // listan från models.list (tom = inte hämtad ännu)
    }

    juce::StringArray fallbackModels()
    {
        return { "gemini-3.6-flash", "gemini-3.5-flash-lite", "gemini-3.1-flash-lite", "gemini-3.8-flash", "gemini-3.1-pro-preview" };
    }

    juce::StringArray models()
    {
        const juce::ScopedLock sl (fetchedLock);
        return fetchedModels.isEmpty() ? fallbackModels() : fetchedModels;
    }

    void setFetchedModels (const juce::StringArray& list)
    {
        const juce::ScopedLock sl (fetchedLock);
        fetchedModels = list;
    }

    // models.list: models[].name ("models/gemini-..."), supportedGenerationMethods[]. Bara textmodeller som kan generateContent.
    juce::StringArray parseModelList (const var& response)
    {
        juce::StringArray result;
        if (auto* list = response["models"].getArray())
            for (const auto& model : *list)
            {
                auto name = model["name"].toString();
                if (name.startsWith ("models/"))
                    name = name.substring (7);
                if (! name.startsWith ("gemini-"))
                    continue;
                bool canGenerate = false;
                if (auto* methods = model["supportedGenerationMethods"].getArray())
                    for (const auto& method : *methods)
                        canGenerate = canGenerate || method.toString() == "generateContent";
                if (! canGenerate)
                    continue;
                bool special = false;
                for (const char* word : { "tts", "image", "live", "audio", "embedding", "computer-use", "robotics", "native" })
                    special = special || name.contains (word);
                if (! special)
                    result.addIfNotAlreadyThere (name);
            }
        return result;
    }

    var sanitizeSchema (const var& schema)
    {
        if (! schema.isObject())
            return schema;

        // Flera typer (["array","string"]): anyOf; "null" blir nullable.
        if (auto* types = schema["type"].getArray())
        {
            juce::Array<var> alternatives;
            auto nullable = false;
            for (const auto& type : *types)
            {
                if (type.toString() == "null") { nullable = true; continue; }
                auto copy = schema.clone();
                copy.getDynamicObject()->setProperty ("type", type);
                alternatives.add (copy);
            }
            var result;
            if (alternatives.size() == 1)
                result = sanitizeSchema (alternatives[0]);
            else
            {
                result = object();
                juce::Array<var> cleaned;
                for (auto alternative : alternatives)
                {
                    alternative.getDynamicObject()->removeProperty ("description");
                    cleaned.add (sanitizeSchema (alternative));
                }
                put (result, "anyOf", cleaned);
                if (schema["description"].toString().isNotEmpty())
                    put (result, "description", schema["description"]);
            }
            if (nullable && result.isObject())
                put (result, "nullable", true);
            return result;
        }

        auto out = object();
        const auto type = schema["type"].toString();
        for (const char* key : { "type", "format", "description", "nullable", "minimum", "maximum", "minItems", "maxItems", "title" })
            if (schema.hasProperty (key))
                put (out, key, schema[key]);

        if (schema.hasProperty ("enum") && type == "string")
        {
            juce::Array<var> values;
            for (const auto& value : *schema["enum"].getArray())
                values.add (value.toString());
            put (out, "enum", values);
        }

        if (type == "object")
        {
            if (! hasFields (schema))
            {
                auto text = object();
                put (text, "type", "string");
                put (text, "description", (schema["description"].toString() + " Pass it as a JSON object written as a string, e.g. {\"name\": 1}.").trim());
                return text;
            }
            auto properties = object();
            for (const auto& property : schema["properties"].getDynamicObject()->getProperties())
                put (properties, property.name.toString().toRawUTF8(), sanitizeSchema (property.value));
            put (out, "properties", properties);

            if (auto* required = schema["required"].getArray())
            {
                juce::Array<var> kept;
                for (const auto& name : *required)
                    if (properties.getDynamicObject()->hasProperty (juce::Identifier (name.toString())))
                        kept.add (name);
                if (! kept.isEmpty())
                    put (out, "required", kept);
            }
        }
        else if (type == "array")
        {
            if (schema["items"].isObject())
                put (out, "items", sanitizeSchema (schema["items"]));
            else
            {
                auto items = object();
                put (items, "type", "string");
                put (out, "items", items);
            }
        }
        return out;
    }

    var buildRequest (const juce::String& system, const std::vector<ToolLayer::ToolInfo>& tools, const juce::Array<var>& messages, int maxOutputTokens)
    {
        auto body = object();

        auto systemPart = object();
        put (systemPart, "text", system);
        auto systemInstruction = object();
        put (systemInstruction, "parts", juce::Array<var> { systemPart });
        put (body, "systemInstruction", systemInstruction);

        put (body, "contents", convertMessages (messages));

        juce::Array<var> declarations;
        for (const auto& info : tools)
        {
            auto declaration = object();
            put (declaration, "name", info.name);
            put (declaration, "description", info.description);
            const auto schema = juce::JSON::parse (info.schema);
            if (schema.isObject() && hasFields (schema)) // Gemini tar inte ett objekt utan fält: verktyg utan argument får ingen parameters
                put (declaration, "parameters", sanitizeSchema (schema));
            declarations.add (declaration);
        }
        if (! declarations.isEmpty())
        {
            auto tool = object();
            put (tool, "functionDeclarations", declarations);
            put (body, "tools", juce::Array<var> { tool });
        }

        auto config = object();
        put (config, "maxOutputTokens", maxOutputTokens); // tänkandet räknas in i taket
        put (body, "generationConfig", config);
        return body;
    }

    var parseResponse (const var& response, const std::vector<ToolLayer::ToolInfo>& tools, juce::String& error)
    {
        auto* candidates = response["candidates"].getArray();
        if (candidates == nullptr || candidates->isEmpty())
        {
            const auto reason = response["promptFeedback"]["blockReason"].toString();
            error = reason.isNotEmpty() ? "Gemini refused the request (" + reason + "). Try rephrasing it."
                                        : "Gemini gave no answer. Try again.";
            return {};
        }

        const auto& candidate = (*candidates)[0];
        const auto finish = candidate["finishReason"].toString();

        juce::Array<var> content;
        auto hasToolUse = false;
        if (auto* parts = candidate["content"]["parts"].getArray())
            for (const auto& part : *parts)
            {
                if ((bool) part["thought"])
                    continue; // tankesammanfattning: visas inte
                if (part.hasProperty ("functionCall"))
                {
                    const auto name = part["functionCall"]["name"].toString();
                    auto input = part["functionCall"]["args"].isObject() ? part["functionCall"]["args"] : object();
                    for (const auto& info : tools)
                        if (info.name == name)
                        {
                            restore (juce::JSON::parse (info.schema), input);
                            break;
                        }
                    auto block = object();
                    put (block, "type", "tool_use");
                    const auto callId = part["functionCall"]["id"].toString(); // Gemini 3 ger varje anrop ett id som svaret ska ange
                    put (block, "id", callId.isNotEmpty() ? callId : "gemini-" + juce::Uuid().toString());
                    put (block, "name", name);
                    put (block, "input", input);
                    if (part.hasProperty ("thoughtSignature"))
                        put (block, "thoughtSignature", part["thoughtSignature"]);
                    content.add (block);
                    hasToolUse = true;
                }
                else if (part["text"].toString().isNotEmpty())
                {
                    auto block = object();
                    put (block, "type", "text");
                    put (block, "text", part["text"]);
                    if (part.hasProperty ("thoughtSignature"))
                        put (block, "thoughtSignature", part["thoughtSignature"]);
                    content.add (block);
                }
            }

        if (content.isEmpty())
        {
            error = finish.isNotEmpty() && finish != "STOP" && finish != "MAX_TOKENS"
                      ? "Gemini stopped without an answer (" + finish + "). Try rephrasing the request."
                      : finish == "MAX_TOKENS" ? juce::String ("Gemini ran out of output space before answering. Try a shorter request.")
                                                : juce::String ("Gemini gave an empty answer. Try again.");
            return {};
        }

        auto usage = object();
        put (usage, "input_tokens", (int) response["usageMetadata"]["promptTokenCount"]);
        put (usage, "output_tokens", (int) response["usageMetadata"]["candidatesTokenCount"] + (int) response["usageMetadata"]["thoughtsTokenCount"]);

        auto result = object();
        put (result, "content", content);
        put (result, "stop_reason", hasToolUse ? "tool_use" : finish == "MAX_TOKENS" ? "max_tokens" : "end_turn");
        put (result, "usage", usage);
        return result;
    }

    juce::String withKey (const juce::String& url, const juce::String& key)
    {
        return url + (url.contains ("?") ? "&key=" : "?key=") + juce::URL::addEscapeChars (key.trim(), true);
    }

    juce::String errorMessage (int httpStatus, const juce::String& body)
    {
        const auto parsed = juce::JSON::parse (body);
        const auto message = parsed["error"]["message"].toString();
        const auto detail = message.isNotEmpty() ? message : shortened (body, 300);

        if (httpStatus == 401 || (httpStatus == 400 && (message.containsIgnoreCase ("API key") || body.contains ("API_KEY_INVALID"))))
            return "The Gemini API key was not accepted. Check that you copied all of it (it starts with AIza), or create a new one with the KEY button.";
        if (httpStatus == 403)
            return "Google refused the request (403). The model may not be available for your key (pick another in the MODEL box), "
                   "or the key is restricted, deleted, or not allowed in your country: " + detail;
        if (httpStatus == 404)
            return "This model does not exist for your key (404). Pick another model in the MODEL box (you can also type a name).";
        if (httpStatus == 429)
            return "The free quota is used up for now (429). The free tier has limits per minute and per day: wait a while and try again, "
                   "or check your limits in Google AI Studio. " + detail;
        if (httpStatus >= 500)
            return "Google's service is busy or down (" + juce::String (httpStatus) + "). Try again in a moment.";
        return "Gemini answered with an error" + (httpStatus > 0 ? " (" + juce::String (httpStatus) + ")" : juce::String()) + ": " + detail;
    }

    juce::StringArray fetchModels (const juce::String& key)
    {
        // Googles referens visar nyckeln som ?key= (headern är obekräftad för just models.list); pageSize 1000 = max, och nextPageToken följs.
        const juce::String base = Settings::get ("geminiModelsEndpoint", "https://generativelanguage.googleapis.com/v1beta/models"); // (bara för tester)
        juce::StringArray list;
        juce::String pageToken;
        for (int page = 0; page < 10; ++page)
        {
            auto url = juce::URL (base).withParameter ("pageSize", "1000").withParameter ("key", key.trim());
            if (pageToken.isNotEmpty())
                url = url.withParameter ("pageToken", pageToken);
            juce::WebInputStream stream (url, false);
            stream.withConnectionTimeout (15000).withNumRedirectsToFollow (2);
            if (! stream.connect (nullptr) || stream.isError() || stream.getStatusCode() >= 400)
                break;
            const auto response = juce::JSON::parse (stream.readEntireStreamAsString());
            list.addArray (parseModelList (response));
            pageToken = response["nextPageToken"].toString();
            if (pageToken.isEmpty())
                break;
        }
        list.removeDuplicates (false);
        if (! list.isEmpty())
            setFetchedModels (list);
        return list;
    }

    // Kan modellen function calling? Ett minimalt anrop med en dummy-funktionsdeklaration och ett uppdrag att anropa den.
    // Resultat: 1 = ja, 0 = nej (modellen svarar men anropar inte, eller avvisar verktyg), -1 = vet ej (nät/kvot: prova senare).
    int probeTools (const juce::String& key, const juce::String& model)
    {
        auto part = object();
        put (part, "text", "Call the function ping with value 1.");
        auto entry = object();
        put (entry, "role", "user");
        put (entry, "parts", juce::Array<var> { part });
        auto property = object();
        put (property, "type", "integer");
        auto properties = object();
        put (properties, "value", property);
        auto schema = object();
        put (schema, "type", "object");
        put (schema, "properties", properties);
        auto declaration = object();
        put (declaration, "name", "ping");
        put (declaration, "description", "Test function.");
        put (declaration, "parameters", schema);
        auto tool = object();
        put (tool, "functionDeclarations", juce::Array<var> { declaration });
        auto config = object();
        put (config, "maxOutputTokens", 256);
        auto body = object();
        put (body, "contents", juce::Array<var> { entry });
        put (body, "tools", juce::Array<var> { tool });
        put (body, "generationConfig", config);

        juce::URL url (withKey (Settings::get ("geminiEndpoint", endpoint (model)), key)); // (geminiEndpoint: bara för tester)
        url = url.withPOSTData (juce::JSON::toString (body, true));
        juce::WebInputStream stream (url, true);
        stream.withExtraHeaders ("content-type: application/json\r\n")
              .withConnectionTimeout (20000).withNumRedirectsToFollow (2).withCustomRequestCommand ("POST");
        if (! stream.connect (nullptr) || stream.isError())
            return -1;
        const auto status = stream.getStatusCode();
        const auto text = stream.readEntireStreamAsString();
        if (status == 429 || status >= 500 || status == 401)
            return -1;
        if (status >= 400)
            return 0;
        return text.contains ("\"functionCall\"") ? 1 : 0;
    }

    KeyTest testKey (const juce::String& key)
    {
        KeyTest result;
        if (key.trim().isEmpty())
        {
            result.message = "Paste the key first.";
            return result;
        }

        auto part = object();
        put (part, "text", "Reply with the single word OK.");
        auto entry = object();
        put (entry, "role", "user");
        put (entry, "parts", juce::Array<var> { part });
        auto config = object();
        put (config, "maxOutputTokens", 32);
        auto body = object();
        put (body, "contents", juce::Array<var> { entry });
        put (body, "generationConfig", config);

        juce::URL url (withKey (Settings::get ("geminiEndpoint", endpoint (Settings::geminiModel())), key)); // (geminiEndpoint: bara för tester)
        url = url.withPOSTData (juce::JSON::toString (body, true));
        juce::WebInputStream stream (url, true);
        stream.withExtraHeaders ("content-type: application/json\r\n")
              .withConnectionTimeout (20000).withNumRedirectsToFollow (2).withCustomRequestCommand ("POST");

        const auto connected = stream.connect (nullptr);
        const auto status = stream.getStatusCode();
        if (! connected || stream.isError())
        {
            result.message = "Could not reach Google (no internet, a firewall, or a proxy?).";
            return result;
        }
        const auto text = stream.readEntireStreamAsString();
        if (status >= 400)
        {
            result.message = errorMessage (status, text);
            return result;
        }
        if (! juce::JSON::parse (text).isObject())
        {
            result.message = "Unexpected answer from Google: " + shortened (text, 200);
            return result;
        }
        result.ok = true;
        result.message = "The key works.";
        return result;
    }
}
