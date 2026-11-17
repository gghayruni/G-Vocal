#include "PluginEditor.h"
#include <cmath>
#include <initializer_list>

namespace
{
const juce::Colour bgTop      = juce::Colour::fromRGB(8, 16, 30);
const juce::Colour bgBottom   = juce::Colour::fromRGB(5, 11, 22);
const juce::Colour panel      = juce::Colour::fromRGB(16, 29, 46);
const juce::Colour panel2     = juce::Colour::fromRGB(20, 35, 56);
const juce::Colour border     = juce::Colour::fromRGB(44, 66, 92);
const juce::Colour borderSoft = juce::Colour::fromRGB(27, 45, 68);
const juce::Colour mint       = juce::Colour::fromRGB(39, 225, 189);
const juce::Colour mintSoft   = juce::Colour::fromRGB(28, 177, 152);
const juce::Colour text       = juce::Colour::fromRGB(235, 243, 252);
const juce::Colour muted      = juce::Colour::fromRGB(152, 171, 198);
const juce::Colour yellow     = juce::Colour::fromRGB(245, 196, 92);
const juce::Colour purple     = juce::Colour::fromRGB(154, 140, 255);
const juce::Colour orange     = juce::Colour::fromRGB(245, 164, 84);
const juce::Colour blue       = juce::Colour::fromRGB(84, 166, 255);

juce::Colour withGlow(juce::Colour c, float alpha) { return c.withAlpha(alpha); }

float average(std::initializer_list<float> values)
{
    float total = 0.0f;
    int count = 0;
    for (auto v : values)
    {
        total += v;
        ++count;
    }
    return count > 0 ? total / (float) count : 0.0f;
}
}

GVocalAudioProcessorEditor::GVocalAudioProcessorEditor(GVocalAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setSize(1080, 720);

    styles.addItemList(juce::StringArray{ "NATURAL", "POP", "BALLAD", "WARM", "POWERFUL" }, 1);
    addAndMakeVisible(styles);
    styles.setColour(juce::ComboBox::backgroundColourId, panel2);
    styles.setColour(juce::ComboBox::textColourId, text);
    styles.setColour(juce::ComboBox::outlineColourId, border);
    styles.setColour(juce::ComboBox::arrowColourId, text);
    styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.parameters, "style", styles);
    styles.onChange = [this] { proc.applyPreset(styles.getSelectedItemIndex()); };

    addAndMakeVisible(analyze);
    analyze.setColour(juce::TextButton::buttonColourId, mint);
    analyze.setColour(juce::TextButton::textColourOffId, bgTop);
    analyze.onClick = [this]
    {
        waitingForAnalysis = true;
        proc.beginAnalysis();
        status.setText("Analyzing for 7 seconds... play a vocal track in Ableton.", juce::dontSendNotification);
    };

    addAndMakeVisible(engine);
    engine.setClickingTogglesState(true);
    engine.setColour(juce::TextButton::buttonColourId, panel2);
    engine.setColour(juce::TextButton::textColourOffId, text);
    engineAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.parameters, "engine", engine);
    engine.onClick = [this] { updateBypassLabel(); repaint(); };
    updateBypassLabel();

    addAndMakeVisible(fix);
    fix.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(34, 50, 74));
    fix.setColour(juce::TextButton::textColourOffId, text);
    fix.onClick = [this]
    {
        const int style = juce::jlimit(0, 4, styles.getSelectedItemIndex());
        proc.applyPreset(style, selectedStage, false);
        status.setText("Selected stage was reset to the active preset.", juce::dontSendNotification);
    };

    subheader.setText("8 PROCESSING STAGES  /  LIVE INDICATORS  /  ON-OFF  /  LOCK  /  MANUAL CONTROL", juce::dontSendNotification);
    subheader.setColour(juce::Label::textColourId, muted);
    subheader.setFont(juce::Font(12.5f));
    addAndMakeVisible(subheader);

    status.setText("Hybrid mode | Press ANALYZE and play your vocal.", juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, muted);
    status.setFont(juce::Font(13.0f));
    addAndMakeVisible(status);

    for (int i = 0; i < 8; ++i)
    {
        auto& b = stageButtons[(size_t) i];
        b.setButtonText({});
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::transparentBlack);
        b.onClick = [this, i] { selectStage(i); };
        addAndMakeVisible(b);

        auto& on = stageEnabled[(size_t) i];
        on.setButtonText({});
        on.setColour(juce::ToggleButton::tickColourId, mint);
        on.setColour(juce::ToggleButton::tickDisabledColourId, border);
        on.setColour(juce::ToggleButton::textColourId, muted);
        addAndMakeVisible(on);
        onAttachments[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.parameters, "on" + juce::String(i), on);

        auto& lock = stageLocked[(size_t) i];
        lock.setButtonText({});
        lock.setColour(juce::ToggleButton::tickColourId, yellow);
        lock.setColour(juce::ToggleButton::tickDisabledColourId, border);
        lock.setColour(juce::ToggleButton::textColourId, muted);
        addAndMakeVisible(lock);
        lockAttachments[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.parameters, "lock" + juce::String(i), lock);
    }

    selectStage(selectedStage);
    startTimerHz(20);
}

GVocalAudioProcessorEditor::~GVocalAudioProcessorEditor() { stopTimer(); }

float GVocalAudioProcessorEditor::getParamValue(const char* id) const
{
    if (auto* raw = proc.parameters.getRawParameterValue(id))
        return raw->load();
    return 0.0f;
}

juce::Colour GVocalAudioProcessorEditor::stageAccent(int index) const
{
    switch (index)
    {
        case 0: return juce::Colour::fromRGB(76, 239, 143);
        case 1: return juce::Colour::fromRGB(82, 192, 255);
        case 2: return mint;
        case 3: return juce::Colour::fromRGB(69, 235, 255);
        case 4: return orange;
        case 5: return purple;
        case 6: return juce::Colour::fromRGB(120, 155, 255);
        case 7: return juce::Colour::fromRGB(56, 242, 199);
        default: return mint;
    }
}

juce::String GVocalAudioProcessorEditor::stageSubtitle(int index) const
{
    static const char* subtitles[] =
    {
        "Noise gate / cleanup",
        "Adaptive EQ (3-band)",
        "Dynamic range control",
        "Reduce sibilance",
        "Harmonic color",
        "Space and ambience",
        "Echo and movement",
        "Level and safety"
    };
    return subtitles[juce::jlimit(0, 7, index)];
}

juce::String GVocalAudioProcessorEditor::stageDescription(int index) const
{
    static const char* descriptions[] =
    {
        "Clean up the signal and suppress low-level noise before tonal processing.",
        "Shape body, presence and air so the vocal sits better in the mix.",
        "Control peaks and make the performance more even and stable.",
        "Tame harsh S, SH and CH sounds without making the voice dull.",
        "Add gentle harmonic richness and extra character to the vocal.",
        "Place the vocal into a room-like space with controlled ambience.",
        "Add repeat, depth and rhythmic movement behind the dry voice.",
        "Set final output level and optional sample peak safety soft clip."
    };
    return descriptions[juce::jlimit(0, 7, index)];
}

juce::String GVocalAudioProcessorEditor::prettyParamName(const juce::String& id) const
{
    if (id == "lowCut") return "LOW CUT";
    if (id == "gate") return "GATE";
    if (id == "body") return "BODY";
    if (id == "presence") return "PRESENCE";
    if (id == "air") return "AIR";
    if (id == "compThresh") return "THRESHOLD";
    if (id == "ratio") return "RATIO";
    if (id == "attack") return "ATTACK";
    if (id == "release") return "RELEASE";
    if (id == "essThresh") return "ESS THRESHOLD";
    if (id == "essReduction") return "REDUCTION";
    if (id == "drive") return "DRIVE";
    if (id == "satMix") return "MIX";
    if (id == "room") return "ROOM";
    if (id == "revWet") return "WET";
    if (id == "damping") return "DAMPING";
    if (id == "delayTime") return "TIME";
    if (id == "feedback") return "FEEDBACK";
    if (id == "delayMix") return "MIX";
    if (id == "outputGain") return "OUTPUT GAIN";
    return id.toUpperCase();
}

float GVocalAudioProcessorEditor::stageActivity(int index) const
{
    switch (index)
    {
        case 0:
            return juce::jlimit(0.0f, 1.0f, average({ (getParamValue("lowCut") - 50.0f) / 130.0f, (70.0f + getParamValue("gate")) / 50.0f }));
        case 1:
            return juce::jlimit(0.0f, 1.0f, average({ std::abs(getParamValue("body")) / 9.0f, std::abs(getParamValue("presence")) / 9.0f, std::abs(getParamValue("air")) / 9.0f }));
        case 2:
            return juce::jlimit(0.0f, 1.0f, average({ (42.0f + getParamValue("compThresh")) / 39.0f, (getParamValue("ratio") - 1.0f) / 7.0f }));
        case 3:
            return juce::jlimit(0.0f, 1.0f, average({ (50.0f + getParamValue("essThresh")) / 42.0f, getParamValue("essReduction") / 12.0f }));
        case 4:
            return juce::jlimit(0.0f, 1.0f, average({ getParamValue("drive") / 15.0f, getParamValue("satMix") / 100.0f }));
        case 5:
            return juce::jlimit(0.0f, 1.0f, average({ getParamValue("room") / 100.0f, getParamValue("revWet") / 60.0f }));
        case 6:
            return juce::jlimit(0.0f, 1.0f, average({ (getParamValue("delayTime") - 60.0f) / 840.0f, getParamValue("feedback") / 65.0f, getParamValue("delayMix") / 50.0f }));
        case 7:
            return juce::jlimit(0.0f, 1.0f, average({ (getParamValue("outputGain") + 18.0f) / 30.0f, getParamValue("safety") > 0.5f ? 1.0f : 0.0f }));
        default:
            return 0.0f;
    }
}

juce::String GVocalAudioProcessorEditor::analysisTag() const
{
    const float bright = proc.analysisBrightness.load();
    const float rms = proc.analysisRmsDb.load();

    juce::String tone = "Balanced vocal";
    if (bright < 0.06f) tone = "Warm / dark vocal";
    else if (bright > 0.32f) tone = "Bright vocal";

    if (rms > -18.0f) tone << " | strong";
    else if (rms < -28.0f) tone << " | soft";
    else tone << " | medium";
    return tone;
}

void GVocalAudioProcessorEditor::selectStage(int i)
{
    selectedStage = juce::jlimit(0, 7, i);
    createParameterControls();
    resized();
    repaint();
}

void GVocalAudioProcessorEditor::createParameterControls()
{
    sliderAttachments.clear();
    controlLabels.clear();
    controlSliders.clear();
    safetyAttachment.reset();
    safetyToggle.reset();

    for (auto id : GVocalAudioProcessor::stageParams(selectedStage))
    {
        if (id == "safety")
        {
            safetyToggle = std::make_unique<juce::ToggleButton>("SAMPLE PEAK SAFETY (SOFT CLIP)");
            safetyToggle->setColour(juce::ToggleButton::textColourId, text);
            safetyToggle->setColour(juce::ToggleButton::tickColourId, mint);
            safetyToggle->setColour(juce::ToggleButton::tickDisabledColourId, border);
            addAndMakeVisible(*safetyToggle);
            safetyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.parameters, id, *safetyToggle);
            continue;
        }

        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 88, 26);
        slider->setColour(juce::Slider::trackColourId, mint);
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        slider->setColour(juce::Slider::textBoxTextColourId, text);
        slider->setColour(juce::Slider::textBoxOutlineColourId, border);
        slider->setColour(juce::Slider::textBoxBackgroundColourId, panel2);
        slider->setColour(juce::Slider::backgroundColourId, borderSoft);
        slider->setNumDecimalPlacesToDisplay(1);
        addAndMakeVisible(*slider);

        auto label = std::make_unique<juce::Label>();
        label->setText(prettyParamName(id), juce::dontSendNotification);
        label->setColour(juce::Label::textColourId, text);
        label->setFont(juce::Font(14.0f, juce::Font::bold));
        addAndMakeVisible(*label);

        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.parameters, id, *slider));
        controlSliders.push_back(std::move(slider));
        controlLabels.push_back(std::move(label));
    }
}

void GVocalAudioProcessorEditor::updateBypassLabel()
{
    const auto* raw = proc.parameters.getRawParameterValue("engine");
    const bool on = raw && raw->load() >= 0.5f;
    engine.setButtonText(on ? "ENGINE ON" : "BYPASS");
}

void GVocalAudioProcessorEditor::timerCallback()
{
    updateBypassLabel();

    if (waitingForAnalysis)
    {
        if (proc.analysisReady.exchange(false, std::memory_order_acquire))
        {
            waitingForAnalysis = false;
            proc.applyAnalysis(juce::jlimit(0, 4, styles.getSelectedItemIndex()));
            status.setText("Analysis complete | RMS " + juce::String(proc.analysisRmsDb.load(), 1)
                             + " dBFS | Peak " + juce::String(proc.analysisPeakDb.load(), 1)
                             + " dBFS | chain updated",
                           juce::dontSendNotification);
        }
        else
        {
            status.setText("ANALYSIS: " + juce::String((int) (100 * proc.analysisProgress.load()))
                             + "% | listening to input",
                           juce::dontSendNotification);
        }
    }

    repaint();
}

void GVocalAudioProcessorEditor::drawStageIcon(juce::Graphics& g, int index, juce::Rectangle<float> area, juce::Colour colour, bool active)
{
    g.setColour(active ? colour : colour.withAlpha(0.75f));
    const float x = area.getX(), y = area.getY(), w = area.getWidth(), h = area.getHeight();

    switch (index)
    {
        case 0:
            g.drawLine(x + 2, y + h - 6, x + 10, y + h - 6, 2.2f);
            g.drawLine(x + 10, y + h - 6, x + 10, y + 8, 2.2f);
            g.drawLine(x + 10, y + 8, x + 24, y + 8, 2.2f);
            g.drawLine(x + 24, y + 8, x + 24, y + h - 6, 2.2f);
            g.drawLine(x + 24, y + h - 6, x + w - 2, y + h - 6, 2.2f);
            break;
        case 1:
            g.drawLine(x + 4, y + h - 8, x + 12, y + 10, 2.0f);
            g.drawLine(x + 12, y + 10, x + 21, y + h - 12, 2.0f);
            g.drawLine(x + 21, y + h - 12, x + w - 6, y + 12, 2.0f);
            g.fillEllipse(x + 2, y + h - 10, 4, 4);
            g.fillEllipse(x + 10, y + 8, 4, 4);
            g.fillEllipse(x + 19, y + h - 14, 4, 4);
            g.fillEllipse(x + w - 8, y + 10, 4, 4);
            break;
        case 2:
        case 7:
        {
            const float bw = 4.5f;
            for (int i = 0; i < 4; ++i)
            {
                const float bh = 8.0f + i * 5.0f;
                g.fillRoundedRectangle(x + 3 + i * 7.0f, y + h - bh - 3.0f, bw, bh, 1.6f);
            }
            break;
        }
        case 3:
            g.setFont(juce::Font(26.0f, juce::Font::bold));
            g.drawText("S", area.toNearestInt(), juce::Justification::centred);
            break;
        case 4:
            g.drawRoundedRectangle(x + 7, y + 3, w - 14, h - 6, 5.0f, 2.0f);
            g.drawLine(x + w * 0.5f, y + 6, x + w * 0.5f, y + h - 6, 1.8f);
            g.drawLine(x + 10, y + h * 0.45f, x + w - 10, y + h * 0.45f, 1.8f);
            break;
        case 5:
        {
            // JUCE Graphics has no drawArc() method: stroke a juce::Path instead.
            juce::Path outerArc, innerArc;
            outerArc.addArc(x + 4.0f, y + 12.0f, w - 8.0f, h - 16.0f,
                            juce::MathConstants<float>::pi, juce::MathConstants<float>::twoPi, true);
            innerArc.addArc(x + 8.0f, y + 16.0f, w - 16.0f, h - 24.0f,
                            juce::MathConstants<float>::pi, juce::MathConstants<float>::twoPi, true);
            g.strokePath(outerArc, juce::PathStrokeType(2.0f));
            g.strokePath(innerArc, juce::PathStrokeType(2.0f));
            break;
        }
        case 6:
            g.drawEllipse(x + 2, y + 6, w * 0.55f, h - 12, 2.0f);
            g.drawEllipse(x + w * 0.35f, y + 6, w * 0.55f, h - 12, 2.0f);
            break;
        default:
            g.fillEllipse(area.reduced(7.0f));
            break;
    }
}

void GVocalAudioProcessorEditor::drawStageRow(juce::Graphics& g, int index, juce::Rectangle<int> r)
{
    const bool active = index == selectedStage;
    const bool on = stageEnabled[(size_t) index].getToggleState();
    const bool locked = stageLocked[(size_t) index].getToggleState();
    const auto accent = stageAccent(index);
    const float activity = stageActivity(index);

    auto row = r.toFloat();
    g.setColour(active ? panel2.brighter(0.16f) : panel2.withAlpha(0.92f));
    g.fillRoundedRectangle(row, 12.0f);
    g.setColour(active ? accent.withAlpha(0.85f) : border);
    g.drawRoundedRectangle(row, 12.0f, active ? 1.8f : 1.0f);

    if (active)
    {
        g.setColour(withGlow(accent, 0.15f));
        g.fillRoundedRectangle(row.reduced(1.5f), 12.0f);
    }

    auto iconArea = juce::Rectangle<float>((float) r.getX() + 10.0f, (float) r.getY() + 10.0f, 34.0f, 34.0f);
    drawStageIcon(g, index, iconArea, accent, active || on);

    g.setColour(text);
    g.setFont(juce::Font(15.5f, juce::Font::bold));
    g.drawText(juce::String(index + 1) + "  " + GVocalAudioProcessor::stageName(index), r.getX() + 55, r.getY() + 8, 155, 20, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::Font(12.5f));
    g.drawText(stageSubtitle(index), r.getX() + 55, r.getY() + 28, 155, 18, juce::Justification::centredLeft);

    auto lamp = juce::Rectangle<float>((float) r.getX() + 214.0f, (float) r.getY() + 18.0f, 10.0f, 10.0f);
    g.setColour(on ? accent : borderSoft.brighter(0.25f));
    g.fillEllipse(lamp);
    if (on)
    {
        g.setColour(withGlow(accent, 0.25f));
        g.drawEllipse(lamp.expanded(3.5f), 2.0f);
    }

    const int bars = 4;
    const int lit = (int) std::round(activity * bars);
    for (int i = 0; i < bars; ++i)
    {
        juce::Rectangle<float> b((float) r.getX() + 205.0f + i * 7.5f, (float) r.getBottom() - 15.0f - (i * 2.0f), 4.6f, 7.0f + i * 2.0f);
        g.setColour(i < lit ? accent.withAlpha(0.95f) : borderSoft.brighter(0.15f));
        g.fillRoundedRectangle(b, 1.4f);
    }

    g.setColour(on ? muted.withAlpha(0.9f) : muted.withAlpha(0.55f));
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText("ON", r.getX() + 244, r.getY() + 7, 26, 12, juce::Justification::centred);
    g.drawText("LOCK", r.getX() + 282, r.getY() + 7, 38, 12, juce::Justification::centred);

    if (locked)
    {
        g.setColour(yellow.withAlpha(0.35f));
        g.fillRoundedRectangle((float) r.getRight() - 46.0f, (float) r.getY() + 11.0f, 25.0f, 14.0f, 7.0f);
    }
}

void GVocalAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bgGrad(bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    g.setColour(withGlow(mint, 0.07f));
    g.fillEllipse(-130.0f, -50.0f, 400.0f, 260.0f);
    g.setColour(withGlow(blue, 0.06f));
    g.fillEllipse((float) getWidth() - 280.0f, -30.0f, 260.0f, 230.0f);

    const juce::Rectangle<float> header(18.0f, 16.0f, (float) getWidth() - 36.0f, 102.0f);
    const juce::Rectangle<float> leftCard(18.0f, 136.0f, 320.0f, 564.0f);
    const juce::Rectangle<float> analysisCard(352.0f, 136.0f, (float) getWidth() - 370.0f, 154.0f);
    const juce::Rectangle<float> controlCard(352.0f, 305.0f, (float) getWidth() - 370.0f, 395.0f);

    auto drawCard = [&](juce::Rectangle<float> r)
    {
        g.setColour(panel.withAlpha(0.96f));
        g.fillRoundedRectangle(r, 16.0f);
        g.setColour(border.withAlpha(0.9f));
        g.drawRoundedRectangle(r, 16.0f, 1.0f);
    };

    drawCard(header);
    drawCard(leftCard);
    drawCard(analysisCard);
    drawCard(controlCard);

    g.setColour(mint);
    g.fillRoundedRectangle(34.0f, 32.0f, 58.0f, 58.0f, 13.0f);
    g.setColour(bgTop);
    g.setFont(juce::Font(42.0f, juce::Font::bold));
    g.drawText("g", 34, 32, 58, 58, juce::Justification::centred);

    g.setColour(text);
    g.setFont(juce::Font(35.0f, juce::Font::bold));
    g.drawText("g-vocal", 110, 27, 250, 40, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::Font(13.0f));
    g.drawText("HYBRID VOCAL PROCESSOR  |  v0.2 UI  |  HAYRUNI", 112, 68, 360, 18, juce::Justification::centredLeft);

    g.setColour(text);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("PROCESSING CHAIN", 34, 153, 250, 22, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::Font(12.0f));
    g.drawText("Each stage has its own activity lamp and mini meter.", 34, 176, 250, 16, juce::Justification::centredLeft);

    for (int i = 0; i < 8; ++i)
        drawStageRow(g, i, { 28, 205 + i * 61, 300, 52 });

    g.setColour(text);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("SIGNAL ANALYSIS", 372, 152, 220, 22, juce::Justification::centredLeft);

    auto pill = juce::Rectangle<float>(560.0f, 150.0f, 210.0f, 26.0f);
    g.setColour(withGlow(mint, 0.16f));
    g.fillRoundedRectangle(pill, 12.0f);
    g.setColour(withGlow(mint, 0.85f));
    g.drawRoundedRectangle(pill, 12.0f, 1.0f);
    g.setColour(text);
    g.setFont(juce::Font(12.5f, juce::Font::bold));
    g.drawText(analysisTag(), pill.toNearestInt(), juce::Justification::centred);

    g.setColour(muted);
    g.setFont(juce::Font(12.2f));
    g.drawText("RMS " + juce::String(proc.analysisRmsDb.load(), 1) + " dBFS", 788, 153, 110, 16, juce::Justification::centredLeft);
    g.drawText("Peak " + juce::String(proc.analysisPeakDb.load(), 1) + " dBFS", 902, 153, 120, 16, juce::Justification::centredLeft);

    drawMeter(g, { 372, 196, 670, 30 }, proc.inputPeakDb.load(), "INPUT");
    drawMeter(g, { 372, 236, 670, 30 }, proc.outputPeakDb.load(), "OUTPUT");

    if (waitingForAnalysis)
    {
        g.setColour(muted.withAlpha(0.7f));
        g.setFont(juce::Font(11.5f));
        g.drawText("Analysis progress", 373, 270, 100, 14, juce::Justification::centredLeft);
        g.setColour(borderSoft.brighter(0.15f));
        g.fillRoundedRectangle(470.0f, 272.0f, 350.0f, 6.0f, 3.0f);
        g.setColour(mint);
        g.fillRoundedRectangle(470.0f, 272.0f, 350.0f * juce::jlimit(0.0f, 1.0f, proc.analysisProgress.load()), 6.0f, 3.0f);
    }

    const auto accent = stageAccent(selectedStage);
    g.setColour(accent);
    g.fillRoundedRectangle(372.0f, 325.0f, 38.0f, 38.0f, 10.0f);
    drawStageIcon(g, selectedStage, { 375.0f, 328.0f, 32.0f, 32.0f }, bgTop, true);

    g.setColour(text);
    g.setFont(juce::Font(31.0f, juce::Font::bold));
    g.drawText(GVocalAudioProcessor::stageName(selectedStage), 425, 323, 280, 32, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::Font(14.0f));
    g.drawText(stageDescription(selectedStage), 425, 356, 480, 20, juce::Justification::centredLeft);

    g.setColour(text);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText("STAGE INTENSITY", 376, 394, 150, 18, juce::Justification::centredLeft);
    g.drawText(juce::String(std::round(stageActivity(selectedStage) * 100.0f)) + "%", 970, 394, 70, 18, juce::Justification::centredRight);

    for (int i = 0; i < 24; ++i)
    {
        const float x = 540.0f + i * 18.0f;
        const bool lit = i < (int) std::round(stageActivity(selectedStage) * 24.0f);
        g.setColour(lit ? accent.withAlpha(0.95f) : borderSoft.brighter(0.12f));
        g.fillRoundedRectangle(x, 397.0f, 11.0f, 18.0f, 3.0f);
    }

    g.setColour(muted);
    g.setFont(juce::Font(11.2f));
    g.drawText("Live status meter for the selected module", 376, 418, 270, 14, juce::Justification::centredLeft);

    g.setColour(borderSoft.brighter(0.15f));
    g.drawLine(374.0f, 630.0f, 1030.0f, 630.0f, 1.0f);

    g.setColour(text);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("Mix workflow stays the same: ANALYZE, tweak, compare with ENGINE ON/BYPASS.", 24, getHeight() - 26, 720, 18, juce::Justification::centredLeft);
    g.setColour(muted);
    g.setFont(juce::Font(12.0f));
    g.drawText("VST3  |  Windows x64  |  G-Vocal 0.2 UI concept", getWidth() - 290, getHeight() - 26, 260, 18, juce::Justification::centredRight);
}

void GVocalAudioProcessorEditor::drawMeter(juce::Graphics& g, juce::Rectangle<int> r, float db, const juce::String& caption, bool)
{
    auto labelArea = r.removeFromLeft(90);
    auto valueArea = r.removeFromRight(90);
    auto bar = r.reduced(0, 7);

    g.setColour(text);
    g.setFont(juce::Font(13.2f, juce::Font::bold));
    g.drawText(caption, labelArea, juce::Justification::centredLeft);
    g.setColour(text);
    g.setFont(juce::Font(12.2f));
    g.drawText(juce::String(db, 1) + " dB", valueArea, juce::Justification::centredRight);

    g.setColour(borderSoft.brighter(0.12f));
    g.fillRoundedRectangle(bar.toFloat(), 6.0f);
    const auto norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto meterColour = db > -1.0f ? juce::Colours::orangered : mint;
    g.setColour(meterColour);
    g.fillRoundedRectangle((float) bar.getX(), (float) bar.getY(), bar.getWidth() * norm, (float) bar.getHeight(), 6.0f);
}

void GVocalAudioProcessorEditor::resized()
{
    styles.setBounds(640, 43, 160, 36);
    analyze.setBounds(812, 43, 130, 36);
    engine.setBounds(953, 43, 105, 36);
    subheader.setBounds(28, 98, 820, 20);

    for (int i = 0; i < 8; ++i)
    {
        const int y = 205 + i * 61;
        stageButtons[(size_t) i].setBounds(28, y, 225, 52);
        stageEnabled[(size_t) i].setBounds(272, y + 17, 22, 20);
        stageLocked[(size_t) i].setBounds(309, y + 17, 22, 20);
    }

    fix.setBounds(904, 323, 126, 32);

    int y = 446;
    for (size_t i = 0; i < controlSliders.size(); ++i)
    {
        controlLabels[i]->setBounds(378, y, 165, 24);
        controlSliders[i]->setBounds(545, y - 2, 485, 28);
        y += 46;
    }

    if (safetyToggle)
        safetyToggle->setBounds(378, y + 4, 340, 24);

    status.setBounds(372, 270, 650, 18);
}
