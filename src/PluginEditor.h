#pragma once
#include "PluginProcessor.h"
#include <array>
#include <memory>
#include <vector>

// Invisible click target for stage rows. The editor draws the row itself;
// an ordinary TextButton would add an unwanted rounded hover/press rectangle.
class GVocalStageHitButton final : public juce::TextButton
{
public:
    void paintButton(juce::Graphics&, bool, bool) override {}
};

// Small, centred ON/LOCK controls. Standard ToggleButton checkboxes paint
// relative to their widget's left side, causing drift/overlap in narrow rows.
// Drawing each square in its own fixed bounds fixes that alignment.
class GVocalMiniToggle final : public juce::ToggleButton
{
public:
    void setLockMode(bool b) { lockMode = b; repaint(); }

    void paintButton(juce::Graphics& g, bool hovered, bool pressed) override
    {
        auto square = getLocalBounds().toFloat().reduced(2.0f);
        const bool selected = getToggleState();
        const auto accent = lockMode ? juce::Colour::fromRGB(245, 196, 92)
                                     : juce::Colour::fromRGB(39, 225, 189);
        const auto background = juce::Colour::fromRGB(15, 29, 46);
        const auto frame = juce::Colour::fromRGB(58, 81, 108);

        g.setColour(selected ? accent.withAlpha(pressed ? 0.23f : 0.13f) : background);
        g.fillRoundedRectangle(square, 6.0f);
        g.setColour(selected ? accent.withAlpha(0.88f) : frame.brighter(hovered ? 0.35f : 0.0f));
        g.drawRoundedRectangle(square, 6.0f, 1.4f);
        if (selected)
        {
            g.setColour(accent);
            juce::Path check;
            check.startNewSubPath(square.getX() + square.getWidth() * 0.24f,
                                  square.getY() + square.getHeight() * 0.52f);
            check.lineTo(square.getX() + square.getWidth() * 0.43f,
                         square.getY() + square.getHeight() * 0.69f);
            check.lineTo(square.getX() + square.getWidth() * 0.76f,
                         square.getY() + square.getHeight() * 0.32f);
            g.strokePath(check, juce::PathStrokeType(2.5f,
                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
private:
    bool lockMode = false;
};

class GVocalAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GVocalAudioProcessorEditor(GVocalAudioProcessor&);
    ~GVocalAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectStage(int);
    void createParameterControls();
    void updateBypassLabel();
    void drawMeter(juce::Graphics&, juce::Rectangle<int>, float, const juce::String&, bool drawScale = false);
    void drawStageRow(juce::Graphics&, int, juce::Rectangle<int>);
    void drawStageIcon(juce::Graphics&, int, juce::Rectangle<float>, juce::Colour, bool active);
    juce::String stageSubtitle(int) const;
    juce::String stageDescription(int) const;
    juce::Colour stageAccent(int) const;
    juce::String prettyParamName(const juce::String&) const;
    float stageActivity(int) const;
    juce::String analysisTag() const;
    float getParamValue(const char* id) const;

    GVocalAudioProcessor& proc;
    juce::TextButton analyze { "ANALYZE" }, engine { "ENGINE ON" }, fix { "RESET STAGE" };
    juce::ComboBox styles;
    juce::Label status, subheader;
    std::array<GVocalStageHitButton, 8> stageButtons;
    std::array<GVocalMiniToggle, 8> stageEnabled;
    std::array<GVocalMiniToggle, 8> stageLocked;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 8> onAttachments, lockAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> engineAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment;
    std::vector<std::unique_ptr<juce::Slider>> controlSliders;
    std::vector<std::unique_ptr<juce::Label>> controlLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::ToggleButton> safetyToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> safetyAttachment;
    int selectedStage = 2;
    bool waitingForAnalysis = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GVocalAudioProcessorEditor)
};
