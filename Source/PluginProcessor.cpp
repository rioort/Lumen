#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    static const juce::String threshold = "threshold", ratio = "ratio", attack = "attack",
        release = "release", mode = "mode", gain = "gain", tube = "tube", mix = "mix",
        scHpf = "schpf", scExt = "scext", link = "link";
}

//==============================================================================
LumenAudioProcessor::LumenAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "LumenState", createLayout())
{
    pThreshold = apvts.getRawParameterValue (ids::threshold);
    pRatio     = apvts.getRawParameterValue (ids::ratio);
    pAttack    = apvts.getRawParameterValue (ids::attack);
    pRelease   = apvts.getRawParameterValue (ids::release);
    pMode      = apvts.getRawParameterValue (ids::mode);
    pGain      = apvts.getRawParameterValue (ids::gain);
    pTube      = apvts.getRawParameterValue (ids::tube);
    pMix       = apvts.getRawParameterValue (ids::mix);
    pScHpf     = apvts.getRawParameterValue (ids::scHpf);
    pScExt     = apvts.getRawParameterValue (ids::scExt);
    pLink      = apvts.getRawParameterValue (ids::link);
}

juce::AudioProcessorValueTreeState::ParameterLayout LumenAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dbText = [] (float v, int) { return String (v, 1) + " dB"; };
    auto msText = [] (float v, int) { return v < 10.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; };
    auto sText  = [] (float v, int) { return v < 1.0f ? String (roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; };

    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::threshold, 1 }, "Threshold",
        NormalisableRange<float> (-40.0f, 20.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::ratio, 1 }, "Ratio",
        skewed (2.0f, 10.0f, 4.0f), 4.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + ":1"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::attack, 1 }, "Attack",
        skewed (0.5f, 300.0f, 15.0f), 10.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (msText)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::release, 1 }, "Release",
        skewed (0.05f, 10.0f, 0.6f), 0.5f,
        AudioParameterFloatAttributes().withLabel ("s").withStringFromValueFunction (sText)));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::mode, 1 }, "Timing",
        StringArray { "Fixed", "Manual", "Fix/Man" }, 1));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::gain, 1 }, "Gain",
        NormalisableRange<float> (0.0f, 30.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (dbText)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::tube, 1 }, "Tube",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::scHpf, 1 }, "SC Low Cut",
        skewed (20.0f, 300.0f, 90.0f), 20.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
            { return v <= 20.5f ? String ("Off") : String (roundToInt (v)) + " Hz"; })));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::scExt, 1 }, "External SC", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::link, 1 }, "Stereo Link", true));

    return { p.begin(), p.end() };
}

//==============================================================================
bool LumenAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void LumenAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = juce::jmax (1, getMainBusNumOutputChannels());

    comp.prepare (sampleRate);

    // Oversampling 2x con filtri FIR a fase lineare (latenza compensata)
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numCh, 1,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    oversampling->reset();
    tube.prepare (sampleRate * (double) oversampling->getOversamplingFactor());

    const auto latency = oversampling->getLatencyInSamples();
    setLatencySamples (juce::roundToInt (latency));

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh };
    mixer.prepare (spec);
    mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    mixer.setWetLatency (latency);
    mixer.reset();

    makeup.reset (sampleRate, 0.02);
    makeup.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pGain->load()));

    gainBuffer.setSize (2, samplesPerBlock);
    scBuffer.setSize (2, samplesPerBlock);
}

void LumenAudioProcessor::releaseResources()
{
    if (oversampling != nullptr)
        oversampling->reset();
}

void LumenAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainBus = getBusBuffer (buffer, true, 0);
    const int numCh = juce::jmin (2, mainBus.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0 || oversampling == nullptr) return;

    // Il blocco puo' essere piu' grande di quanto annunciato: lo dividiamo
    if (n > gainBuffer.getNumSamples())
    {
        const int maxN = gainBuffer.getNumSamples();
        for (int start = 0; start < n; start += maxN)
        {
            const int len = juce::jmin (maxN, n - start);
            juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, len);
            juce::MidiBuffer dummy;
            processBlock (sub, dummy);
        }
        return;
    }

    // Pulizia ingresso da NaN/Inf e meter di ingresso
    float inPeak = 0.0f;
    for (int c = 0; c < numCh; ++c)
    {
        auto* d = mainBus.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            d[i] = lumen::sanitize (d[i]);
            inPeak = juce::jmax (inPeak, std::abs (d[i]));
        }
    }

    // Copia dei dati di ingresso per il mix dry/wet
    juce::dsp::AudioBlock<float> mainBlock (mainBus);
    mixer.setWetMixProportion (pMix->load() * 0.01f);
    mixer.pushDrySamples (mainBlock);

    // Sorgente del sidechain: interna o esterna
    const bool useExt = pScExt->load() > 0.5f && getBusCount (true) > 1 && getBus (true, 1)->isEnabled();
    int scCh = numCh;
    const float* scPtr[2] = { mainBus.getReadPointer (0), mainBus.getReadPointer (numCh > 1 ? 1 : 0) };
    if (useExt)
    {
        auto scBus = getBusBuffer (buffer, true, 1);
        if (scBus.getNumChannels() > 0)
        {
            scCh = juce::jmin (2, scBus.getNumChannels());
            for (int c = 0; c < scCh; ++c)
                scBuffer.copyFrom (c, 0, scBus, c, 0, n);
            scPtr[0] = scBuffer.getReadPointer (0);
            scPtr[1] = scBuffer.getReadPointer (scCh > 1 ? 1 : 0);
        }
    }

    lumen::CompParams params;
    params.thresholdKnob = pThreshold->load();
    params.ratio = pRatio->load();
    params.attackMs = pAttack->load();
    params.releaseS = pRelease->load();
    params.mode = (lumen::TimingMode) juce::jlimit (0, 2, (int) pMode->load());
    params.scHpfHz = pScHpf->load();
    params.link = pLink->load() > 0.5f;

    float* gp[2] = { gainBuffer.getWritePointer (0), gainBuffer.getWritePointer (1) };
    comp.process (scPtr, scCh, gp, numCh, n, params);

    // Riduzione di guadagno + gain di uscita (smussato)
    makeup.setTargetValue (juce::Decibels::decibelsToGain (pGain->load()));
    for (int i = 0; i < n; ++i)
    {
        const float mg = makeup.getNextValue();
        for (int c = 0; c < numCh; ++c)
            mainBus.getWritePointer (c)[i] *= gp[c][i] * mg;
    }

    // Stadio valvolare in oversampling
    {
        auto ioBlock = mainBlock.getSubsetChannelBlock (0, (size_t) numCh);
        auto osBlock = oversampling->processSamplesUp (ioBlock);
        float* osPtr[2] = { osBlock.getChannelPointer (0),
                            osBlock.getChannelPointer (osBlock.getNumChannels() > 1 ? 1 : 0) };
        tube.process (osPtr, (int) osBlock.getNumChannels(), (int) osBlock.getNumSamples(), pTube->load() * 0.01f);
        oversampling->processSamplesDown (ioBlock);
    }

    mixer.mixWetSamples (mainBlock);

    // Uscita: mai NaN, protezione a +12 dBFS
    float outPeak = 0.0f;
    for (int c = 0; c < numCh; ++c)
    {
        auto* d = mainBus.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            d[i] = juce::jlimit (-4.0f, 4.0f, lumen::sanitize (d[i]));
            outPeak = juce::jmax (outPeak, std::abs (d[i]));
        }
    }

    // Meter (con caduta morbida gestita dall'interfaccia)
    meterGrDb.store (juce::jmax (meterGrDb.load(), comp.takeMaxGainReductionDb()));
    meterInDb.store (juce::jmax (meterInDb.load(), juce::Decibels::gainToDecibels (inPeak, -100.0f)));
    meterOutDb.store (juce::jmax (meterOutDb.load(), juce::Decibels::gainToDecibels (outPeak, -100.0f)));
}

//==============================================================================
juce::AudioProcessorEditor* LumenAudioProcessor::createEditor()
{
    return new LumenAudioProcessorEditor (*this);
}

void LumenAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LumenAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LumenAudioProcessor();
}
