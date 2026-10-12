#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ------------------------------------------------------------ formatting
namespace
{
    String fmtHz (float v, int)   { return v >= 1000.f ? String (v / 1000.f, 2) + " kHz" : String ((int) std::round (v)) + " Hz"; }
    String fmtMs (float v, int)   { return v >= 1000.f ? String (v / 1000.f, 2) + " s" : String (v, v < 10.f ? 1 : 0) + " ms"; }
    String fmtDb (float v, int)   { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; }
    String fmtPct (float v, int)  { return String ((int) std::round (v * 100.f)) + "%"; }
    String fmtSemi (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " st"; }
    float  parseNum (const String& t) { return t.retainCharacters ("0123456789.-+").getFloatValue(); }
    float  parseHz (const String& t)  { auto v = parseNum (t); return t.containsIgnoreCase ("k") ? v * 1000.f : v; }
    float  parseMs (const String& t)  { auto v = parseNum (t); return (t.trim().endsWithIgnoreCase ("s") && ! t.containsIgnoreCase ("ms")) ? v * 1000.f : v; }
    float  parsePct (const String& t) { return parseNum (t) / 100.f; }

    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    std::unique_ptr<AudioParameterFloat> fparam (const String& id, const String& name, NormalisableRange<float> range, float def,
                                                 String (*fmt) (float, int), float (*parse) (const String&))
    {
        auto attrs = AudioParameterFloatAttributes().withStringFromValueFunction (fmt).withValueFromStringFunction (parse);
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def, attrs);
    }
    std::unique_ptr<AudioParameterBool> bparam (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    }

    struct LayerDefaults { const char* name; float level, length, maxLength, hp, lp, maxStart; };
    const LayerDefaults layerDefaults[NumSampleLayers] {
        { "Punch",  0.f,  150.f, 1000.f, 20.f,  20000.f, 100.f },
        { "Crunch", -6.f, 220.f, 1500.f, 250.f, 12000.f, 100.f },
        { "Kick",   0.f,  800.f, 2500.f, 20.f,  20000.f, 200.f },
    };
}

AudioProcessorValueTreeState::ParameterLayout AncientProcessor::createLayout()
{
    using namespace ParamIDs;
    AudioProcessorValueTreeState::ParameterLayout l;

    for (int i = 0; i < NumSampleLayers; ++i)
    {
        const auto& d = layerDefaults[i];
        const String n (d.name);
        l.add (bparam (id (i, on), n + " On", true));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id (i, src), 1 }, n + " Sample", Library::choicesFor (i), 0));
        l.add (fparam (id (i, level),  n + " Level", { -36.f, 6.f }, d.level, fmtDb, parseNum));
        l.add (fparam (id (i, pitch),  n + " Pitch", { -24.f, 24.f, 0.01f }, 0.f, fmtSemi, parseNum));
        l.add (fparam (id (i, start),  n + " Start", skewed (0.f, d.maxStart, d.maxStart * 0.2f), 0.f, fmtMs, parseMs));
        l.add (fparam (id (i, length), n + " Length", skewed (10.f, d.maxLength, d.maxLength * 0.25f), d.length, fmtMs, parseMs));
        l.add (fparam (id (i, hp),     n + " Low Cut", skewed (20.f, 2000.f, 250.f), d.hp, fmtHz, parseHz));
        l.add (fparam (id (i, lp),     n + " High Cut", skewed (200.f, 20000.f, 3000.f), d.lp, fmtHz, parseHz));
    }
    l.add (bparam (punchKey, "Punch Key Track", false));

    // SUB
    l.add (bparam (subOn, "Sub On", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { subWave, 1 }, "Sub Wave", Choices::subWaves, 0));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { subOct, 1 }, "Sub Octave", -2, 1, 0));
    l.add (fparam (subLevel,  "Sub Level", { -36.f, 6.f }, -6.f, fmtDb, parseNum));
    l.add (fparam (subAttack, "Sub Attack", skewed (0.1f, 100.f, 10.f), 2.f, fmtMs, parseMs));
    l.add (fparam (subLength, "Sub Length", skewed (20.f, 2500.f, 500.f), 450.f, fmtMs, parseMs));
    l.add (fparam (subTone,   "Sub Tone", skewed (40.f, 5000.f, 400.f), 300.f, fmtHz, parseHz));
    l.add (fparam (subDrive,  "Sub Drive", { 0.f, 1.f }, 0.f, fmtPct, parsePct));

    // RUMBLE
    l.add (bparam (rumOn, "Rumble On", true));
    l.add (fparam (rumLevel, "Rumble Level", { -36.f, 6.f }, -9.f, fmtDb, parseNum));
    l.add (fparam (rumDelay, "Rumble Delay", skewed (0.f, 250.f, 60.f), 60.f, fmtMs, parseMs));
    l.add (fparam (rumDecay, "Rumble Decay", { 0.f, 1.f }, 0.55f, fmtPct, parsePct));
    l.add (fparam (rumDrive, "Rumble Drive", { 0.f, 1.f }, 0.5f, fmtPct, parsePct));
    l.add (fparam (rumLp,    "Rumble Low Pass", skewed (30.f, 500.f, 120.f), 110.f, fmtHz, parseHz));
    l.add (fparam (rumDuck,  "Rumble Duck", { 0.f, 1.f }, 0.8f, fmtPct, parsePct));

    // MASTER
    l.add (std::make_unique<AudioParameterInt> (ParameterID { root, 1 }, "Root Note", 24, 96, 60, AudioParameterIntAttributes()
              .withStringFromValueFunction ([] (int v, int) { return MidiMessage::getMidiNoteName (v, true, true, 5); })
              .withValueFromStringFunction ([] (const String& t)
              {
                  static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                  const auto up = t.trim().toUpperCase();
                  for (int n = 11; n >= 0; --n)
                      if (up.startsWith (names[n]))
                          return jlimit (24, 96, n + 12 * up.substring ((int) strlen (names[n])).getIntValue());
                  return t.getIntValue();
              })));
    l.add (fparam (preGain,  "Pre-Gain", { -12.f, 24.f }, 0.f, fmtDb, parseNum));
    l.add (bparam (clipOn,   "Clipper On", true));
    l.add (fparam (clipCeil, "Clipper Ceiling", { -12.f, 0.f }, -0.5f, fmtDb, parseNum));
    l.add (fparam (clipKnee, "Clipper Softness", { 0.f, 1.f }, 0.25f, fmtPct, parsePct));
    l.add (bparam (limOn,    "Limiter On", true));
    l.add (fparam (limCeil,  "Limiter Ceiling", { -12.f, 0.f }, -0.3f, fmtDb, parseNum));
    l.add (fparam (limRel,   "Limiter Release", skewed (5.f, 500.f, 60.f), 60.f, fmtMs, parseMs));
    l.add (fparam (volume,   "Volume", { -36.f, 6.f }, -3.f, fmtDb, parseNum));
    return l;
}

// ------------------------------------------------------------ factory presets
namespace
{
    struct Preset { const char* name; std::vector<std::pair<String, float>> values; };

    const std::vector<Preset>& presets()
    {
        using namespace ParamIDs;
        static const std::vector<Preset> p {
            { "Ancient Raw", {} },
            { "Temple Euphoric", { { id (Kick, src), 3 }, { id (Crunch, src), 3 }, { id (Crunch, level), -12 }, { id (Punch, src), 1 },
                                   { id (Kick, length), 650 }, { rumOn, 0 }, { subOn, 1 }, { subLevel, -9 }, { subLength, 520 },
                                   { clipKnee, 0.5f } } },
            { "Pharaoh Uptempo", { { id (Kick, src), 0 }, { id (Kick, pitch), 5 }, { id (Kick, length), 380 },
                                   { id (Crunch, src), 1 }, { id (Crunch, level), -3 }, { id (Punch, src), 5 },
                                   { preGain, 9 }, { clipKnee, 0.05f }, { rumDelay, 40 }, { rumDecay, 0.35f } } },
            { "Babylon Hardcore", { { id (Kick, src), 2 }, { id (Kick, length), 1100 }, { id (Crunch, src), 2 },
                                    { rumLevel, -6 }, { rumDecay, 0.75f }, { rumDrive, 0.8f }, { rumLp, 90 }, { preGain, 6 } } },
            { "Colossus Frenchcore", { { id (Kick, src), 1 }, { id (Kick, length), 1400 }, { id (Kick, pitch), 2 },
                                       { id (Crunch, src), 5 }, { id (Crunch, level), -4 }, { id (Punch, src), 2 },
                                       { rumOn, 0 }, { preGain, 12 }, { clipKnee, 0.f } } },
            { "Monolith Rawstyle", { { id (Kick, src), 5 }, { id (Kick, length), 900 }, { id (Crunch, src), 4 }, { id (Crunch, pitch), 3 },
                                     { id (Punch, src), 6 }, { rumLevel, -7 }, { rumDelay, 80 }, { preGain, 4 } } },
            { "Punch Only", { { id (Crunch, on), 0 }, { id (Kick, on), 0 }, { rumOn, 0 }, { id (Punch, src), 0 } } },
            { "Sub & Rumble", { { id (Punch, on), 0 }, { id (Crunch, on), 0 }, { id (Kick, src), 3 }, { subOn, 1 },
                                { subWave, 0 }, { subLevel, -4 }, { rumLevel, -5 } } },
        };
        return p;
    }
}

int AncientProcessor::getNumPrograms() { return (int) presets().size(); }

const String AncientProcessor::getProgramName (int i)
{
    return isPositiveAndBelow (i, (int) presets().size()) ? String (presets()[(size_t) i].name) : String();
}

void AncientProcessor::setCurrentProgram (int index)
{
    if (! isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    currentPresetName = presets()[(size_t) index].name;

    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (auto& [pid, v] : presets()[(size_t) index].values)
        if (auto* rp = apvts.getParameter (pid))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

// ------------------------------------------------------------ construction
AncientProcessor::AncientProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ANCIENT", createLayout())
{
    formats.registerBasicFormats();
    SampleLibrary::get();   // build the library now, not on the audio thread

    using namespace ParamIDs;
    auto g = [this] (const String& pid) { auto* r = apvts.getRawParameterValue (pid); jassert (r != nullptr); return r; };
    for (int i = 0; i < NumSampleLayers; ++i)
    {
        ptr.on[(size_t) i] = g (id (i, on));        ptr.src[(size_t) i] = g (id (i, src));
        ptr.level[(size_t) i] = g (id (i, level));  ptr.pitch[(size_t) i] = g (id (i, pitch));
        ptr.start[(size_t) i] = g (id (i, start));  ptr.length[(size_t) i] = g (id (i, length));
        ptr.hp[(size_t) i] = g (id (i, hp));        ptr.lp[(size_t) i] = g (id (i, lp));
    }
    ptr.punchKey = g (punchKey);
    ptr.subOn = g (subOn); ptr.subWave = g (subWave); ptr.subOct = g (subOct); ptr.subLevel = g (subLevel);
    ptr.subAttack = g (subAttack); ptr.subLength = g (subLength); ptr.subTone = g (subTone); ptr.subDrive = g (subDrive);
    ptr.rumOn = g (rumOn); ptr.rumLevel = g (rumLevel); ptr.rumDelay = g (rumDelay); ptr.rumDecay = g (rumDecay);
    ptr.rumDrive = g (rumDrive); ptr.rumLp = g (rumLp); ptr.rumDuck = g (rumDuck);
    ptr.root = g (root); ptr.preGain = g (preGain); ptr.clipOn = g (clipOn); ptr.clipCeil = g (clipCeil);
    ptr.clipKnee = g (clipKnee); ptr.limOn = g (limOn); ptr.limCeil = g (limCeil); ptr.limRel = g (limRel);
    ptr.volume = g (volume);
}

bool AncientProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void AncientProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (gatherParams());
    engine.prepare (sampleRate, samplesPerBlock);
}

SamplePtr AncientProcessor::getLayerSample (int layer) const
{
    const int s = (int) std::round (ptr.src[(size_t) layer]->load());
    if (s >= Library::userIndex)
        return std::atomic_load (&userSample[(size_t) layer]);
    return SampleLibrary::get().sample (layer, s);
}

KickParams AncientProcessor::gatherParams()
{
    auto db = [] (std::atomic<float>* a) { return Decibels::decibelsToGain (a->load(), -60.f); };
    auto on = [] (std::atomic<float>* a) { return a->load() > 0.5f; };

    KickParams k;
    for (int i = 0; i < NumSampleLayers; ++i)
    {
        auto& L = k.layer[(size_t) i];
        const auto u = (size_t) i;
        L.on = on (ptr.on[u]);
        L.gain = db (ptr.level[u]);
        L.pitch = ptr.pitch[u]->load();
        L.startMs = ptr.start[u]->load();
        L.lengthMs = ptr.length[u]->load();
        L.hpHz = ptr.hp[u]->load();
        L.lpHz = ptr.lp[u]->load();
        L.keyTrack = i == Punch ? on (ptr.punchKey) : true;      // crunch & kick always follow the note
        L.sample = getLayerSample (i);
    }
    k.subOn = on (ptr.subOn);
    k.subWave = jlimit (0, 2, (int) std::round (ptr.subWave->load()));
    k.subOct = (int) std::round (ptr.subOct->load());
    k.subGain = db (ptr.subLevel);
    k.subAttackMs = ptr.subAttack->load();
    k.subLengthMs = ptr.subLength->load();
    k.subToneHz = ptr.subTone->load();
    k.subDrive = ptr.subDrive->load();

    k.rumOn = on (ptr.rumOn);
    k.rumGain = db (ptr.rumLevel);
    k.rumDelayMs = ptr.rumDelay->load();
    k.rumDecay = ptr.rumDecay->load();
    k.rumDrive = ptr.rumDrive->load();
    k.rumLpHz = ptr.rumLp->load();
    k.rumDuck = ptr.rumDuck->load();

    k.root = (int) std::round (ptr.root->load());
    k.preGain = Decibels::decibelsToGain (ptr.preGain->load());
    k.clipOn = on (ptr.clipOn);
    k.clipCeil = Decibels::decibelsToGain (ptr.clipCeil->load());
    k.clipKnee = ptr.clipKnee->load();
    k.limOn = on (ptr.limOn);
    k.limCeilDb = ptr.limCeil->load();
    k.limRelMs = ptr.limRel->load();
    k.volume = db (ptr.volume);
    return k;
}

void AncientProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    if (buffer.getNumChannels() < 2) { buffer.clear(); return; }

    engine.setParams (gatherParams());

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    if (testTrigger.exchange (false))
        engine.trigger ((int) std::round (ptr.root->load()));

    int pos = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn()) continue;
        const int at = jlimit (0, n, meta.samplePosition);
        if (at > pos) { engine.process (L + pos, R + pos, at - pos); pos = at; }
        engine.trigger (msg.getNoteNumber());
    }
    if (pos < n) engine.process (L + pos, R + pos, n - pos);
}

// ------------------------------------------------------------ user samples
bool AncientProcessor::isSupportedAudioFile (const String& path)
{
    return File (path).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}

bool AncientProcessor::loadUserSample (int layer, const File& file)
{
    if (! isPositiveAndBelow (layer, (int) NumSampleLayers) || ! file.existsAsFile()) return false;

    std::unique_ptr<AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;

    const int len = (int) std::min<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 20.0));
    const int chans = jlimit (1, 2, (int) reader->numChannels);
    AudioBuffer<float> tmp (chans, len);
    reader->read (&tmp, 0, len, 0, true, chans > 1);

    auto s = std::make_shared<SampleData>();
    s->data.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        float v = 0.f;
        for (int c = 0; c < chans; ++c) v += tmp.getSample (c, i);
        s->data[(size_t) i] = v / (float) chans;
    }
    s->sampleRate = reader->sampleRate;
    s->name = file.getFileNameWithoutExtension();
    s->isUser = true;
    s->path = file.getFullPathName();

    keepAlive.push_back (std::atomic_load (&userSample[(size_t) layer]));
    std::atomic_store (&userSample[(size_t) layer], SamplePtr (s));
    apvts.state.setProperty ("user" + String (layer), s->path, nullptr);

    if (auto* p = apvts.getParameter (ParamIDs::id (layer, ParamIDs::src)))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) Library::userIndex));

    collectGarbage();
    return true;
}

String AncientProcessor::getUserSamplePath (int layer) const
{
    return apvts.state.getProperty ("user" + String (layer)).toString();
}

void AncientProcessor::collectGarbage()
{
    keepAlive.erase (std::remove_if (keepAlive.begin(), keepAlive.end(),
                                     [] (const SamplePtr& s) { return s == nullptr || s.use_count() <= 1; }),
                     keepAlive.end());
}

void AncientProcessor::restoreUserSamplesFromState()
{
    for (int l = 0; l < NumSampleLayers; ++l)
    {
        const auto path = getUserSamplePath (l);
        auto current = std::atomic_load (&userSample[(size_t) l]);
        if (path.isEmpty())
        {
            if (current != nullptr) { keepAlive.push_back (current); std::atomic_store (&userSample[(size_t) l], SamplePtr()); }
            continue;
        }
        if (current != nullptr && current->path == path) continue;

        // load without changing the layer's source selection
        auto* srcParam = apvts.getParameter (ParamIDs::id (l, ParamIDs::src));
        const float before = srcParam != nullptr ? srcParam->getValue() : 0.f;
        if (! loadUserSample (l, File (path)))
        {
            // keep the path so the GUI can say what is missing
            keepAlive.push_back (current);
            std::atomic_store (&userSample[(size_t) l], SamplePtr());
            apvts.state.setProperty ("user" + String (l), path, nullptr);
        }
        if (srcParam != nullptr) srcParam->setValueNotifyingHost (before);
    }
    collectGarbage();
}

// ------------------------------------------------------------ user presets
File AncientProcessor::getUserPresetFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Ancient Drums").getChildFile ("Presets");
}

Array<File> AncientProcessor::getUserPresets() const
{
    auto files = getUserPresetFolder().findChildFiles (File::findFiles, false, "*.adp");
    files.sort();
    return files;
}

bool AncientProcessor::saveUserPreset (const String& name)
{
    const auto clean = File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto folder = getUserPresetFolder();
    if (! folder.createDirectory()) return false;

    std::unique_ptr<XmlElement> xml (apvts.copyState().createXml());
    if (xml == nullptr) return false;
    xml->removeAttribute ("program");
    xml->removeAttribute ("presetName");
    if (! xml->writeTo (folder.getChildFile (clean + ".adp"))) return false;
    currentPresetName = clean;
    return true;
}

void AncientProcessor::applyValues (const XmlElement& xml)
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (auto* child : xml.getChildWithTagNameIterator ("PARAM"))
        if (auto* rp = apvts.getParameter (child->getStringAttribute ("id")))
            rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child->getDoubleAttribute ("value")));
}

bool AncientProcessor::loadUserPreset (const File& file)
{
    auto xml = XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    applyValues (*xml);
    for (int l = 0; l < NumSampleLayers; ++l)
        apvts.state.setProperty ("user" + String (l), xml->getStringAttribute ("user" + String (l)), nullptr);
    restoreUserSamplesFromState();
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

// ------------------------------------------------------------ randomize
void AncientProcessor::randomize()
{
    using namespace ParamIDs;
    auto& r = rng;
    auto set = [this] (const String& pid, float v)
    {
        if (auto* p = apvts.getParameter (pid))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };
    auto between = [&r] (float lo, float hi) { return lo + r.nextFloat() * (hi - lo); };
    auto logBetween = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };
    auto chance = [&r] (float p) { return r.nextFloat() < p ? 1.f : 0.f; };

    for (int l = 0; l < NumSampleLayers; ++l)
    {
        // a layer playing your own sample keeps it; library layers get a new sample
        if ((int) std::round (ptr.src[(size_t) l]->load()) != Library::userIndex)
            set (id (l, src), (float) r.nextInt (Library::perLayer));
        set (id (l, start), 0.f);
    }
    set (id (Punch, on), chance (0.8f));
    set (id (Punch, level), between (-8.f, 0.f));
    set (id (Punch, pitch), between (-5.f, 7.f));
    set (id (Punch, length), logBetween (70.f, 220.f));
    set (id (Punch, hp), logBetween (20.f, 200.f));
    set (id (Punch, lp), logBetween (6000.f, 20000.f));
    set (punchKey, chance (0.3f));

    set (id (Crunch, on), chance (0.85f));
    set (id (Crunch, level), between (-14.f, -2.f));
    set (id (Crunch, pitch), between (-7.f, 7.f));
    set (id (Crunch, length), logBetween (100.f, 350.f));
    set (id (Crunch, hp), logBetween (150.f, 700.f));
    set (id (Crunch, lp), logBetween (5000.f, 18000.f));

    set (id (Kick, on), 1.f);
    set (id (Kick, level), between (-3.f, 0.f));
    set (id (Kick, pitch), between (-3.f, 3.f));
    set (id (Kick, length), logBetween (380.f, 1300.f));
    set (id (Kick, hp), 20.f);
    set (id (Kick, lp), logBetween (5000.f, 20000.f));

    set (subOn, chance (0.4f));
    set (subWave, (float) r.nextInt (3));
    set (subOct, chance (0.8f) > 0.f ? 0.f : -1.f);
    set (subLevel, between (-14.f, -4.f));
    set (subAttack, logBetween (0.5f, 20.f));
    set (subLength, logBetween (250.f, 900.f));
    set (subTone, logBetween (120.f, 900.f));
    set (subDrive, between (0.f, 0.5f));

    set (rumOn, chance (0.6f));
    set (rumLevel, between (-14.f, -5.f));
    set (rumDelay, between (30.f, 110.f));
    set (rumDecay, between (0.3f, 0.85f));
    set (rumDrive, between (0.2f, 0.9f));
    set (rumLp, logBetween (70.f, 200.f));
    set (rumDuck, between (0.5f, 1.f));

    set (preGain, between (0.f, 9.f));
    set (clipOn, 1.f);
    set (clipCeil, between (-1.5f, -0.2f));
    set (clipKnee, between (0.f, 0.6f));
    set (limOn, 1.f);
    // root note and volume are left alone on purpose

    currentPresetName = "Random #" + String (1000 + r.nextInt (9000));
}

// ------------------------------------------------------------ state
AudioProcessorEditor* AncientProcessor::createEditor() { return new AncientEditor (*this); }

void AncientProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void AncientProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = ValueTree::fromXml (*xml);
            currentProgram = vt.getProperty ("program", 0);
            currentPresetName = vt.getProperty ("presetName", getProgramName (currentProgram)).toString();
            apvts.replaceState (vt);
            restoreUserSamplesFromState();
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AncientProcessor(); }
