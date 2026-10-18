// `tool freesound`: hämtar CC0-ljud från Freesound till biblioteket (fas A i docs/content-plan.md).
//
// BARA VERKTYGET: det finns inget Freesound i pluginet och ingen nyckel i SETTINGS (Tobias beslut). Den här filen ingår bara i KickbaseMcp,
// aldrig i VST3-filerna. Nyckeln och hemligheten läses ur miljövariabler (som går före) och en gitignorerad .env-local i arbetsmappen.
// Token (som gäller i 24 h) sparas krypterad (DPAPI) i inställningsfilen, aldrig i repot. Inget som är hemligt skrivs någonsin ut.
//
//   tool freesound auth                      skriver adressen du öppnar i webbläsaren för engångsgodkännandet
//   tool freesound code <kod>                byter koden mot en token (klistra in koden, eller hela adressen du skickades till)
//   tool freesound status                    visar om det finns en token (inga värden)
//   tool freesound search <fråga...> [--max=N]
//   tool freesound fetch <id ...> <kategori> [--package=namn] [--library=mapp]
//   tillägg för alla: --env=<fil> (annan .env-local)
//
// Freesound API v2: sökning med "Authorization: Token <API-nyckel>", originalfiler kräver OAuth2 ("Authorization: Bearer <access_token>").
// Bara ljud med licensen Creative Commons 0 hämtas: licensen kontrolleras per ljud (fältet license), inte per sökning.
// Gränserna (60 anrop per minut, 2000 per dygn) respekteras: minst 1,1 s mellan anrop, och vid 429 väntar verktyget och försöker igen.

#include "../Source/Core/LibraryImport.h"
#include "../Source/Core/Settings.h"
#include "Scenarios.h"
#include <map>

#if JUCE_WINDOWS
 #include <stdlib.h>
#endif

namespace
{
    constexpr const char* defaultBase = "https://freesound.org/apiv2";

    struct Credentials
    {
        juce::String clientId, secret, redirect;
    };

    // KEY=värde-rader (kommentarer med #, valfria citattecken). Läser bara filen: inget skrivs.
    juce::StringPairArray readEnvFile (const juce::File& file)
    {
        juce::StringPairArray values;
        juce::StringArray lines;
        file.readLines (lines);
        for (auto line : lines)
        {
            line = line.trim();
            if (line.isEmpty() || line.startsWithChar ('#'))
                continue;
            if (line.startsWith ("export "))
                line = line.fromFirstOccurrenceOf (" ", false, false).trim();
            const auto equals = line.indexOfChar ('=');
            if (equals <= 0)
                continue;
            auto value = line.substring (equals + 1).trim();
            if (value.length() >= 2 && ((value.startsWithChar ('"') && value.endsWithChar ('"')) || (value.startsWithChar ('\'') && value.endsWithChar ('\''))))
                value = value.substring (1, value.length() - 1);
            values.set (line.substring (0, equals).trim(), value);
        }
        return values;
    }

    // Miljövariabeln först (båda namnen), sedan filen.
    juce::String lookup (const juce::StringPairArray& file, std::initializer_list<const char*> names)
    {
        for (const auto* name : names)
            if (const auto value = juce::SystemStats::getEnvironmentVariable (name, {}); value.isNotEmpty())
                return value;
        for (const auto* name : names)
            if (file.containsKey (name) && file[name].isNotEmpty())
                return file[name];
        return {};
    }

    Credentials loadCredentials (const juce::File& envFile)
    {
        const auto file = envFile.existsAsFile() ? readEnvFile (envFile) : juce::StringPairArray();
        Credentials credentials;
        credentials.clientId = lookup (file, { "KICKBASE_FREESOUND_ID", "FREESOUND_CLIENT_ID" });
        credentials.secret = lookup (file, { "KICKBASE_FREESOUND_SECRET", "FREESOUND_CLIENT_SECRET_APIKEY" });
        credentials.redirect = lookup (file, { "KICKBASE_FREESOUND_REDIRECT", "FREESOUND_OATH_REDIRECT" });
        return credentials;
    }

    juce::String escape (const juce::String& text) { return juce::URL::addEscapeChars (text, true); }

    bool isCC0 (const juce::String& license)
    {
        return license.containsIgnoreCase ("publicdomain/zero") || license.equalsIgnoreCase ("Creative Commons 0");
    }

    struct Response
    {
        bool connected = false;
        int status = 0;
        juce::MemoryBlock body;
        juce::StringPairArray headers;
        juce::String text() const { return juce::String::fromUTF8 ((const char*) body.getData(), (int) body.getSize()); }
    };

    using Log = std::function<void (const juce::String&)>;

    // Allt som kan vara hemligt: tas bort ur varje rad som skrivs ut (också i felmeddelanden och svar från servern).
    class Redactor
    {
    public:
        void add (const juce::String& secret) { if (secret.length() >= 4 && ! secrets.contains (secret)) secrets.add (secret); }
        juce::String operator() (juce::String text) const
        {
            for (const auto& secret : secrets)
                text = text.replace (secret, "***");
            return text;
        }
    private:
        juce::StringArray secrets;
    };

    class Client
    {
    public:
        Client (Credentials credentialsIn, Log logIn) : credentials (std::move (credentialsIn)), log (std::move (logIn))
        {
            base = Settings::get ("freesoundBase", defaultBase);        // en annan adress bara för testerna (mock-server)
            spacingMs = Settings::get ("freesoundSpacingMs", "1100").getIntValue();
            retryCapMs = Settings::get ("freesoundRetryCapMs", "120000").getIntValue();
            redactor.add (credentials.secret);
            redactor.add (accessToken());
            redactor.add (refreshToken());
        }

        void say (const juce::String& line) const { log (redactor (line)); }
        void protectFromOutput (const juce::String& secret) { redactor.add (secret); }

        // ---- token i inställningarna (krypterade)
        juce::String accessToken() const { const auto stored = Settings::get ("freesoundAccess"); return stored.isEmpty() ? juce::String() : Settings::unprotect (stored); }
        juce::String refreshToken() const { const auto stored = Settings::get ("freesoundRefresh"); return stored.isEmpty() ? juce::String() : Settings::unprotect (stored); }
        juce::int64 expiry() const { return Settings::get ("freesoundExpiry", "0").getLargeIntValue(); }

        void storeTokens (const juce::var& json)
        {
            const auto access = json["access_token"].toString();
            const auto refresh = json["refresh_token"].toString();
            const auto seconds = json.hasProperty ("expires_in") ? (juce::int64) json["expires_in"] : (juce::int64) 86400;
            redactor.add (access);
            redactor.add (refresh);
            Settings::set ("freesoundAccess", Settings::protect (access));
            if (refresh.isNotEmpty())
                Settings::set ("freesoundRefresh", Settings::protect (refresh));
            Settings::set ("freesoundExpiry", juce::String (juce::Time::currentTimeMillis() + seconds * 1000));
        }

        juce::String authorizeUrl() const
        {
            return base + "/oauth2/authorize/?client_id=" + escape (credentials.clientId) + "&response_type=code&state=kickbase"
                   + (credentials.redirect.isNotEmpty() ? "&redirect_uri=" + escape (credentials.redirect) : juce::String());
        }

        // Byter koden mot en token. Samma redirect_uri som vid godkännandet.
        bool exchangeCode (const juce::String& code, juce::String& error)
        {
            redactor.add (code);
            auto form = "client_id=" + escape (credentials.clientId) + "&client_secret=" + escape (credentials.secret) + "&grant_type=authorization_code&code=" + escape (code);
            if (credentials.redirect.isNotEmpty())
                form << "&redirect_uri=" << escape (credentials.redirect);
            return tokenRequest (form, error);
        }

        bool refresh (juce::String& error)
        {
            const auto refreshText = refreshToken();
            if (refreshText.isEmpty())
            {
                error = "no refresh token: run `tool freesound auth` and `tool freesound code <code>` again";
                return false;
            }
            const auto form = "client_id=" + escape (credentials.clientId) + "&client_secret=" + escape (credentials.secret) + "&grant_type=refresh_token&refresh_token=" + escape (refreshText);
            return tokenRequest (form, error);
        }

        // GET med API-nyckeln (sökning, uppgifter om ett ljud).
        Response getWithKey (const juce::String& path) { return send ("GET", path, "Authorization: Token " + credentials.secret + "\r\n", {}); }

        // GET med access-token (originalfilen). Är den på väg att gå ut, eller avvisas den (401), förnyas den en gång.
        Response getWithBearer (const juce::String& path, juce::String& error)
        {
            if (accessToken().isEmpty())
            {
                error = "no access token: run `tool freesound auth`, open the address, then `tool freesound code <code>`";
                return {};
            }
            if (expiry() - juce::Time::currentTimeMillis() < 60000 && ! refresh (error))
                return {};
            auto response = send ("GET", path, "Authorization: Bearer " + accessToken() + "\r\n", {});
            if (response.status == 401)
            {
                if (! refresh (error))
                    return {};
                response = send ("GET", path, "Authorization: Bearer " + accessToken() + "\r\n", {});
            }
            return response;
        }

        const Credentials credentials;
        juce::String base;

    private:
        bool tokenRequest (const juce::String& form, juce::String& error)
        {
            const auto response = send ("POST", "/oauth2/access_token/", "Content-Type: application/x-www-form-urlencoded\r\n", form);
            if (! response.connected)
            {
                error = "could not reach Freesound";
                return false;
            }
            const auto json = juce::JSON::parse (response.text());
            if (response.status != 200 || json["access_token"].toString().isEmpty())
            {
                const auto reason = json["error_description"].toString().isNotEmpty() ? json["error_description"].toString() : json["error"].toString();
                error = "Freesound did not accept the code or token (" + juce::String (response.status) + (reason.isNotEmpty() ? ": " + reason : juce::String()) + ")";
                return false;
            }
            storeTokens (json);
            return true;
        }

        Response once (const juce::String& method, const juce::String& path, const juce::String& headers, const juce::String& body)
        {
            Response response;
            juce::URL url (base + path);
            const auto post = method == "POST";
            if (post)
                url = url.withPOSTData (body);
            juce::WebInputStream stream (url, post);
            stream.withExtraHeaders (headers).withConnectionTimeout (30000).withNumRedirectsToFollow (3);
            if (! stream.connect (nullptr))
                return response;
            response.connected = true;
            response.status = stream.getStatusCode();
            response.headers = stream.getResponseHeaders();
            stream.readIntoMemoryBlock (response.body, 120 * 1024 * 1024);
            return response;
        }

        // Ett anrop med avstånd till det förra, och väntan + nytt försök vid 429 (högst tre gånger).
        Response send (const juce::String& method, const juce::String& path, const juce::String& headers, const juce::String& body)
        {
            for (int attempt = 0;; ++attempt)
            {
                const auto since = (int) (juce::Time::getMillisecondCounter() - lastRequest);
                if (lastRequest != 0 && since < spacingMs)
                    juce::Thread::sleep (spacingMs - since);
                auto response = once (method, path, headers, body);
                lastRequest = juce::Time::getMillisecondCounter();
                if (response.status != 429 || attempt >= 3)
                    return response;
                const auto retryAfter = response.headers.getValue ("Retry-After", "60").getIntValue();
                const auto waitMs = juce::jlimit (0, retryCapMs, retryAfter * 1000);
                say ("rate limit (429): waiting " + juce::String (waitMs / 1000.0, 1) + " s, then trying again");
                juce::Thread::sleep (waitMs);
            }
        }

        Log log;
        Redactor redactor;
        int spacingMs = 1100, retryCapMs = 120000;
        juce::uint32 lastRequest = 0;
    };

    juce::String shorten (const juce::String& text, int length) { return text.length() > length ? text.substring (0, length - 3) + "..." : text; }

    juce::String formatTags (const juce::var& tags, int count)
    {
        juce::StringArray list;
        if (const auto* array = tags.getArray())
            for (const auto& tag : *array)
                if (list.size() < count)
                    list.add (tag.toString());
        return list.joinIntoString (" ");
    }
}

// Kör ett kommando. `log` får det som ska skrivas (redan rensat från hemligheter).
int freesoundRun (const juce::StringArray& arguments, const Log& log)
{
    juce::StringArray positional;
    juce::File envFile = juce::File::getCurrentWorkingDirectory().getChildFile (".env-local");
    juce::File libraryRoot = Settings::libraryFolder();
    juce::String packageName;
    int maxResults = 15;
    for (const auto& arg : arguments)
    {
        if (arg.startsWith ("--env="))          envFile = juce::File::getCurrentWorkingDirectory().getChildFile (arg.fromFirstOccurrenceOf ("=", false, false));
        else if (arg.startsWith ("--library=")) libraryRoot = juce::File::getCurrentWorkingDirectory().getChildFile (arg.fromFirstOccurrenceOf ("=", false, false));
        else if (arg.startsWith ("--package=")) packageName = arg.fromFirstOccurrenceOf ("=", false, false);
        else if (arg.startsWith ("--max="))     maxResults = juce::jlimit (1, 150, arg.fromFirstOccurrenceOf ("=", false, false).getIntValue());
        else                                    positional.add (arg);
    }
    const auto command = positional.isEmpty() ? juce::String() : positional[0];
    positional.remove (0);

    Client client (loadCredentials (envFile), log);
    auto fail = [&client] (const juce::String& message) { client.say ("ERROR: " + message); return 1; };

    if (command.isEmpty() || ! juce::StringArray { "auth", "code", "status", "search", "fetch" }.contains (command))
    {
        log ("usage: tool freesound auth | code <code> | status | search <query...> [--max=N] | fetch <id ...> <category> [--package=name] [--library=folder]\n"
             "  credentials: environment variables (KICKBASE_FREESOUND_ID/_SECRET/_REDIRECT or FREESOUND_CLIENT_ID, FREESOUND_CLIENT_SECRET_APIKEY, FREESOUND_OATH_REDIRECT),\n"
             "  otherwise a git-ignored .env-local in the current folder (--env=<file> for another). Only CC0 sounds are fetched.");
        return 1;
    }
    if (client.credentials.clientId.isEmpty() || client.credentials.secret.isEmpty())
        return fail ("no Freesound credentials: set FREESOUND_CLIENT_ID and FREESOUND_CLIENT_SECRET_APIKEY (environment or .env-local in this folder)");

    if (command == "auth")
    {
        if (client.credentials.redirect.isEmpty())
            log ("note: FREESOUND_OATH_REDIRECT is not set; Freesound will use the redirect address set on your API application.");
        log ("Open this address in your browser, log in to Freesound and press Authorize:");
        log (client.authorizeUrl());
        log ("Freesound then sends you to your redirect address with ?code=... at the end. Copy the code and run:  tool freesound code <code>");
        return 0;
    }

    if (command == "code")
    {
        if (positional.isEmpty())
            return fail ("missing the code: tool freesound code <code>");
        auto code = positional[0];
        if (code.contains ("code="))
            code = code.fromFirstOccurrenceOf ("code=", false, false).upToFirstOccurrenceOf ("&", false, false); // hela adressen funkar också
        juce::String error;
        if (! client.exchangeCode (code, error))
            return fail (error);
        log ("The token is stored (encrypted for this Windows user) and lasts about 24 hours; it renews itself with the refresh token.");
        return 0;
    }

    if (command == "status")
    {
        const auto left = client.expiry() - juce::Time::currentTimeMillis();
        log (juce::String ("access token: ") + (client.accessToken().isNotEmpty() ? "stored" : "none")
             + (client.accessToken().isNotEmpty() ? (left > 0 ? ", valid for " + juce::String (left / 60000) + " min" : juce::String (", expired (renews on use)")) : juce::String()));
        log (juce::String ("refresh token: ") + (client.refreshToken().isNotEmpty() ? "stored" : "none"));
        return 0;
    }

    if (command == "search")
    {
        if (positional.isEmpty())
            return fail ("missing the search words: tool freesound search <query...>");
        const auto query = positional.joinIntoString (" ");
        const auto response = client.getWithKey ("/search/text/?query=" + escape (query) + "&filter=" + escape ("license:\"Creative Commons 0\"")
                                                 + "&fields=id,name,tags,duration,username,license&page_size=" + juce::String (maxResults));
        if (! response.connected)
            return fail ("could not reach Freesound");
        const auto json = juce::JSON::parse (response.text());
        if (response.status == 401 || response.status == 403)
            return fail ("Freesound did not accept the API key (" + juce::String (response.status) + ")");
        if (response.status != 200)
            return fail ("Freesound answered " + juce::String (response.status) + ": " + shorten (json["detail"].toString(), 200));
        auto shown = 0;
        if (const auto* results = json["results"].getArray())
            for (const auto& item : *results)
                if (isCC0 (item["license"].toString())) // också här: licensen per ljud
                {
                    log (juce::String (item["id"].toString()).paddedRight (' ', 9) + shorten (item["name"].toString(), 40).paddedRight (' ', 41)
                         + juce::String ((double) item["duration"], 2) + " s  " + item["username"].toString() + "  " + formatTags (item["tags"], 6));
                    ++shown;
                }
        log (juce::String (shown) + " CC0 sounds shown (" + json["count"].toString() + " found)");
        return 0;
    }

    // fetch <id ...> <kategori>
    juce::StringArray ids;
    juce::String category;
    for (const auto& item : positional)
    {
        if (item.containsOnly ("0123456789") && item.isNotEmpty())
            ids.add (item);
        else
            category = item;
    }
    if (ids.isEmpty() || category.isEmpty())
        return fail ("usage: tool freesound fetch <id ...> <category> [--package=name]   (categories: " + Library::categories().joinIntoString (", ") + ")");
    if (! Library::categories().contains (category))
        return fail ("unknown category \"" + category + "\" (" + Library::categories().joinIntoString (", ") + ")");

    auto errors = 0;
    auto imported = 0;
    const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_freesound_" + juce::String (juce::Time::currentTimeMillis()));
    temp.createDirectory();
    for (const auto& id : ids)
    {
        const auto detail = client.getWithKey ("/sounds/" + id + "/?fields=id,name,tags,license,username,type,duration");
        if (! detail.connected)
        {
            client.say ("ERROR: " + id + ": could not reach Freesound");
            ++errors;
            continue;
        }
        const auto info = juce::JSON::parse (detail.text());
        if (detail.status != 200)
        {
            client.say ("ERROR: " + id + ": Freesound answered " + juce::String (detail.status) + " (" + shorten (info["detail"].toString(), 120) + ")");
            ++errors;
            continue;
        }
        if (! isCC0 (info["license"].toString())) // aldrig något annat än CC0
        {
            client.say ("ERROR: " + id + " (" + shorten (info["name"].toString(), 40) + "): the license is not CC0 (" + info["license"].toString() + "), not fetched");
            ++errors;
            continue;
        }

        juce::String error;
        const auto download = client.getWithBearer ("/sounds/" + id + "/download/", error);
        if (! download.connected || download.status != 200 || download.body.getSize() == 0)
        {
            client.say ("ERROR: " + id + ": " + (error.isNotEmpty() ? error : download.connected ? "the download failed (" + juce::String (download.status) + (download.status == 401 || download.status == 403 ? ", the access token was not accepted" : juce::String()) + ")"
                                                                                               : juce::String ("could not reach Freesound")));
            ++errors;
            continue;
        }
        const auto type = info["type"].toString().isNotEmpty() ? info["type"].toString() : juce::String ("wav");
        const auto file = temp.getChildFile (id + "." + type);
        if (! file.replaceWithData (download.body.getData(), download.body.getSize()))
        {
            client.say ("ERROR: " + id + ": could not write the downloaded file");
            ++errors;
            continue;
        }

        Library::ImportOptions options;
        options.category = category;
        options.package = packageName.isNotEmpty() ? packageName : juce::String ("Freesound CC0");
        options.license = "CC0-1.0";
        options.source = "Freesound";
        options.url = "https://freesound.org/";
        const auto name = info["name"].toString().isNotEmpty() ? info["name"].toString() : "Freesound " + id;
        const auto author = info["username"].toString();
        const auto pageUrl = "https://freesound.org/s/" + id + "/";
        juce::StringArray tags;
        if (const auto* array = info["tags"].getArray())
            for (const auto& tag : *array)
                if (tags.size() < 10 && tag.toString().isNotEmpty())
                    tags.add (tag.toString().toLowerCase());
        options.decorateItem = [&] (const juce::File&, Library::Item& item)
        {
            item.name = name;
            item.author = author;
            item.url = pageUrl;
            item.tags = tags;
        };
        const auto result = Library::importFiles ({ file }, options, libraryRoot);
        for (const auto& note : result.notes)
            client.say ("  " + id + ": " + note);
        for (const auto& message : result.errors)
        {
            client.say ("ERROR: " + id + ": " + message);
            ++errors;
        }
        if (result.imported > 0)
        {
            ++imported;
            client.say (id + "  " + name + "  by " + author + "  ->  " + result.packageFolder.getFullPathName());
        }
    }
    temp.deleteRecursively();
    log (juce::String (imported) + " sound(s) fetched, " + juce::String (errors) + " error(s)");
    return errors == 0 && imported > 0 ? 0 : 1;
}

// ===========================================================================================================================================
// freesoundtest: en mock-server på 127.0.0.1 (inget når Freesound). Provar OAuth2-flödet, redirect_uri, sökningen, licenskontrollen, 429, förnyelse av
// token, krypterad lagring, att inga hemligheter skrivs ut och att miljövariabler går före .env-local.
namespace
{
    class MockFreesound : private juce::Thread
    {
    public:
        MockFreesound() : juce::Thread ("mock freesound")
        {
            listener.createListener (0, "127.0.0.1");
            startThread();
        }
        ~MockFreesound() override { stopThread (3000); listener.close(); }
        int port() const { return listener.getBoundPort(); }

        // Det servern sett (för testerna).
        std::atomic<int> tokenRequests { 0 }, refreshRequests { 0 }, searchRequests { 0 }, searchWithRightKey { 0 }, detailRequests { 0 }, downloadRequests { 0 };
        std::atomic<bool> rateLimitNextSearch { false }, revokeNextDownload { false };
        juce::String lastTokenForm, lastSearchPath;
        juce::CriticalSection lock;
        juce::String expectedRedirect;

        static constexpr const char* secret = "TESTSECRETVALUE";
        static constexpr const char* cc0 = "http://creativecommons.org/publicdomain/zero/1.0/";

    private:
        static juce::String formValue (const juce::String& form, const juce::String& key)
        {
            for (const auto& pair : juce::StringArray::fromTokens (form, "&", ""))
                if (pair.upToFirstOccurrenceOf ("=", false, false) == key)
                    return juce::URL::removeEscapeChars (pair.fromFirstOccurrenceOf ("=", false, false).replace ("+", " "));
            return {};
        }

        std::string soundJson (const juce::String& id) const
        {
            const auto license = id == "1002" ? "http://creativecommons.org/licenses/by/4.0/" : cc0;
            return ("{\"id\":" + id + ",\"name\":\"Mock Kick " + id + "\",\"tags\":[\"Kick\",\"Psytrance\"],\"license\":\"" + juce::String (license)
                    + "\",\"username\":\"mockuser\",\"type\":\"wav\",\"duration\":1.0}").toStdString();
        }

        std::string searchJson() const
        {
            const juce::String item1 = "{\"id\":1001,\"name\":\"Mock Kick 1001\",\"tags\":[\"kick\",\"psy\"],\"duration\":1.0,\"username\":\"mockuser\",\"license\":\"" + juce::String (cc0) + "\"}";
            const juce::String rogue = "{\"id\":1002,\"name\":\"Mock Kick 1002\",\"tags\":[\"kick\"],\"duration\":1.0,\"username\":\"other\",\"license\":\"http://creativecommons.org/licenses/by/4.0/\"}";
            return ("{\"count\":2,\"results\":[" + item1 + "," + rogue + "]}").toStdString(); // en som inte är CC0 smyger sig med: klienten ska filtrera bort den
        }

        std::string wavBytes() const
        {
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("kickbase_mock_freesound.wav");
            file.deleteFile();
            {
                std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
                juce::WavAudioFormat wav;
                auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (48000.0).withNumChannels (2).withBitsPerSample (16));
                juce::AudioBuffer<float> audio (2, 48000);
                audio.clear();
                for (int i = 4800; i < audio.getNumSamples(); ++i)
                    for (int c = 0; c < 2; ++c)
                        audio.setSample (c, i, (float) (0.6 * std::cos (juce::MathConstants<double>::twoPi * 55.0 * (i - 4800) / 48000.0) * std::exp (-(i - 4800) / 9000.0)));
                writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
            }
            juce::MemoryBlock data;
            file.loadFileAsData (data);
            file.deleteFile();
            return std::string ((const char*) data.getData(), data.getSize());
        }

        void respond (juce::StreamingSocket& socket, int status, const std::string& body, const juce::String& extraHeaders = {}, const char* type = "application/json")
        {
            const juce::String head = "HTTP/1.1 " + juce::String (status) + (status == 200 ? " OK" : " Error") + "\r\nContent-Type: " + type + "\r\n" + extraHeaders
                                      + "Content-Length: " + juce::String ((juce::int64) body.size()) + "\r\nConnection: close\r\n\r\n";
            const auto text = head.toStdString();
            socket.write (text.data(), (int) text.size());
            if (! body.empty())
                socket.write (body.data(), (int) body.size());
        }

        void handle (juce::StreamingSocket& socket)
        {
            std::string request;
            char buffer[4096];
            size_t headerEnd = std::string::npos;
            while (headerEnd == std::string::npos)
            {
                if (socket.waitUntilReady (true, 3000) != 1)
                    return;
                const auto n = socket.read (buffer, sizeof (buffer), false);
                if (n <= 0)
                    return;
                request.append (buffer, (size_t) n);
                headerEnd = request.find ("\r\n\r\n");
            }
            const juce::String head = juce::String (request.substr (0, headerEnd));
            auto contentLength = 0;
            for (const auto& line : juce::StringArray::fromLines (head))
                if (line.startsWithIgnoreCase ("content-length:"))
                    contentLength = line.fromFirstOccurrenceOf (":", false, false).trim().getIntValue();
            while ((int) (request.size() - headerEnd - 4) < contentLength)
            {
                if (socket.waitUntilReady (true, 3000) != 1)
                    return;
                const auto n = socket.read (buffer, sizeof (buffer), false);
                if (n <= 0)
                    return;
                request.append (buffer, (size_t) n);
            }
            const auto body = juce::String (request.substr (headerEnd + 4));
            const auto requestLine = head.upToFirstOccurrenceOf ("\r\n", false, false);
            const auto method = requestLine.upToFirstOccurrenceOf (" ", false, false);
            const auto target = requestLine.fromFirstOccurrenceOf (" ", false, false).upToLastOccurrenceOf (" ", false, false);
            const auto path = target.upToFirstOccurrenceOf ("?", false, false);
            juce::String authorization;
            for (const auto& line : juce::StringArray::fromLines (head))
                if (line.startsWithIgnoreCase ("authorization:"))
                    authorization = line.fromFirstOccurrenceOf (":", false, false).trim();

            if (method == "POST" && path == "/apiv2/oauth2/access_token/")
            {
                const auto grant = formValue (body, "grant_type");
                {
                    const juce::ScopedLock sl (lock);
                    lastTokenForm = body;
                }
                const auto okClient = formValue (body, "client_id") == "testid" && formValue (body, "client_secret") == secret;
                if (okClient && grant == "authorization_code" && formValue (body, "code") == "CODE123" && formValue (body, "redirect_uri") == expectedRedirect)
                {
                    ++tokenRequests;
                    respond (socket, 200, R"({"access_token":"AT1","scope":"read write","expires_in":86400,"refresh_token":"RT1"})");
                }
                else if (okClient && grant == "refresh_token" && formValue (body, "refresh_token") == "RT1")
                {
                    ++refreshRequests;
                    respond (socket, 200, R"({"access_token":"AT2","scope":"read write","expires_in":86400,"refresh_token":"RT2"})");
                }
                else
                    respond (socket, 400, R"({"error":"invalid_grant","error_description":"Invalid code"})");
                return;
            }
            if (method == "GET" && path == "/apiv2/search/text/")
            {
                ++searchRequests;
                {
                    const juce::ScopedLock sl (lock);
                    lastSearchPath = target;
                }
                if (rateLimitNextSearch.exchange (false))
                {
                    respond (socket, 429, R"({"detail":"Request was throttled."})", "Retry-After: 0\r\n");
                    return;
                }
                if (authorization != "Token " + juce::String (secret))
                {
                    respond (socket, 401, R"({"detail":"Invalid token."})");
                    return;
                }
                ++searchWithRightKey;
                respond (socket, 200, searchJson());
                return;
            }
            if (method == "GET" && path.startsWith ("/apiv2/sounds/"))
            {
                const auto id = path.fromFirstOccurrenceOf ("/apiv2/sounds/", false, false).upToFirstOccurrenceOf ("/", false, false);
                const auto isDownload = path.endsWith ("/download/");
                if (id == "9999")
                {
                    respond (socket, 404, R"({"detail":"Not found."})");
                    return;
                }
                if (! isDownload)
                {
                    ++detailRequests;
                    if (authorization != "Token " + juce::String (secret))
                        respond (socket, 401, R"({"detail":"Invalid token."})");
                    else
                        respond (socket, 200, soundJson (id));
                    return;
                }
                ++downloadRequests;
                if (revokeNextDownload.exchange (false) || ! (authorization == "Bearer AT1" || authorization == "Bearer AT2"))
                {
                    respond (socket, 401, R"({"detail":"Authentication credentials were not provided."})");
                    return;
                }
                if (id == "1003")
                {
                    respond (socket, 404, R"({"detail":"File missing."})");
                    return;
                }
                respond (socket, 200, wavBytes(), "Content-Disposition: attachment; filename=\"" + id + ".wav\"\r\n", "audio/wav");
                return;
            }
            respond (socket, 404, R"({"detail":"Unknown route."})");
        }

        void run() override
        {
            while (! threadShouldExit())
            {
                if (listener.waitUntilReady (true, 100) != 1)
                    continue;
                std::unique_ptr<juce::StreamingSocket> connection (listener.waitForNextConnection());
                if (connection != nullptr)
                    handle (*connection);
            }
        }

        juce::StreamingSocket listener;
    };

    void setEnvironment (const char* name, const char* value)
    {
       #if JUCE_WINDOWS
        _putenv_s (name, value);
       #else
        juce::ignoreUnused (name, value);
       #endif
    }
}

int freesoundTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    Scenarios::TemporaryUserFolders folders;
    auto failed = false;
    auto check = [&failed] (bool ok, const juce::String& what) { std::printf ("%s: %s\n", ok ? "ok" : "FEL", what.toRawUTF8()); failed |= ! ok; };

    // Kan det finnas riktiga nycklar i miljön? Rensa dem för testet (processen återställs när den slutar).
    for (const auto* name : { "KICKBASE_FREESOUND_ID", "KICKBASE_FREESOUND_SECRET", "KICKBASE_FREESOUND_REDIRECT", "FREESOUND_CLIENT_ID", "FREESOUND_CLIENT_SECRET_APIKEY", "FREESOUND_OATH_REDIRECT" })
        setEnvironment (name, "");

    MockFreesound server;
    const auto base = "http://127.0.0.1:" + juce::String (server.port()) + "/apiv2";
    Settings::set ("freesoundBase", base);
    Settings::set ("freesoundSpacingMs", "0");
    server.expectedRedirect = "http://localhost:8080/cb?x=1";

    const auto envFile = folders.root.getChildFile ("env-test");
    envFile.replaceWithText ("# testfil\nFREESOUND_CLIENT_ID=fileid\nFREESOUND_CLIENT_SECRET_APIKEY=\"" + juce::String (MockFreesound::secret) + "\"\nFREESOUND_OATH_REDIRECT=http://localhost:8080/cb?x=1\n"
                             "FREESOUND_URL=https://example.com/ignored\n");
    const auto library = folders.root.getChildFile ("FsLibrary");
    juce::StringArray output;
    const Log log = [&output] (const juce::String& line) { output.add (line); };
    auto run = [&] (juce::StringArray args)
    {
        output.clear();
        args.add ("--env=" + envFile.getFullPathName());
        args.add ("--library=" + library.getFullPathName());
        return freesoundRun (args, log);
    };
    auto printed = [&output] { return output.joinIntoString ("\n"); };
    auto leaked = [&] (const juce::String& text)
    {
        for (const auto* secret : { "TESTSECRETVALUE", "AT1", "AT2", "RT1", "RT2", "CODE123" })
            if (text.contains (secret))
                return true;
        return false;
    };
    juce::String allOutput;
    auto remember = [&] { allOutput << printed() << "\n"; };

    // 1. Nycklarna: .env-local när miljön är tom, miljövariabler före filen, båda namnen.
    {
        const auto code = run ({ "auth" });
        const auto text = printed();
        remember();
        check (code == 0 && text.contains ("/oauth2/authorize/?client_id=fileid&response_type=code"), "utan miljövariabler används .env-local (klient-id ur filen)");
        check (text.contains ("redirect_uri=" + juce::URL::addEscapeChars (server.expectedRedirect, true)), "authorize-adressen har redirect_uri ur FREESOUND_OATH_REDIRECT");
        setEnvironment ("FREESOUND_CLIENT_ID", "testid");
        run ({ "auth" });
        check (printed().contains ("client_id=testid&"), "miljövariabeln går före .env-local");
        setEnvironment ("FREESOUND_CLIENT_ID", "");
        setEnvironment ("KICKBASE_FREESOUND_ID", "testid"); // planens namn fungerar också
        run ({ "auth" });
        check (printed().contains ("client_id=testid&"), "KICKBASE_FREESOUND_ID fungerar också");
    }
    // Resten av testet använder id "testid" ur miljön (som servern väntar sig) och hemligheten ur filen.

    // 2. Utan token: sökning går med API-nyckeln, men en nedladdning kräver godkännandet.
    {
        check (run ({ "status" }) == 0 && printed().contains ("access token: none"), "status: ingen token än");
        check (run ({ "fetch", "1001", "Kicks" }) == 1 && printed().contains ("no access token"), "fetch utan token ger ett tydligt fel");
        remember();
    }

    // 3. Koden byts mot en token (med samma redirect_uri), den sparas krypterad, och inget hemligt skrivs ut.
    {
        check (run ({ "code", "WRONG" }) == 1 && printed().contains ("did not accept"), "en felaktig kod avvisas med ett begripligt fel");
        remember();
        const auto code = run ({ "code", "http://localhost:8080/cb?x=1&code=CODE123&state=kickbase" }); // hela adressen
        remember();
        check (code == 0 && server.tokenRequests == 1, "koden byts mot en token (en hel adress duger också)");
        juce::String form;
        {
            const juce::ScopedLock sl (server.lock);
            form = server.lastTokenForm;
        }
        check (form.contains ("grant_type=authorization_code") && form.contains ("redirect_uri=" + juce::URL::addEscapeChars (server.expectedRedirect, true)),
               "tokenbytet skickar samma redirect_uri");
        const auto settingsText = Settings::file().loadFileAsString();
        check (settingsText.contains ("freesoundAccess=\"dpapi:") || settingsText.contains ("freesoundAccess=\"plain:"), "access-token sparas som dpapi:-text i inställningsfilen");
        check (! settingsText.contains ("AT1") && ! settingsText.contains ("RT1") && ! settingsText.contains (MockFreesound::secret) && ! settingsText.contains ("CODE123"),
               "varken token, hemlighet eller kod ligger i klartext i inställningsfilen");
        check (Settings::unprotect (Settings::get ("freesoundAccess")) == "AT1" && Settings::unprotect (Settings::get ("freesoundRefresh")) == "RT1", "tokens går att läsa tillbaka (krypterad rundtur)");
        check (run ({ "status" }) == 0 && printed().contains ("valid for") && ! leaked (printed()), "status visar att token finns utan att visa den");
    }

    // 4. Sökning: API-nyckeln i huvudet, bara CC0, 429 väntas ut.
    {
        server.rateLimitNextSearch = true;
        const auto code = run ({ "search", "psytrance", "kick" });
        remember();
        const auto text = printed();
        check (code == 0 && text.contains ("1001") && ! text.contains ("1002"), "sökningen visar CC0-ljudet och döljer ett som inte är CC0, även om servern skickade det");
        check (text.contains ("rate limit (429)") && server.searchRequests == 2, "vid 429 väntar verktyget och försöker igen (" + juce::String (server.searchRequests.load()) + " anrop)");
        check (server.searchWithRightKey == 1, "API-nyckeln skickas som Authorization: Token ...");
        juce::String path;
        {
            const juce::ScopedLock sl (server.lock);
            path = server.lastSearchPath;
        }
        check (path.contains ("query=psytrance") && (path.contains ("%20kick") || path.contains ("+kick")) && path.contains ("filter=license%3A") && path.contains ("Creative") && path.contains ("Commons"),
               "frågan och licensfiltret (license:\"Creative Commons 0\") skickas (" + path + ")");
    }

    // 5. Hämtning: licensen kontrolleras per ljud, filen importeras med källa, upphovsperson och sida, token förnyas när den gått ut eller avvisas.
    {
        const auto code = run ({ "fetch", "1001", "1002", "1003", "9999", "Kicks", "--package=Freesound test" });
        remember();
        const auto text = printed();
        check (code == 1 && text.contains ("1 sound(s) fetched, 3 error(s)"), "ett ljud hämtas, tre fel: " + text.fromLastOccurrenceOf ("\n", false, false));
        check (text.contains ("1002") && text.contains ("not CC0") && server.downloadRequests == 2, "ett ljud som inte är CC0 hämtas aldrig (bara 1001 och 1003 nådde nedladdningen)");
        check (text.contains ("1003") && text.contains ("download failed"), "en saknad fil rapporteras utan att stoppa de andra");
        check (text.contains ("9999") && text.contains ("404"), "ett okänt id rapporteras");
        const auto result = Library::parseFile (library.getChildFile ("Kicks/Freesound test").getChildFile (Library::manifestName), true);
        check (result.ok() && result.manifest.items.size() == 1 && result.manifest.license == "CC0-1.0" && result.manifest.redistributable && result.manifest.source == "Freesound",
               "manifestet är giltigt: CC0, redistributable, källa Freesound" + (result.ok() ? juce::String() : " (" + result.errors.joinIntoString (" / ") + ")"));
        if (! result.manifest.items.empty())
        {
            const auto& item = result.manifest.items[0];
            check (item.name == "Mock Kick 1001" && item.author == "mockuser" && item.url == "https://freesound.org/s/1001/" && item.tags.contains ("psytrance"),
                   "posten har namn, upphovsperson, sidan https://freesound.org/s/1001/ och taggar");
        }
        const auto wav = library.getChildFile ("Kicks/Freesound test").findChildFiles (juce::File::findFiles, false, "*.wav");
        check (wav.size() == 1, "en WAV i paketet");

        // En token som gått ut förnyas med refresh-token före nedladdningen.
        Settings::set ("freesoundExpiry", juce::String (juce::Time::currentTimeMillis() - 1000));
        const auto before = server.refreshRequests.load();
        const auto again = run ({ "fetch", "1001", "Kicks", "--package=Freesound refresh" });
        remember();
        check (again == 0 && server.refreshRequests == before + 1 && Settings::unprotect (Settings::get ("freesoundAccess")) == "AT2" && Settings::unprotect (Settings::get ("freesoundRefresh")) == "RT2",
               "en utgången token förnyas (AT2/RT2 sparas krypterade)");

        // En token som servern avvisar (401) förnyas en gång och nedladdningen provas igen.
        server.revokeNextDownload = true;
        Settings::set ("freesoundAccess", Settings::protect ("AT1"));
        Settings::set ("freesoundRefresh", Settings::protect ("RT1"));
        Settings::set ("freesoundExpiry", juce::String (juce::Time::currentTimeMillis() + 3600000));
        const auto beforeRefresh = server.refreshRequests.load();
        const auto third = run ({ "fetch", "1001", "Kicks", "--package=Freesound revoked" });
        remember();
        check (third == 0 && server.refreshRequests == beforeRefresh + 1, "en avvisad token (401) förnyas en gång och nedladdningen lyckas");
    }

    // 6. Felmeddelanden utan nycklar eller med fel kategori, och att inget hemligt skrivits ut någonstans.
    {
        run ({ "fetch", "1001", "Nope" });
        check (printed().contains ("unknown category"), "en okänd kategori vägras");
        remember();
        setEnvironment ("KICKBASE_FREESOUND_ID", "");
        const auto emptyEnv = folders.root.getChildFile ("empty-env");
        emptyEnv.replaceWithText ("");
        output.clear();
        const auto code = freesoundRun ({ "search", "x", "--env=" + emptyEnv.getFullPathName() }, log);
        check (code == 1 && printed().contains ("no Freesound credentials"), "utan nycklar: ett tydligt fel");
        check (! leaked (allOutput), "ingen hemlighet, token eller kod skrevs ut i något av kommandona");
    }

    Settings::set ("freesoundBase", {});
    Settings::set ("freesoundSpacingMs", {});
    std::printf (failed ? "Freesound: FEL\n" : "Freesound-verktyget gick igenom\n");
    return failed ? 1 : 0;
}

// Verktygets ingång: skriver till konsolen.
int freesoundMain (const juce::StringArray& arguments)
{
    juce::ScopedJuceInitialiser_GUI gui;
    return freesoundRun (arguments, [] (const juce::String& line) { std::printf ("%s\n", line.toRawUTF8()); });
}
