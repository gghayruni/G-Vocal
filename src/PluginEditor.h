#pragma once
#include "PluginProcessor.h"
#include <array>
#include <memory>
#include <vector>

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
    std::array<juce::TextButton, 8> stageButtons;
    std::array<juce::ToggleButton, 8> stageEnabled;
    std::array<juce::ToggleButton, 8> stageLocked;
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
