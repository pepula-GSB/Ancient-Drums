// Offline test: renders every preset, checks user-sample loading, layer toggles,
// randomize, preset save/load, state round-trip and snapshots the GUI.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

using namespace juce;

static const double sr = 48000.0;
static const int block = 512;

struct Stats { float peak = 0.f; double rms = 0.0; bool nan = false; };

static Stats renderHits (AncientProcessor& proc, AudioBuffer<float>* dest, int destOffset, int hits = 4, int note = 60)
{
    const int beat = (int) (sr * 60.0 / 150.0);
    const int total = beat * hits;
    Stats st; double sum = 0.0;
    for (int pos = 0; pos < total; pos += block)
    {
        AudioBuffer<float> buf (2, block); buf.clear();
        MidiBuffer midi;
        for (int h = 0; h < hits; ++h)
        {
            const int at = h * beat;
            if (at >= pos && at < pos + block) midi.addEvent (MidiMessage::noteOn (1, note + (h == 3 ? 5 : 0), (uint8) 120), at - pos);
        }
        proc.processBlock (buf, midi);
        for (int i = 0; i < block; ++i)
        {
            const float v = buf.getSample (0, i);
            if (! std::isfinite (v)) st.nan = true;
            st.peak = jmax (st.peak, std::abs (v));
            sum += v * v;
        }
        if (dest != nullptr && destOffset + pos + block <= dest->getNumSamples())
            for (int ch = 0; ch < 2; ++ch) dest->copyFrom (ch, destOffset + pos, buf, ch, 0, block);
    }
    st.rms = std::sqrt (sum / total);
    return st;
}

static void set (AncientProcessor& p, const String& id, float v)
{
    if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir = argc > 1 ? File (argv[1]) : File::getCurrentWorkingDirectory();
    bool ok = true;

    AncientProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    const int beat = (int) (sr * 60.0 / 150.0);
    AudioBuffer<float> demo (2, beat * 4 * (proc.getNumPrograms() + 1));
    demo.clear();

    for (int prog = 0; prog < proc.getNumPrograms(); ++prog)
    {
        proc.setCurrentProgram (prog);
        auto st = renderHits (proc, &demo, prog * beat * 4);
        const bool pass = ! st.nan && st.peak > 0.1f && st.peak <= 1.0f;
        ok &= pass;
        printf ("%-20s peak %.3f  rms %.3f  %s\n", proc.getProgramName (prog).toRawUTF8(), st.peak, st.rms, pass ? "OK" : "FAIL");
    }

    // ---- every library sample on its own
    proc.setCurrentProgram (0);
    for (int l = 0; l < NumSampleLayers; ++l)
        for (int s = 0; s < Library::perLayer; ++s)
        {
            set (proc, ParamIDs::id (Punch, ParamIDs::on), l == Punch);
            set (proc, ParamIDs::id (Crunch, ParamIDs::on), l == Crunch);
            set (proc, ParamIDs::id (Kick, ParamIDs::on), l == Kick);
            set (proc, ParamIDs::rumOn, 0);
            set (proc, ParamIDs::id (l, ParamIDs::src), (float) s);
            auto st = renderHits (proc, nullptr, 0, 1);
            if (st.nan || st.peak < 0.05f) { printf ("library layer %d sample %d FAIL (%.3f)\n", l, s, st.peak); ok = false; }
        }
    printf ("library samples x24: checked\n");

    // ---- layer toggles: everything off -> silence
    proc.setCurrentProgram (0);
    for (int l = 0; l < NumSampleLayers; ++l) set (proc, ParamIDs::id (l, ParamIDs::on), 0);
    set (proc, ParamIDs::subOn, 0);
    set (proc, ParamIDs::rumOn, 0);
    renderHits (proc, nullptr, 0, 4);  // let the rumble window close
    auto silent = renderHits (proc, nullptr, 0, 2);
    printf ("all layers off -> peak %.5f %s\n", silent.peak, silent.peak < 0.001f ? "OK" : "FAIL");
    ok &= silent.peak < 0.001f;

    // ---- rumble dies out after the last kick
    {
        proc.setCurrentProgram (3);   // Babylon Hardcore: long, driven rumble
        renderHits (proc, nullptr, 0, 1);
        MidiBuffer none; AudioBuffer<float> buf (2, block); float tail = 0.f;
        for (int b = 0; b < (int) (sr * 2.0) / block; ++b) { buf.clear(); proc.processBlock (buf, none); }
        for (int b = 0; b < 20; ++b) { buf.clear(); proc.processBlock (buf, none); tail = jmax (tail, buf.getMagnitude (0, 0, block)); }
        printf ("rumble tail 2.4 s after hit: %.5f %s\n", tail, tail < 0.002f ? "OK" : "FAIL");
        ok &= tail < 0.002f;
        proc.setCurrentProgram (0);
        for (int l = 0; l < NumSampleLayers; ++l) set (proc, ParamIDs::id (l, ParamIDs::on), 0);
    }

    // ---- sub only, each waveform
    for (int w = 0; w < 3; ++w)
    {
        set (proc, ParamIDs::subOn, 1); set (proc, ParamIDs::subWave, (float) w);
        auto st = renderHits (proc, nullptr, 0, 1);
        printf ("sub wave %d -> peak %.3f %s\n", w, st.peak, st.peak > 0.05f && ! st.nan ? "OK" : "FAIL");
        ok &= st.peak > 0.05f && ! st.nan;
    }

    // ---- user sample: write a wav, load it into the crunch layer
    auto userWav = outDir.getChildFile ("user_crunch.wav");
    {
        AudioBuffer<float> b (1, 9600);
        Random r (3);
        for (int i = 0; i < 9600; ++i) b.setSample (0, i, std::tanh (8.f * std::sin (i * 0.06f) + r.nextFloat() - 0.5f) * std::exp (-i / 2500.f) * 0.8f);
        userWav.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (userWav), 44100.0, 1, 24, {}, 0));
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    proc.setCurrentProgram (0);
    const bool loaded = proc.loadUserSample (Crunch, userWav);
    auto us = proc.getLayerSample (Crunch);
    const bool userOk = loaded && us != nullptr && us->isUser && us->data.size() == 9600;
    printf ("user sample load: %s\n", userOk ? "OK" : "FAIL");
    ok &= userOk;
    auto stUser = renderHits (proc, &demo, proc.getNumPrograms() * beat * 4);
    ok &= ! stUser.nan && stUser.peak > 0.1f;

    // ---- state round trip keeps the user sample
    MemoryBlock state;
    proc.getStateInformation (state);
    AncientProcessor proc2;
    proc2.setPlayConfigDetails (0, 2, sr, block);
    proc2.prepareToPlay (sr, block);
    proc2.setStateInformation (state.getData(), (int) state.getSize());
    auto us2 = proc2.getLayerSample (Crunch);
    const bool stateOk = us2 != nullptr && us2->isUser && us2->path == userWav.getFullPathName();
    printf ("state round-trip with user sample: %s\n", stateOk ? "OK" : "FAIL");
    ok &= stateOk;

    // ---- randomize
    for (int i = 0; i < 30; ++i)
    {
        proc.randomize();
        auto st = renderHits (proc, nullptr, 0, 1);
        if (st.nan || st.peak < 0.05f || st.peak > 1.0f) { printf ("randomize #%d FAIL peak %.3f\n", i, st.peak); ok = false; }
    }
    printf ("randomize x30: done\n");

    // ---- user preset save / load
    {
        const float len = proc.apvts.getRawParameterValue (ParamIDs::id (Kick, ParamIDs::length))->load();
        const bool saved = proc.saveUserPreset ("__test_kick");
        auto f = AncientProcessor::getUserPresetFolder().getChildFile ("__test_kick.adp");
        proc.setCurrentProgram (3);
        const bool back = proc.loadUserPreset (f);
        const bool same = std::abs (proc.apvts.getRawParameterValue (ParamIDs::id (Kick, ParamIDs::length))->load() - len) < 1.f;
        printf ("user preset save/load: %s\n", saved && back && same ? "OK" : "FAIL");
        ok &= saved && back && same;
        f.deleteFile();
    }

    // ---- demo wav
    {
        auto f = outDir.getChildFile ("ancient_demo.wav");
        f.deleteFile();
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
        w->writeFromAudioSampleBuffer (demo, 0, demo.getNumSamples());
    }

    // ---- GUI snapshot: default preset, user sample on crunch, one hit captured
    proc.setCurrentProgram (0);
    proc.loadUserSample (Crunch, userWav);
    set (proc, ParamIDs::id (Crunch, ParamIDs::src), 0);   // back to library for the screenshot
    set (proc, ParamIDs::subOn, 1);
    renderHits (proc, nullptr, 0, 1);
    {
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (1280, 760);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile ("ancient_gui.png");
        f.deleteFile();
        FileOutputStream os (f);
        PNGImageFormat().writeImageToStream (img, os);
    }

    printf (ok ? "ALL OK\n" : "SOME CHECKS FAILED\n");
    return ok ? 0 : 1;
}
