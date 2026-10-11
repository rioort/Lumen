#pragma once
// Lumen - motore DSP (C++ puro, nessuna dipendenza da JUCE, testabile da solo).
//
// Tutte le costanti di tempo sono espresse in secondi e convertite con il
// sample rate corrente: il comportamento e' identico a qualsiasi frequenza.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace lumen
{

constexpr float kPi = 3.14159265358979323846f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// Coefficiente di un filtro a un polo con costante di tempo "seconds"
inline float onePoleCoeff (double seconds, double sampleRate)
{
    if (seconds <= 0.0) return 0.0f;
    return (float) std::exp (-1.0 / (seconds * sampleRate));
}

inline float sanitize (float x)
{
    return std::isfinite (x) ? x : 0.0f;
}

//==============================================================================
// Biquad (RBJ) in forma trasposta diretta II
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    void setHighPass (double f, double q, double sr)
    {
        const double w = 2.0 * 3.14159265358979323846 * f / sr;
        const double c = std::cos (w), a = std::sin (w) / (2.0 * q);
        const double n = 1.0 + a;
        b0 = (float) ((1.0 + c) * 0.5 / n);
        b1 = (float) (-(1.0 + c) / n);
        b2 = b0;
        a1 = (float) (-2.0 * c / n);
        a2 = (float) ((1.0 - a) / n);
    }

    void setLowShelf (double f, double q, double gainDb, double sr)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * 3.14159265358979323846 * f / sr;
        const double c = std::cos (w), s = std::sin (w);
        const double al = s / (2.0 * q), sq = 2.0 * std::sqrt (A) * al;
        const double n = (A + 1) + (A - 1) * c + sq;
        b0 = (float) (A * ((A + 1) - (A - 1) * c + sq) / n);
        b1 = (float) (2 * A * ((A - 1) - (A + 1) * c) / n);
        b2 = (float) (A * ((A + 1) - (A - 1) * c - sq) / n);
        a1 = (float) (-2 * ((A - 1) + (A + 1) * c) / n);
        a2 = (float) (((A + 1) + (A - 1) * c - sq) / n);
    }

    void setLowPass (double f, double q, double sr)
    {
        const double w = 2.0 * 3.14159265358979323846 * f / sr;
        const double c = std::cos (w), a = std::sin (w) / (2.0 * q);
        const double n = 1.0 + a;
        b0 = (float) ((1.0 - c) * 0.5 / n);
        b1 = (float) ((1.0 - c) / n);
        b2 = b0;
        a1 = (float) (-2.0 * c / n);
        a2 = (float) ((1.0 - a) / n);
    }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        // protezione denormali e valori corrotti
        if (! std::isfinite (y)) { reset(); return 0.0f; }
        if (std::abs (z1) < 1.0e-20f) z1 = 0;
        if (std::abs (z2) < 1.0e-20f) z2 = 0;
        return y;
    }
};

//==============================================================================
enum class TimingMode { Fixed = 0, Manual = 1, FixedManual = 2 };

struct CompParams
{
    float thresholdKnob = 0.0f; // -40..+20 dB (0 = -18 dBFS)
    float ratio         = 4.0f; // 2..10
    float attackMs      = 10.0f;
    float releaseS      = 0.5f;
    TimingMode mode     = TimingMode::Manual;
    float scHpfHz       = 20.0f; // <= 20 = spento
    bool  link          = true;
};

//==============================================================================
// Calcolo della riduzione di guadagno con modello "ottico".
class OptoCompressor
{
public:
    static constexpr int maxChannels = 2;
    static constexpr float kneeDb = 12.0f;
    // 0 sulla manopola Threshold corrisponde a -18 dBFS (0 dBu)
    static constexpr float thresholdOffsetDb = -18.0f;
    static constexpr float maxGrDb = 40.0f;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        detCoeff = onePoleCoeff (0.002, sr);         // detector RMS ~2 ms
        smoothCoeff = onePoleCoeff (0.02, sr);       // smoothing parametri 20 ms
        fatigueUpCoeff = onePoleCoeff (2.0, sr);     // "memoria" della cella: carica
        fatigueDownCoeff = onePoleCoeff (8.0, sr);   // ...e scarica lenta
        lastHpf = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
            c = ChannelState{};
        sThreshold = -1000.0f; // forza l'inizializzazione al primo blocco
    }

    // Curva statica: riduzione di guadagno (dB, positiva) per livello in dBFS.
    // Ginocchio morbido; come sull'originale, la soglia e' il punto in cui la
    // riduzione raggiunge 1 dB.
    static float staticCurve (float levelDb, float thresholdDbfs, float ratio)
    {
        const float slope = 1.0f - 1.0f / ratio;
        const float W = kneeDb;
        // centro del ginocchio tale che GR(threshold) = 1 dB
        const float d = std::sqrt (2.0f * W / slope);
        const float t0 = (d <= W) ? thresholdDbfs + W * 0.5f - d
                                  : thresholdDbfs - 1.0f / slope; // (non accade con ratio >= 2)
        const float over = levelDb - t0;
        if (over <= -W * 0.5f) return 0.0f;
        if (over >= W * 0.5f)  return slope * over;
        const float k = over + W * 0.5f;
        return slope * k * k / (2.0f * W);
    }

    // Calcola il guadagno lineare per ogni campione e canale.
    // sc: segnale di detection (interno o sidechain esterno), numCh 1 o 2.
    // gainOut: buffer [numCh][n].
    void process (const float* const* sc, int numScCh, float* const* gainOut, int numOutCh,
                  int n, const CompParams& p)
    {
        if (sThreshold < -900.0f)
        {
            sThreshold = p.thresholdKnob;
            sRatio = p.ratio;
        }

        if (std::abs (p.scHpfHz - lastHpf) > 0.01f)
        {
            lastHpf = p.scHpfHz;
            for (auto& c : ch)
                c.hpf.setHighPass (std::max (10.0f, p.scHpfHz), 0.7071, sr);
        }
        const bool hpfOn = p.scHpfHz > 20.5f;

        const float atkUser = onePoleCoeff (p.attackMs * 0.001, sr);
        const float relUser = (float) p.releaseS;
        const float atkFixed = onePoleCoeff (0.001, sr);  // FIXED: 1 ms
        const float relFixed = 0.05f;                     // FIXED: 50 ms

        const bool linked = p.link && numScCh > 1;

        for (int i = 0; i < n; ++i)
        {
            sThreshold = p.thresholdKnob + (sThreshold - p.thresholdKnob) * smoothCoeff;
            sRatio = p.ratio + (sRatio - p.ratio) * smoothCoeff;
            const float thrDbfs = sThreshold + thresholdOffsetDb;

            float lev[maxChannels] = { -200.0f, -200.0f };
            for (int c = 0; c < numScCh && c < maxChannels; ++c)
            {
                float x = std::clamp (sanitize (sc[c][i]), -16.0f, 16.0f);
                if (hpfOn) x = ch[(size_t) c].hpf.process (x);
                auto& ms = ch[(size_t) c].ms;
                ms = x * x + (ms - x * x) * detCoeff;
                if (ms < 1.0e-20f) ms = 0.0f;
                // +3 dB: un seno a 0 dBFS di picco legge ~0 dB
                lev[c] = 10.0f * std::log10 (ms + 1.0e-12f) + 3.0103f;
            }
            if (linked)
                lev[0] = lev[1] = std::max (lev[0], lev[1]);
            if (numScCh == 1)
                lev[1] = lev[0];

            for (int c = 0; c < numOutCh && c < maxChannels; ++c)
            {
                auto& s = ch[(size_t) (linked ? 0 : c)];
                // con link attivo il canale 0 calcola, il canale 1 copia
                if (linked && c == 1)
                {
                    gainOut[1][i] = gainOut[0][i];
                    continue;
                }

                const float target = std::min (maxGrDb, staticCurve (lev[c], thrDbfs, sRatio));

                // Comportamento "ottico": rilascio piu' rapido dalle riduzioni
                // profonde e coda piu' lenta vicino a 0 dB; la cella si "stanca"
                // dopo compressioni forti e prolungate e rilascia ancora piu' piano.
                const float optoMult = (0.6f + 1.4f * std::exp (-s.grSlow / 8.0f))
                                       * (1.0f + 1.5f * s.fatigue);

                float gr = 0.0f;
                switch (p.mode)
                {
                    case TimingMode::Fixed:
                        gr = ballistics (s.grSlow, target, atkFixed, relFixed * optoMult);
                        s.grFast = 0.0f; s.overTime = 0.0f;
                        break;
                    case TimingMode::Manual:
                        gr = ballistics (s.grSlow, target, atkUser, relUser * optoMult);
                        s.grFast = 0.0f; s.overTime = 0.0f;
                        break;
                    case TimingMode::FixedManual:
                    {
                        // Come sull'originale: attacco sempre veloce. I picchi brevi
                        // rilasciano in modo rapido (50 ms); se il segnale resta oltre
                        // la soglia piu' a lungo del tempo di Attack (qui un ritardo),
                        // subentra il rilascio manuale.
                        if (target > 0.5f) s.overTime += 1.0f / (float) sr;
                        else               s.overTime = 0.0f;

                        const float multFast = 0.6f + 1.4f * std::exp (-s.grFast / 8.0f);
                        const float f = ballistics (s.grFast, target, atkFixed, relFixed * multFast);
                        const bool sustained = s.overTime >= p.attackMs * 0.001f;
                        const float slowTarget = sustained ? target : std::min (target, s.grSlow);
                        const float sl = ballistics (s.grSlow, slowTarget, atkFixed, relUser * optoMult);
                        gr = std::max (f, sl);
                        break;
                    }
                }

                const float ft = std::min (1.0f, gr / 12.0f);
                s.fatigue = ft + (s.fatigue - ft) * (ft > s.fatigue ? fatigueUpCoeff : fatigueDownCoeff);

                gr = std::max (0.0f, sanitize (gr));
                gainOut[c][i] = dbToGain (-gr);
                if (gr > blockMaxGr) blockMaxGr = gr;
            }
        }
    }

    // Massima riduzione di guadagno dall'ultima lettura (per il meter)
    float takeMaxGainReductionDb()
    {
        const float g = blockMaxGr;
        blockMaxGr = 0.0f;
        return g;
    }

private:
    struct ChannelState
    {
        Biquad hpf;
        float ms = 0.0f;
        float grSlow = 0.0f;
        float grFast = 0.0f;
        float fatigue = 0.0f;
        float overTime = 0.0f;
    };

    float ballistics (float& state, float target, float atkCoeff, float releaseSeconds)
    {
        const float coeff = target > state ? atkCoeff : onePoleCoeff (releaseSeconds, sr);
        state = target + (state - target) * coeff;
        if (state < 1.0e-6f) state = 0.0f;
        return state;
    }

    double sr = 44100.0;
    float detCoeff = 0, smoothCoeff = 0, fatigueUpCoeff = 0, fatigueDownCoeff = 0;
    float sThreshold = 0, sRatio = 4;
    float lastHpf = -1.0f;
    float blockMaxGr = 0.0f;
    std::array<ChannelState, maxChannels> ch {};
};

//==============================================================================
// Stadio di uscita valvolare + trasformatore. Va eseguito al sample rate
// sovracampionato (oversampling) per evitare aliasing.
class TubeStage
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        smoothCoeff = onePoleCoeff (0.02, sr);
        for (auto& c : ch)
        {
            c.dc.setHighPass (5.0, 0.7071, sr);          // DC blocker
            c.shelf.setLowShelf (60.0, 0.6, 0.8, sr);    // "peso" del trasformatore
            c.top.setLowPass (std::min (32000.0, sr * 0.45), 0.6, sr); // morbidezza in alto
        }
        reset();
    }

    void reset()
    {
        for (auto& c : ch) { c.dc.reset(); c.shelf.reset(); c.top.reset(); }
        sAmount = -1.0f;
    }

    // amount 0..1
    void process (float* const* data, int numCh, int n, float amount)
    {
        if (sAmount < 0.0f) sAmount = amount;
        for (int i = 0; i < n; ++i)
        {
            sAmount = amount + (sAmount - amount) * smoothCoeff;
            const float a = sAmount;
            const float k = 0.05f + 1.0f * a;     // drive dello stadio
            const float b = 0.30f * a;            // asimmetria -> 2a armonica
            const float tb = std::tanh (k * b);
            const float norm = 1.0f / (k * (1.0f - tb * tb)); // guadagno unitario a basso livello

            for (int c = 0; c < numCh && c < 2; ++c)
            {
                const float x = sanitize (data[c][i]);
                float y = (std::tanh (k * (x + b)) - tb) * norm;
                y = ch[(size_t) c].dc.process (y);
                const float colored = ch[(size_t) c].top.process (ch[(size_t) c].shelf.process (y));
                y = y + (colored - y) * a;  // il colore cresce con "Tube"
                // a Tube = 0 il segnale passa pulito
                data[c][i] = x + (y - x) * std::min (1.0f, a * 4.0f);
            }
        }
    }

private:
    struct ChannelState { Biquad dc, shelf, top; };
    double sr = 88200.0;
    float smoothCoeff = 0, sAmount = -1.0f;
    std::array<ChannelState, 2> ch {};
};

} // namespace lumen
