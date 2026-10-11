#include "PluginEditor.h"

using namespace lumen::ui;

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Palette::text);
    setColour (juce::TextButton::textColourOffId, Palette::textDim);
    setColour (juce::TextButton::textColourOnId, Palette::bg);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto r = bounds.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float radius = size * 0.5f;
    const float angle = start + pos * (end - start);

    // traccia dell'arco
    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, end, true);
    g.setColour (Palette::panelHi);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, angle, true);
    g.setColour (Palette::amber);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // corpo della manopola
    const float body = radius * 0.72f;
    juce::ColourGradient grad (juce::Colour (0xff3a3d44), c.x, c.y - body,
                               juce::Colour (0xff1c1d21), c.x, c.y + body, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - body, c.y - body, body * 2.0f, body * 2.0f, 1.0f);

    // indicatore
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -body + 3.0f, 3.0f, body * 0.45f, 1.5f);
    g.setColour (Palette::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                        bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Palette::amber : (highlighted ? Palette::panelHi : Palette::panel));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? Palette::amber : Palette::panelHi.brighter (0.2f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? Palette::bg : Palette::text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::FontOptions (12.0f));
    l->setColour (juce::Label::textColourId, Palette::text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
            const juce::String& name, bool large)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (large ? 260 : 220);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Palette::text);
    slider.setColour (juce::Slider::textBoxHighlightColourId, Palette::amber.withAlpha (0.4f));
    addAndMakeVisible (slider);

    label.setText (name.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, Palette::textDim);
    addAndMakeVisible (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
void GainReductionDisplay::setValue (float grDb)
{
    if (std::abs (grDb - gr) > 0.05f)
    {
        gr = grDb;
        repaint();
    }
}

void GainReductionDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 8.0f);

    // Lampada: si accende come la cella ottica quando comprime
    auto lampArea = r.removeFromTop (r.getHeight() * 0.58f).reduced (10.0f);
    const float glow = juce::jlimit (0.0f, 1.0f, gr / 12.0f);
    const auto lc = lampArea.getCentre();
    const float lr = juce::jmin (lampArea.getWidth(), lampArea.getHeight()) * 0.32f;

    juce::ColourGradient halo (Palette::amber.withAlpha (0.55f * glow), lc.x, lc.y,
                               Palette::amber.withAlpha (0.0f), lc.x + lr * 2.6f, lc.y, true);
    g.setGradientFill (halo);
    g.fillEllipse (lc.x - lr * 2.6f, lc.y - lr * 2.6f, lr * 5.2f, lr * 5.2f);

    g.setColour (Palette::amberDim.interpolatedWith (Palette::amber.brighter (0.4f), glow));
    g.fillEllipse (lc.x - lr, lc.y - lr, lr * 2.0f, lr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (lc.x - lr, lc.y - lr, lr * 2.0f, lr * 2.0f, 1.5f);

    // Barra di riduzione di guadagno (0..20 dB, da destra a sinistra)
    auto barArea = r.reduced (14.0f, 6.0f);
    auto label = barArea.removeFromTop (16.0f);
    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("GAIN REDUCTION", label, juce::Justification::centredLeft);
    g.setColour (Palette::text);
    g.drawText (juce::String (gr, 1) + " dB", label, juce::Justification::centredRight);

    auto bar = barArea.removeFromTop (12.0f);
    g.setColour (Palette::bg);
    g.fillRoundedRectangle (bar, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, gr / 20.0f);
    g.setColour (Palette::amber);
    g.fillRoundedRectangle (bar.withLeft (bar.getRight() - bar.getWidth() * frac), 3.0f);

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (10.0f));
    auto scale = barArea.removeFromTop (14.0f);
    for (int db : { 0, 5, 10, 15, 20 })
    {
        const float xp = bar.getRight() - bar.getWidth() * (float) db / 20.0f;
        g.drawText (juce::String (db), juce::Rectangle<float> (xp - 12.0f, scale.getY(), 24.0f, 14.0f),
                    juce::Justification::centred);
    }
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto lbl = r.removeFromBottom (16.0f);
    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (title, lbl, juce::Justification::centred);

    g.setColour (Palette::bg);
    g.fillRoundedRectangle (r, 3.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (level + 60.0f) / 66.0f); // -60..+6 dBFS
    auto fill = r.withTop (r.getBottom() - r.getHeight() * frac);
    g.setColour (level > -0.1f ? juce::Colour (0xffe0533d) : Palette::amber.withAlpha (0.85f));
    g.fillRoundedRectangle (fill, 3.0f);
    // tacca a 0 dBFS
    const float y0 = r.getBottom() - r.getHeight() * (60.0f / 66.0f);
    g.setColour (Palette::text.withAlpha (0.5f));
    g.drawHorizontalLine ((int) y0, r.getX(), r.getRight());
}

//==============================================================================
LumenAudioProcessorEditor::LumenAudioProcessorEditor (LumenAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      threshold (p.apvts, "threshold", "Threshold", true),
      ratio     (p.apvts, "ratio", "Ratio", true),
      attack    (p.apvts, "attack", "Attack", false),
      release   (p.apvts, "release", "Release", false),
      gain      (p.apvts, "gain", "Gain", true),
      tube      (p.apvts, "tube", "Tube", false),
      mix       (p.apvts, "mix", "Mix", false),
      scHpf     (p.apvts, "schpf", "SC Low Cut", false)
{
    setLookAndFeel (&lnf);

    for (auto* k : { &threshold, &ratio, &attack, &release, &gain, &tube, &mix, &scHpf })
        addAndMakeVisible (k);

    const char* names[] = { "FIXED", "MANUAL", "FIX/MAN" };
    auto* modeParam = p.apvts.getParameter ("mode");
    for (int i = 0; i < 3; ++i)
    {
        modeButtons[i].setButtonText (names[i]);
        modeButtons[i].setClickingTogglesState (false);
        modeButtons[i].onClick = [this, i] { if (modeAttachment) modeAttachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (modeButtons[i]);
    }
    modeAttachment = std::make_unique<juce::ParameterAttachment> (*modeParam,
        [this] (float v) { updateModeButtons (v); });
    modeAttachment->sendInitialUpdate();

    for (auto* b : { &linkButton, &extButton })
    {
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
    }
    linkAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "link", linkButton);
    extAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "scext", extButton);

    addAndMakeVisible (grDisplay);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    setSize (780, 400);
    startTimerHz (30);
}

LumenAudioProcessorEditor::~LumenAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void LumenAudioProcessorEditor::updateModeButtons (float value)
{
    const int m = juce::jlimit (0, 2, juce::roundToInt (value));
    for (int i = 0; i < 3; ++i)
        modeButtons[i].setToggleState (i == m, juce::dontSendNotification);
    // in modalita' Fixed le manopole dei tempi non hanno effetto
    attack.setAlpha (m == 0 ? 0.35f : 1.0f);
    release.setAlpha (m == 0 ? 0.35f : 1.0f);
}

void LumenAudioProcessorEditor::timerCallback()
{
    const float gr = proc.meterGrDb.exchange (0.0f);
    const float in = proc.meterInDb.exchange (-100.0f);
    const float out = proc.meterOutDb.exchange (-100.0f);

    // salita immediata, discesa morbida
    grSmooth = gr > grSmooth ? gr : grSmooth + (gr - grSmooth) * 0.25f;
    inSmooth = in > inSmooth ? in : inSmooth - 1.5f;
    outSmooth = out > outSmooth ? out : outSmooth - 1.5f;

    grDisplay.setValue (grSmooth);
    inMeter.setValue (inSmooth);
    outMeter.setValue (outSmooth);
}

void LumenAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bg);

    auto header = getLocalBounds().removeFromTop (56).reduced (24, 0);
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("LUMEN", header, juce::Justification::centredLeft);
    g.setColour (Palette::amber);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("OPTICAL TUBE COMPRESSOR", header.withTrimmedLeft (110), juce::Justification::centredLeft);

    // pannelli
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (juce::Rectangle<float> (16.0f, 60.0f, 540.0f, 324.0f), 10.0f);
    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("TIMING", juce::Rectangle<int> (32, 196, 200, 14), juce::Justification::centredLeft);
    g.drawText ("COLOUR & SIDECHAIN", juce::Rectangle<int> (300, 196, 220, 14), juce::Justification::centredLeft);
}

void LumenAudioProcessorEditor::resized()
{
    // riga principale: Threshold, Ratio, Gain
    threshold.setBounds (40, 72, 150, 120);
    ratio.setBounds (210, 72, 150, 120);
    gain.setBounds (380, 72, 150, 120);

    // tempi
    for (int i = 0; i < 3; ++i)
        modeButtons[i].setBounds (32 + i * 82, 214, 78, 24);
    attack.setBounds (40, 248, 110, 124);
    release.setBounds (150, 248, 110, 124);

    // colore e sidechain
    tube.setBounds (290, 214, 86, 100);
    mix.setBounds (380, 214, 86, 100);
    scHpf.setBounds (470, 214, 80, 100);
    linkButton.setBounds (300, 326, 120, 24);
    extButton.setBounds (424, 326, 120, 24);

    // lato destro: lampada + meter
    grDisplay.setBounds (572, 60, 192, 236);
    inMeter.setBounds (600, 306, 22, 80);
    outMeter.setBounds (712, 306, 22, 80);
}
