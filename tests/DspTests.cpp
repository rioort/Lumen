// Test offline del motore DSP di Lumen (nessuna DAW necessaria).
#include "LumenDsp.h"
#include <cstdio>
#include <vector>
#include <cmath>

using namespace lumen;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("  FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

// Esegue il compressore su un segnale stereo e restituisce la GR (dB) per campione (canale 0)
static std::vector<float> runComp (double sr, const std::vector<float>& sig, const CompParams& p)
{
    OptoCompressor c;
    c.prepare (sr);
    std::vector<float> g0 (sig.size()), g1 (sig.size()), out (sig.size());
    const int block = 128;
    for (size_t s = 0; s < sig.size(); s += block)
    {
        const int n = (int) std::min<size_t> (block, sig.size() - s);
        const float* in[2] = { sig.data() + s, sig.data() + s };
        float* g[2] = { g0.data() + s, g1.data() + s };
        c.process (in, 2, g, 2, n, p);
    }
    for (size_t i = 0; i < sig.size(); ++i) out[i] = -gainToDb (g0[i]);
    return out;
}

static std::vector<float> sine (double sr, double seconds, float amp, double f = 1000.0)
{
    std::vector<float> v ((size_t) (sr * seconds));
    for (size_t i = 0; i < v.size(); ++i) v[i] = amp * (float) std::sin (2.0 * 3.14159265358979 * f * (double) i / sr);
    return v;
}

int main()
{
    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };

    std::printf ("1) Curva statica\n");
    for (float r : { 2.0f, 4.0f, 10.0f })
    {
        const float atT = OptoCompressor::staticCurve (-18.0f, -18.0f, r);
        CHECK (std::abs (atT - 1.0f) < 1e-3f, "GR alla soglia = %.3f dB (ratio %.0f)", atT, r);
        const float a = OptoCompressor::staticCurve (0.0f, -18.0f, r), b = OptoCompressor::staticCurve (10.0f, -18.0f, r);
        CHECK (std::abs ((b - a) - 10.0f * (1.0f - 1.0f / r)) < 1e-3f, "pendenza sopra il ginocchio errata (ratio %.0f)", r);
        CHECK (OptoCompressor::staticCurve (-40.0f, -18.0f, r) == 0.0f, "GR sotto il ginocchio non nulla");
        std::printf ("   ratio %4.0f:1  GR@soglia %.2f dB, GR con +18 dB sopra %.2f dB\n", r, atT, a);
    }

    std::printf ("2) GR a regime uguale a tutti i sample rate (seno 1 kHz a -6 dBFS, soglia -10)\n");
    CompParams p; p.thresholdKnob = -10.0f; p.ratio = 4.0f; p.attackMs = 10.0f; p.releaseS = 0.3f; p.mode = TimingMode::Manual;
    std::vector<float> steady;
    for (double sr : rates)
    {
        auto gr = runComp (sr, sine (sr, 1.0, 0.5f), p);
        steady.push_back (gr.back());
        std::printf ("   %6.0f Hz: GR %.2f dB\n", sr, gr.back());
    }
    for (float v : steady) CHECK (std::abs (v - steady[0]) < 0.15f, "GR diversa tra sample rate: %.2f vs %.2f", v, steady[0]);

    std::printf ("3) Tempo di attacco indipendente dal sample rate (Attack 10 ms)\n");
    std::vector<double> t63;
    for (double sr : rates)
    {
        auto gr = runComp (sr, sine (sr, 0.5, 0.5f), p);
        const float target = gr.back() * 0.63f;
        size_t i = 0; while (i < gr.size() && gr[i] < target) ++i;
        t63.push_back (1000.0 * (double) i / sr);
        std::printf ("   %6.0f Hz: 63%% raggiunto in %.2f ms\n", sr, t63.back());
    }
    for (double t : t63) CHECK (std::abs (t - t63[0]) < 0.5, "attacco diverso tra sample rate: %.2f vs %.2f ms", t, t63[0]);

    std::printf ("4) Modalita' FIX/MAN: picco breve rilascia veloce, segnale lungo rilascia lento\n");
    for (double sr : { 48000.0, 96000.0 })
    {
        CompParams q = p; q.mode = TimingMode::FixedManual; q.attackMs = 100.0f; q.releaseS = 2.0f; q.thresholdKnob = -20.0f;
        auto make = [&] (double burst) { auto v = sine (sr, burst, 0.7f); v.resize ((size_t) (sr * (burst + 0.3)), 0.0f); return v; };
        auto shortGr = runComp (sr, make (0.005), q);
        auto longGr  = runComp (sr, make (0.6), q);
        const float shortAfter = shortGr[(size_t) (sr * (0.005 + 0.25))];
        const float longAfter  = longGr [(size_t) (sr * (0.6 + 0.25))];
        std::printf ("   %6.0f Hz: 250 ms dopo un picco breve GR %.2f dB, dopo un segnale lungo GR %.2f dB\n", sr, shortAfter, longAfter);
        CHECK (shortAfter < 0.1f * longAfter, "il picco breve non rilascia piu' velocemente del segnale lungo");
        CHECK (longAfter > 3.0f, "il segnale lungo non usa il rilascio manuale");
    }

    std::printf ("5) Robustezza: NaN, Inf, silenzio, valori enormi\n");
    {
        std::vector<float> bad (48000 * 4, 0.0f);
        bad[100] = NAN; bad[200] = INFINITY; bad[300] = -INFINITY; bad[400] = 1.0e6f;
        auto gr = runComp (48000.0, bad, p);
        bool finite = true; for (float v : gr) finite = finite && std::isfinite (v);
        CHECK (finite, "valori non finiti nella GR");
        CHECK (gr.back() < 0.05f, "la GR non torna a zero nel silenzio (%.3f)", gr.back());
        std::printf ("   GR sempre finita: %s, GR finale %.4f dB\n", finite ? "si" : "NO", gr.back());
    }

    std::printf ("6) Stadio valvolare (al sample rate sovracampionato 96 kHz)\n");
    {
        auto measure = [] (float amount, float amp, float& gainDb, float& h2Db, float& h3Db)
        {
            const double sr = 96000.0; TubeStage t; t.prepare (sr);
            auto l = sine (sr, 1.0, amp, 1000.0), r = l;
            float* d[2] = { l.data(), r.data() };
            for (size_t s = 0; s < l.size(); s += 256) { float* b[2] = { d[0] + s, d[1] + s }; t.process (b, 2, (int) std::min<size_t> (256, l.size() - s), amount); }
            // analisi DFT su 0.5 s finali
            auto bin = [&] (double f) { double re = 0, im = 0; const size_t st = l.size() / 2;
                for (size_t i = st; i < l.size(); ++i) { const double ph = 2.0 * 3.14159265358979 * f * (double) i / sr; re += l[i] * std::cos (ph); im += l[i] * std::sin (ph); }
                return 2.0 * std::sqrt (re * re + im * im) / (double) (l.size() - st); };
            const double f1 = bin (1000.0), f2 = bin (2000.0), f3 = bin (3000.0);
            gainDb = (float) (20.0 * std::log10 (f1 / amp));
            h2Db = (float) (20.0 * std::log10 (f2 / f1 + 1e-12));
            h3Db = (float) (20.0 * std::log10 (f3 / f1 + 1e-12));
        };
        float g, h2, h3;
        measure (0.0f, 0.5f, g, h2, h3);
        std::printf ("   Tube 0%%:   guadagno %.2f dB, 2a armonica %.1f dB, 3a %.1f dB\n", g, h2, h3);
        CHECK (h2 < -100.0f && std::abs (g) < 0.01f, "Tube 0%% non e' trasparente");
        measure (1.0f, 0.5f, g, h2, h3);
        std::printf ("   Tube 100%%: guadagno %.2f dB, 2a armonica %.1f dB, 3a %.1f dB\n", g, h2, h3);
        CHECK (h2 > -40.0f && h2 < -20.0f, "seconda armonica fuori range");
        CHECK (h2 > h3, "la seconda armonica dovrebbe dominare sulla terza");
        CHECK (std::abs (g) < 1.5f, "guadagno a 1 kHz troppo diverso da 0 dB");
        measure (1.0f, 0.01f, g, h2, h3);
        std::printf ("   Tube 100%% a -40 dBFS: guadagno %.2f dB, 2a armonica %.1f dB\n", g, h2);
        CHECK (std::abs (g) < 0.3f, "guadagno a basso livello non unitario");
    }

    std::printf (failures == 0 ? "\nTUTTI I TEST SUPERATI\n" : "\n%d TEST FALLITI\n", failures);
    return failures == 0 ? 0 : 1;
}
