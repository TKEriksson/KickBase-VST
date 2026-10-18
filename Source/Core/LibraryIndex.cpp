#include "LibraryIndex.h"

LibraryIndex::LibraryIndex() : juce::Thread ("Kickbase library index"), current (std::make_shared<const std::vector<Entry>>())
{
    startThread (juce::Thread::Priority::background);
}

LibraryIndex::~LibraryIndex()
{
    shutDown();
    cancelPendingUpdate();
}

bool LibraryIndex::shutDown (int timeoutMs)
{
    return stopThread (timeoutMs); // signalerar threadShouldExit och väcker run() (Thread::notify), som avbryter en pågående skanning
}

LibraryIndex::Snapshot LibraryIndex::entries() const
{
    const juce::ScopedLock sl (lock);
    return current;
}

juce::StringArray LibraryIndex::problems() const
{
    const juce::ScopedLock sl (lock);
    return currentProblems;
}

int LibraryIndex::packageCount() const
{
    const juce::ScopedLock sl (lock);
    return currentPackages;
}

juce::File LibraryIndex::folder() const
{
    const juce::ScopedLock sl (lock);
    return currentRoot;
}

void LibraryIndex::rescan (const juce::File& root)
{
    {
        const juce::ScopedLock sl (lock);
        requestedRoot = root;
        requestPending = true;
        requestForced = true;
    }
    notify();
}

void LibraryIndex::refreshIfChanged (const juce::File& root)
{
    {
        const juce::ScopedLock sl (lock);
        requestedRoot = root;
        requestPending = true; // requestForced lämnas som den var: en tidigare beställd rescan förblir tvingad
    }
    notify();
}

int LibraryIndex::scanNow (const juce::File& root)
{
    auto result = scan (root, {});
    const auto count = (int) result.entries->size();
    publish (std::move (result), root);
    return count;
}

bool LibraryIndex::waitUntilIdle (int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    for (;;)
    {
        {
            const juce::ScopedLock sl (lock);
            if (! requestPending && ! scanning.load())
                return true;
        }
        if (juce::Time::getMillisecondCounter() > end)
            return false;
        juce::Thread::sleep (5);
    }
}

std::vector<int> LibraryIndex::filter (const std::vector<Entry>& list, const juce::String& category, const juce::String& search)
{
    juce::StringArray words;
    words.addTokens (search.toLowerCase(), " \t,", "\"");
    words.removeEmptyStrings();

    std::vector<int> result;
    for (size_t i = 0; i < list.size(); ++i)
    {
        const auto& entry = list[i];
        if (category.isNotEmpty() && entry.category != category)
            continue;
        const auto haystack = (entry.name + " " + entry.package + " " + entry.tags.joinIntoString (" ") + " " + entry.author + " " + entry.license).toLowerCase();
        auto all = true;
        for (const auto& word : words)
            all = all && haystack.contains (word);
        if (all)
            result.push_back ((int) i);
    }
    return result;
}

juce::String LibraryIndex::signatureOf (const juce::File& root, const AbortCheck& shouldAbort, bool& aborted)
{
    aborted = false;
    // Bara mapparnas och manifestens ändringstider (och att de finns): ljudfilerna rörs inte. En ny eller borttagen paketmapp ändrar
    // kategorimappens tid, ett nytt manifest eller en ändrad lista ändrar manifestets.
    juce::String text;
    const auto add = [&text] (const juce::File& file)
    {
        text << file.getFullPathName() << '|' << file.getLastModificationTime().toMilliseconds() << '|' << file.getSize() << '\n';
    };
    if (! root.isDirectory())
        return "none:" + root.getFullPathName();
    add (root);
    for (const auto& category : root.findChildFiles (juce::File::findDirectories, false))
    {
        if (shouldAbort && shouldAbort())
        {
            aborted = true;
            return {};
        }
        add (category);
        for (const auto& package : category.findChildFiles (juce::File::findDirectories, false))
        {
            if (shouldAbort && shouldAbort())
            {
                aborted = true;
                return {};
            }
            add (package);
            add (package.getChildFile (Library::manifestName));
        }
    }
    return text;
}

LibraryIndex::Scan LibraryIndex::scan (const juce::File& root, const AbortCheck& shouldAbort)
{
    Scan result;
    result.signature = signatureOf (root, shouldAbort, result.aborted);
    if (result.aborted)
        return result;
    auto list = std::make_shared<std::vector<Entry>>();
    if (root.isDirectory())
    {
        auto categoryFolders = root.findChildFiles (juce::File::findDirectories, false);
        categoryFolders.sort();
        for (const auto& categoryFolder : categoryFolders)
        {
            if (shouldAbort && shouldAbort())
            {
                result.aborted = true;
                return result;
            }
            const auto category = categoryFolder.getFileName();
            if (! Library::categories().contains (category))
                continue;
            auto packageFolders = categoryFolder.findChildFiles (juce::File::findDirectories, false);
            packageFolders.sort();
            for (const auto& packageFolder : packageFolders)
            {
                if (shouldAbort && shouldAbort())
                {
                    result.aborted = true;
                    return result;
                }
                if (packageFolder.getFileName().startsWithChar ('.'))
                    continue;
                const auto parsed = Library::parseFile (packageFolder.getChildFile (Library::manifestName), true); // en post vars fil saknas visas inte
                if (! parsed.ok())
                {
                    for (const auto& error : parsed.errors)
                        result.problems.add (category + "\\" + packageFolder.getFileName() + ": " + error);
                    continue;
                }
                ++result.packages;
                for (const auto& item : parsed.manifest.items)
                {
                    Entry entry;
                    entry.category = category;
                    entry.package = parsed.manifest.package;
                    entry.license = parsed.manifest.license;
                    entry.source = parsed.manifest.source;
                    entry.url = parsed.manifest.url;
                    entry.name = item.name;
                    entry.author = item.author;
                    entry.key = item.key;
                    entry.tags = item.tags;
                    entry.rootNote = item.rootNote;
                    entry.bpm = item.bpm;
                    entry.personal = Library::isPersonal (parsed.manifest.license);
                    entry.file = packageFolder.getChildFile (item.file);
                    list->push_back (std::move (entry));
                }
            }
        }
    }
    result.entries = list;
    return result;
}

void LibraryIndex::publish (Scan&& result, const juce::File& root)
{
    {
        const juce::ScopedLock sl (lock);
        current = std::move (result.entries);
        currentProblems = std::move (result.problems);
        currentPackages = result.packages;
        currentSignature = std::move (result.signature);
        currentRoot = root;
    }
    ++publishedVersion;
    triggerAsyncUpdate();
}

void LibraryIndex::run()
{
    while (! threadShouldExit())
    {
        wait (500); // Thread::wait: vaknar vid notify() (en beställning, eller stopThread) eller efter 500 ms
        if (threadShouldExit())
            break;

        juce::File root;
        bool forced = false;
        juce::String knownSignature;
        juce::File knownRoot;
        {
            const juce::ScopedLock sl (lock);
            if (! requestPending)
                continue;
            root = requestedRoot;
            forced = requestForced;
            requestPending = false;
            requestForced = false;
            knownSignature = currentSignature;
            knownRoot = currentRoot;
            scanning = true;
        }

        const AbortCheck shouldAbort = [this] { return threadShouldExit(); };
        auto aborted = false;
        const auto changed = forced || root != knownRoot || signatureOf (root, shouldAbort, aborted) != knownSignature;
        if (aborted)
        {
            scanning = false;
            break;
        }
        if (changed)
        {
            auto result = scan (root, shouldAbort);
            if (result.aborted)
            {
                scanning = false; // avbruten: publicera inget
                break;
            }
            publish (std::move (result), root);
        }
        scanning = false;
    }
}

void LibraryIndex::handleAsyncUpdate()
{
    sendChangeMessage();
}
