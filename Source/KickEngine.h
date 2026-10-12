#pragma once
// =====================================================================
//  ANCIENT DRUMS - kick engine
//  PUNCH / CRUNCH / KICK sample layers + SUB oscillator  ->  bus
//  bus -> RUMBLE (delay > reverb > drive > LP, ducked)    -> + bus
//  -> PRE-GAIN -> CLIPPER (4x oversampled) -> LIMITER -> VOLUME
// =====================================================================
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "SampleLibrary.h"
#include <atomic>

struct LayerParams
{
    bool  on = true;
    float gain = 1.f, pitch = 0.f, startMs = 0.f, lengthMs = 1000.f, hpHz = 20.f, lpHz = 20000.f;
    bool  keyTrack = true;
    SamplePtr sample;
};

struct KickParams
{
    std::array<LayerParams, NumSampleLayers> layer;
    // sub
    bool  subOn = false; int subWave = 0; int subOct = 0;
    float subGain = 0.5f, subAttackMs = 2.f, subLengthMs = 500.f, subToneHz = 400.f, subDrive = 0.f;
    // rumble
    bool  rumOn = false;
    float rumGain = 0.5f, rumDelayMs = 60.f, rumDecay = 0.6f, rumDrive = 0.5f, rumLpHz = 120.f, rumDuck = 0.7f;
    // master
    int   root = 60;
    float preGain = 1.f; bool clipOn = true; float clipCeil = 0.95f, clipKnee = 0.2f;
    bool  limOn = true; float limCeilDb = -0.3f, limRelMs = 60.f; float volume = 1.f;
};

class KickEngine
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        maxBlockSize = juce::jmax (32, maxBlock);
        busBuf.assign ((size_t) maxBlockSize, 0.f);
        rumBuf.assign ((size_t) maxBlockSize, 0.f);

        rumDelay.assign ((size_t) (sr * 0.5) + 8, 0.f);
        rumWrite = 0;
        rumReverb.setSampleRate (sr);
        rumReverb.reset();

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampler->initProcessing ((size_t) maxBlockSize);

        limiter.prepare ({ sr, (juce::uint32) maxBlockSize, 1 });
        limiter.reset();

        hitBuffer.assign ((size_t) (sr * 1.5), 0.f);
        hitLength.store (0);

        sPre.reset (sr, 0.02);  sPre.setCurrentAndTargetValue (p.preGain);
        sVol.reset (sr, 0.02);  sVol.setCurrentAndTargetValue (p.volume);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = {};
        std::fill (rumDelay.begin(), rumDelay.end(), 0.f);
        rumReverb.reset();
        rumLp1 = {}; rumLp2 = {}; rumHp = 0.f; duckEnv = 0.f; rumWindow = 0.f; rumCountdown = 0;
        if (oversampler) oversampler->reset();
        limiter.reset();
    }

    void setParams (const KickParams& np) { p = np; }

    void trigger (int note)
    {
        // everything still sounding fades out in 4 ms -> no clicks on fast retriggers
        for (auto& v : voices)
            if (v.active) v.stealStep = 1.f / (float) (0.004 * sr);

        auto& v = voices[(size_t) nextVoice];
        nextVoice = (nextVoice + 1) % (int) voices.size();
        v = {};
        v.active = true;

        const float noteSemis = (float) (note - p.root);

        for (int l = 0; l < NumSampleLayers; ++l)
        {
            const auto& lp = p.layer[(size_t) l];
            auto& lv = v.layer[(size_t) l];
            if (! lp.on || lp.sample == nullptr || lp.sample->data.size() < 4) continue;

            const float semis = lp.pitch + (lp.keyTrack ? noteSemis : 0.f);
            lv.sample = lp.sample;
            lv.inc = lp.sample->sampleRate / sr * std::pow (2.0, semis / 12.0);
            lv.pos = juce::jmax (0.0, lp.startMs * 0.001 * lp.sample->sampleRate);
            lv.len = juce::jmax (8, (int) (lp.lengthMs * 0.001 * sr));
            lv.fade = juce::jlimit (4, lv.len, (int) juce::jmin (0.03 * sr, 0.3 * lv.len));
            lv.active = lv.pos < (double) lp.sample->data.size() - 2;
        }

        if (p.subOn)
        {
            auto& s = v.sub;
            const double hz = Library::tailHz * std::pow (2.0, (noteSemis + 12.f * (float) p.subOct) / 12.0);
            s.inc = hz / sr;
            s.phase = 0.0;
            s.len = juce::jmax (16, (int) (p.subLengthMs * 0.001 * sr));
            s.att = juce::jlimit (1, s.len / 2, (int) (p.subAttackMs * 0.001 * sr));
            s.fade = juce::jlimit (8, s.len, (int) (0.35 * s.len));
            s.active = true;
        }

        // rumble window opens when the (delayed) kick reaches the rumble chain
        rumCountdown = juce::jmax (1, (int) (p.rumDelayMs * 0.001 * sr));

        hitWrite = 0;
        hitLength.store (0);
        hitSerial.fetch_add (1);
    }

    void process (float* outL, float* outR, int numSamples)
    {
        int pos = 0;
        while (pos < numSamples)
        {
            const int n = juce::jmin (maxBlockSize, numSamples - pos);
            renderSegment (outL + pos, outR + pos, n);
            pos += n;
        }
    }

    // last-hit capture for the GUI
    std::vector<float> hitBuffer;
    std::atomic<int> hitLength { 0 };
    std::atomic<int> hitSerial { 0 };

private:
    struct Svf
    {
        float ic1 = 0.f, ic2 = 0.f;
        inline float lp (float v0, const float* c) { float l, b; tick (v0, c, l, b); return l; }
        inline float hp (float v0, const float* c) { float l, b; tick (v0, c, l, b); return v0 - c[3] * b - l; }
        inline void tick (float v0, const float* c, float& l, float& b)
        {
            const float v3 = v0 - ic2;
            const float v1 = c[0] * ic1 + c[1] * v3;
            const float v2 = ic2 + c[1] * ic1 + c[2] * v3;
            ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
            l = v2; b = v1;
        }
    };
    static void coeffs (float* c, float hz, float q, double rate)
    {
        const float g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (10.f, (float) (rate * 0.45), hz) / (float) rate);
        const float k = 1.f / q;
        c[0] = 1.f / (1.f + g * (g + k)); c[1] = g * c[0]; c[2] = g * c[1]; c[3] = k;
    }

    struct LayerVoice
    {
        SamplePtr sample;
        double pos = 0.0, inc = 1.0;
        int n = 0, len = 0, fade = 1;
        bool active = false;
        Svf hp, lp;
    };
    struct SubVoice
    {
        double phase = 0.0, inc = 0.0;
        int n = 0, len = 0, att = 1, fade = 1;
        bool active = false;
        Svf lp;
    };
    struct Voice
    {
        bool active = false;
        float steal = 1.f, stealStep = 0.f;
        std::array<LayerVoice, NumSampleLayers> layer;
        SubVoice sub;
    };

    static inline float hermite (const std::vector<float>& d, double pos)
    {
        const int i = (int) pos;
        const float f = (float) (pos - i);
        const int sz = (int) d.size();
        const float xm1 = d[(size_t) juce::jmax (0, i - 1)];
        const float x0  = d[(size_t) i];
        const float x1  = d[(size_t) juce::jmin (sz - 1, i + 1)];
        const float x2  = d[(size_t) juce::jmin (sz - 1, i + 2)];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    static inline float polyBlep (double t, double dt)
    {
        if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.f;
    }
    static inline float fadeCurve (int n, int len, int fade)
    {
        if (n < len - fade) return 1.f;
        if (n >= len) return 0.f;
        const float x = (float) (n - (len - fade)) / (float) fade;
        return 0.5f * (1.f + std::cos (juce::MathConstants<float>::pi * x));
    }
    static inline float softClip (float x, float ceil, float knee)
    {
        const float a = std::abs (x);
        const float t = ceil * (1.f - knee);
        if (a <= t) return x;
        const float s = x < 0.f ? -1.f : 1.f;
        if (knee <= 0.0001f) return s * juce::jmin (a, ceil);
        return s * (t + (ceil - t) * std::tanh ((a - t) / (ceil - t)));
    }

    void renderSegment (float* outL, float* outR, int len)
    {
        // live filter coefficients
        float hpC[NumSampleLayers][4], lpC[NumSampleLayers][4], subC[4];
        bool useHp[NumSampleLayers], useLp[NumSampleLayers];
        for (int l = 0; l < NumSampleLayers; ++l)
        {
            useHp[l] = p.layer[(size_t) l].hpHz > 21.f;
            useLp[l] = p.layer[(size_t) l].lpHz < 19900.f;
            coeffs (hpC[l], p.layer[(size_t) l].hpHz, 0.707f, sr);
            coeffs (lpC[l], p.layer[(size_t) l].lpHz, 0.707f, sr);
        }
        coeffs (subC, p.subToneHz, 0.707f, sr);
        const float subG = 1.f + p.subDrive * 12.f;
        const float subNorm = 1.f / std::tanh (subG);

        // ---------------- layers + sub -> bus
        for (int i = 0; i < len; ++i)
        {
            float bus = 0.f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                float vs = 0.f;
                bool any = false;

                for (int l = 0; l < NumSampleLayers; ++l)
                {
                    auto& lv = v.layer[(size_t) l];
                    if (! lv.active) continue;
                    const auto& data = lv.sample->data;
                    if (lv.pos >= (double) data.size() - 1 || lv.n >= lv.len) { lv.active = false; continue; }
                    any = true;

                    float x = hermite (data, lv.pos) * fadeCurve (lv.n, lv.len, lv.fade);
                    lv.pos += lv.inc;
                    ++lv.n;
                    if (useHp[l]) x = lv.hp.hp (x, hpC[l]);
                    if (useLp[l]) x = lv.lp.lp (x, lpC[l]);
                    const auto& lp = p.layer[(size_t) l];
                    vs += lp.on ? x * lp.gain : 0.f;
                }

                auto& s = v.sub;
                if (s.active)
                {
                    if (s.n >= s.len) s.active = false;
                    else
                    {
                        any = true;
                        float x;
                        switch (p.subWave)
                        {
                            case 0:  x = std::sin ((float) (s.phase * juce::MathConstants<double>::twoPi)); break;
                            case 1:  x = (float) (2.0 * s.phase - 1.0) - polyBlep (s.phase, s.inc); break;
                            default:
                            {
                                x = s.phase < 0.5 ? 1.f : -1.f;
                                x += polyBlep (s.phase, s.inc) - polyBlep (std::fmod (s.phase + 0.5, 1.0), s.inc);
                            }
                        }
                        s.phase += s.inc; if (s.phase >= 1.0) s.phase -= 1.0;
                        if (p.subWave != 0) x = s.lp.lp (x, subC);
                        if (p.subDrive > 0.001f) x = std::tanh (x * subG) * subNorm;
                        const float att = s.n < s.att ? (float) s.n / (float) s.att : 1.f;
                        x *= att * fadeCurve (s.n, s.len, s.fade);
                        ++s.n;
                        vs += p.subOn ? x * p.subGain : 0.f;
                    }
                }

                if (v.stealStep > 0.f)
                {
                    v.steal -= v.stealStep;
                    if (v.steal <= 0.f) { v.active = false; continue; }
                    vs *= v.steal;
                }
                if (! any) v.active = false;
                bus += vs;
            }
            busBuf[(size_t) i] = bus;
        }

        // ---------------- rumble
        const bool rumble = p.rumOn;
        if (rumble)
        {
            const int dSamp = juce::jlimit (1, (int) rumDelay.size() - 2, (int) (p.rumDelayMs * 0.001 * sr));
            const float hpC1 = 1.f - std::exp (-juce::MathConstants<float>::twoPi * 30.f / (float) sr);
            const float duckRel = std::exp (-1.f / (0.09f * (float) sr));
            for (int i = 0; i < len; ++i)
            {
                int rp = rumWrite - dSamp; if (rp < 0) rp += (int) rumDelay.size();
                rumBuf[(size_t) i] = rumDelay[(size_t) rp];
                rumDelay[(size_t) rumWrite] = busBuf[(size_t) i];
                if (++rumWrite >= (int) rumDelay.size()) rumWrite = 0;
                juce::ignoreUnused (hpC1);
            }

            juce::Reverb::Parameters rpm;
            rpm.roomSize = 0.3f + 0.68f * p.rumDecay;
            rpm.damping = 0.45f; rpm.wetLevel = 1.f; rpm.dryLevel = 0.f; rpm.width = 0.f;
            rumReverb.setParameters (rpm);
            rumReverb.processMono (rumBuf.data(), len);

            float lpCo[4]; coeffs (lpCo, p.rumLpHz, 0.6f, sr);
            // rumble length follows DECAY: ~150 ms .. ~1.2 s to -60 dB
            const double winSec = 0.15 + 1.05 * p.rumDecay;
            const float winCoef = (float) std::exp (std::log (0.001) / (winSec * sr));
            const float g = 1.f + p.rumDrive * 20.f;
            const float norm = 1.f / std::tanh (juce::jmin (g, 4.f));
            for (int i = 0; i < len; ++i)
            {
                float x = rumBuf[(size_t) i];
                rumHp += hpC1 * (x - rumHp); x -= rumHp;
                x = std::tanh (x * g) * norm;
                x = rumLp1.lp (x, lpCo);
                x = rumLp2.lp (x, lpCo);

                const float a = std::abs (busBuf[(size_t) i]);
                duckEnv = a > duckEnv ? a : duckEnv * duckRel;
                const float duck = 1.f - p.rumDuck * juce::jmin (1.f, duckEnv * 2.5f);

                if (rumCountdown > 0 && --rumCountdown == 0) rumWindow = 1.f;
                const float win = rumWindow;
                rumWindow *= winCoef;
                rumBuf[(size_t) i] = x * duck * p.rumGain * win;
            }
        }
        else
        {
            rumWindow = 0.f; rumCountdown = 0;
            if (rumWasOn) { rumReverb.reset(); rumLp1 = {}; rumLp2 = {}; rumHp = 0.f; }
            // keep the delay line fed so turning rumble on mid-pattern sounds right
            for (int i = 0; i < len; ++i)
            {
                rumDelay[(size_t) rumWrite] = busBuf[(size_t) i];
                if (++rumWrite >= (int) rumDelay.size()) rumWrite = 0;
            }
        }

        rumWasOn = rumble;

        // ---------------- master: pre-gain -> clipper -> limiter -> volume
        sPre.setTargetValue (p.preGain);
        sVol.setTargetValue (p.volume);
        for (int i = 0; i < len; ++i)
            outL[i] = (busBuf[(size_t) i] + (rumble ? rumBuf[(size_t) i] : 0.f)) * sPre.getNextValue();

        if (p.clipOn)
        {
            float* chans[] = { outL };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) len);
            auto os = oversampler->processSamplesUp (block);
            float* o = os.getChannelPointer (0);
            for (size_t i = 0; i < os.getNumSamples(); ++i)
                o[i] = softClip (o[i], p.clipCeil, p.clipKnee);
            oversampler->processSamplesDown (block);
        }

        if (p.limOn)
        {
            limiter.setThreshold (p.limCeilDb);
            limiter.setRelease (p.limRelMs);
            float* chans[] = { outL };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) len);
            limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
        }

        for (int i = 0; i < len; ++i)
        {
            const float y = juce::jlimit (-1.5f, 1.5f, outL[i] * sVol.getNextValue());
            outL[i] = y;
            outR[i] = y;
            if (hitWrite < (int) hitBuffer.size()) hitBuffer[(size_t) hitWrite++] = y;
        }
        hitLength.store (hitWrite);
    }

    KickParams p;
    double sr = 44100.0;
    int maxBlockSize = 512;
    std::array<Voice, 3> voices;
    int nextVoice = 0;

    std::vector<float> busBuf, rumBuf;
    std::vector<float> rumDelay;
    int rumWrite = 0;
    juce::Reverb rumReverb;
    Svf rumLp1, rumLp2;
    float rumHp = 0.f, duckEnv = 0.f, rumWindow = 0.f;
    int rumCountdown = 0;
    bool rumWasOn = false;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::Limiter<float> limiter;
    juce::SmoothedValue<float> sPre, sVol;
    int hitWrite = 0;
};
