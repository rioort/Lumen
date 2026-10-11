#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "LumenDsp.h"

class LumenAudioProcessor : public juce::AudioProcessor
{
public:
    LumenAudioProcessor();
    ~LumenAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Meter letti dall'interfaccia
    std::atomic<float> meterGrDb { 0.0f };
    std::atomic<float> meterInDb { -100.0f };
    std::atomic<float> meterOutDb { -100.0f };

private:
    lumen::OptoCompressor comp;
    lumen::TubeStage tube;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DryWetMixer<float> mixer { 256 };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> makeup;

    juce::AudioBuffer<float> gainBuffer;
    juce::AudioBuffer<float> scBuffer;

    std::atomic<float>* pThreshold = nullptr;
    std::atomic<float>* pRatio = nullptr;
    std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pGain = nullptr;
    std::atomic<float>* pTube = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pScHpf = nullptr;
    std::atomic<float>* pScExt = nullptr;
    std::atomic<float>* pLink = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenAudioProcessor)
};
