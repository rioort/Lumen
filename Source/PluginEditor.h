#pragma once

#include "PluginProcessor.h"

namespace lumen::ui
{
struct Palette
{
    static inline const juce::Colour bg       { 0xff17181b };
    static inline const juce::Colour panel    { 0xff212328 };
    static inline const juce::Colour panelHi  { 0xff2b2e34 };
    static inline const juce::Colour text     { 0xffe9e4da };
    static inline const juce::Colour textDim  { 0xff8d8a84 };
    static inline const juce::Colour amber    { 0xfff0a64a };
    static inline const juce::Colour amberDim { 0xff6b4a24 };
};

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// Manopola con nome sopra e valore sotto
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& name, bool large);
    void resized() override;
    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Lampada + meter della riduzione di guadagno
class GainReductionDisplay : public juce::Component
{
public:
    void setValue (float grDb);
    void paint (juce::Graphics&) override;

private:
    float gr = 0.0f;
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (juce::String name) : title (std::move (name)) {}
    void setValue (float db) { level = db; repaint(); }
    void paint (juce::Graphics&) override;

private:
    juce::String title;
    float level = -100.0f;
};
} // namespace lumen::ui

//==============================================================================
class LumenAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit LumenAudioProcessorEditor (LumenAudioProcessor&);
    ~LumenAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateModeButtons (float value);

    LumenAudioProcessor& proc;
    lumen::ui::LookAndFeel lnf;

    lumen::ui::Knob threshold, ratio, attack, release, gain, tube, mix, scHpf;
    juce::TextButton modeButtons[3];
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;

    juce::TextButton linkButton { "Stereo Link" }, extButton { "Ext Sidechain" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttach, extAttach;

    lumen::ui::GainReductionDisplay grDisplay;
    lumen::ui::LevelMeter inMeter { "IN" }, outMeter { "OUT" };

    float grSmooth = 0.0f, inSmooth = -100.0f, outSmooth = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LumenAudioProcessorEditor)
};
