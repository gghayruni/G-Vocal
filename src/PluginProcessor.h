#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

// G-Vocal v0.1: original CPU-only DSP; no AI model, no RX/Nectar dependencies.
class GVocalAudioProcessor final : public juce::AudioProcessor
{
public:
    GVocalAudioProcessor();
    ~GVocalAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "G-Vocal"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputPeakDb { -100.0f }, outputPeakDb { -100.0f };
    std::atomic<bool> analysisRequested { false }, analysisReady { false };
    std::atomic<float> analysisRmsDb { -30.0f }, analysisPeakDb { -15.0f }, analysisBrightness { 0.0f };
    std::atomic<float> analysisProgress { 0.0f };

    void beginAnalysis() noexcept;
    void applyPreset(int style, int onlyStage = -1, bool respectLocks = true);
    void applyAnalysis(int style);
    static const char* stageName(int index) noexcept;
    static const juce::StringArray& stageParams(int index);
    static constexpr int stageCount = 8;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    float value(const char* id) const noexcept;
    bool enabled(int i) const noexcept;
    bool locked(int i) const noexcept;
    void setValue(const char* id, float v);

    struct Biquad
    {
        float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
        void reset() noexcept { z1=z2=0; }
        float process(float x) noexcept
        {
            const auto y=b0*x+z1;
            z1=b1*x-a1*y+z2;
            z2=b2*x-a2*y;
            return y;
        }
        void lowShelf(double fs, float hz, float db) noexcept;
        void highShelf(double fs, float hz, float db) noexcept;
        void peak(double fs, float hz, float db) noexcept;
        void highPass(double fs, float hz) noexcept;
        void coefficients(float n0,float n1,float n2,float d0,float d1,float d2) noexcept;
    };
    std::array<Biquad,2> hp, low, mid, high;
    std::array<float,2> gateEnv {}, gateGain {1.f,1.f}, compEnv {}, compGain {1.f,1.f};
    std::array<float,2> essLow {}, essEnv {};
    std::array<std::vector<float>,2> delayBuffer;
    juce::AudioBuffer<float> dryBuffer;
    juce::Reverb reverb;
    double sr=44100.0;
    int delayWrite=0;
    float engineMix=1.f;

    // The audio thread owns sums/counters; the UI reads the atomically published result.
    uint64_t analyzedSamples=0;
    double analyzedSquares=0, analyzedBright=0, analyzedLevel=0;
    float analyzedPeak=0, previousInput=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GVocalAudioProcessor)
};
