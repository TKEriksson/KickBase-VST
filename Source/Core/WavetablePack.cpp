#include "WavetablePack.h"
#include "Settings.h"
#include <juce_events/juce_events.h>

namespace
{
    constexpr const char* archivePrefix = "AKWF-FREE-master/AKWF/";
    constexpr const char* licenseEntry = "AKWF-FREE-master/LICENSE";
}

WavetablePack::WavetablePack() : juce::Thread ("Kickbase wavetable pack") {}

WavetablePack::~WavetablePack()
{
    onFinished = nullptr;
    stopThread (8000);
}

juce::File WavetablePack::packFolder()
{
    return Settings::wavetablesFolder().getChildFile ("AKWF");
}

int WavetablePack::installedCount()
{
    return packFolder().isDirectory() ? packFolder().findChildFiles (juce::File::findFiles, true, "*.wav").size() : 0;
}

juce::String WavetablePack::getStatus() const
{
    const juce::ScopedLock lock (statusLock);
    return status;
}

void WavetablePack::setStatus (const juce::String& text)
{
    const juce::ScopedLock lock (statusLock);
    status = text;
}

void WavetablePack::install()
{
    if (busy.exchange (true))
        return;
    progress = 0.0f;
    setStatus ("Starting ...");
    startThread (juce::Thread::Priority::low);
}

void WavetablePack::cancel()
{
    signalThreadShouldExit();
}

void WavetablePack::finish (const juce::String& text)
{
    setStatus (text);
    busy = false;
    if (onFinished)
        juce::MessageManager::callAsync ([callback = onFinished] { if (callback) callback(); });
}

int WavetablePack::extract (const juce::File& archive, const std::function<bool (float, int)>& progress)
{
    juce::ZipFile zip (archive);
    const auto target = packFolder();
    const auto library = Settings::wavetablesFolder();
    target.createDirectory();

    auto written = 0;
    const auto entries = zip.getNumEntries();
    for (int i = 0; i < entries; ++i)
    {
        const auto* entry = zip.getEntry (i);
        if (entry == nullptr)
            continue;

        const auto name = entry->filename.replaceCharacter ('\\', '/');
        juce::File destination;
        if (name.startsWith (archivePrefix) && ! name.endsWith ("/") && name.endsWithIgnoreCase (".wav"))
            destination = target.getChildFile (name.substring ((int) std::strlen (archivePrefix)));
        else if (name == licenseEntry)
            destination = library.getChildFile ("LICENSE-CC0.md");
        else
            continue;

        std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
        if (in == nullptr)
            continue;
        destination.getParentDirectory().createDirectory();
        destination.deleteFile();
        juce::FileOutputStream out (destination);
        if (out.openedOk())
        {
            out.writeFromInputStream (*in, -1);
            if (destination.hasFileExtension ("wav"))
                ++written;
        }

        if (progress && i % 50 == 0 && ! progress ((float) i / (float) std::max (1, entries), written))
            return -1;
    }
    return written;
}

void WavetablePack::run()
{
    // 1. Hämta arkivet till en temporärfil.
    const auto archive = juce::File::createTempFile (".zip");
    {
        juce::URL url (sourceUrl);
        std::unique_ptr<juce::InputStream> stream (url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                              .withConnectionTimeoutMs (15000)
                                                                              .withNumRedirectsToFollow (6)));
        if (stream == nullptr)
        {
            finish ("Could not reach the download (no internet, or a firewall?). Try again, or run install-wavetables.ps1.");
            return;
        }

        juce::FileOutputStream out (archive);
        if (! out.openedOk())
        {
            finish ("Could not write a temporary file.");
            return;
        }

        const auto total = stream->getTotalLength();
        juce::int64 received = 0;
        juce::HeapBlock<char> chunk (64 * 1024);
        while (! threadShouldExit())
        {
            const auto count = stream->read (chunk, 64 * 1024);
            if (count <= 0)
                break;
            out.write (chunk, (size_t) count);
            received += count;
            progress = total > 0 ? 0.6f * (float) ((double) received / (double) total) : std::min (0.55f, (float) received / 40.0e6f);
            setStatus ("Downloading ... " + juce::String ((double) received / 1.0e6, 1) + " MB");
        }
        out.flush();

        if (threadShouldExit())
        {
            archive.deleteFile();
            finish ("Cancelled.");
            return;
        }
        if (received < 1000000 || (total > 0 && received < total))
        {
            archive.deleteFile();
            finish ("The download was incomplete. Try again.");
            return;
        }
    }

    // 2. Packa upp bara vågformerna (och licensen) till biblioteket.
    const auto written = extract (archive, [this] (float fraction, int count)
    {
        progress = 0.6f + 0.4f * fraction;
        setStatus ("Installing ... " + juce::String (count) + " waveforms");
        return ! threadShouldExit();
    });
    const auto target = packFolder();

    archive.deleteFile();
    if (written < 0)
    {
        finish ("Cancelled.");
        return;
    }
    progress = 1.0f;
    finish (written > 0 ? "Done: " + juce::String (written) + " waveforms installed in " + target.getFullPathName()
                        : "The download did not contain any waveforms.");
}
