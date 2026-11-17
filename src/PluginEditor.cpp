#include "PluginEditor.h"

namespace
{
const juce::Colour bg=juce::Colour::fromRGB(11,18,30);
const juce::Colour panel=juce::Colour::fromRGB(21,32,49);
const juce::Colour border=juce::Colour::fromRGB(44,63,86);
const juce::Colour mint=juce::Colour::fromRGB(39,225,189);
const juce::Colour text=juce::Colour::fromRGB(230,240,249);
const juce::Colour muted=juce::Colour::fromRGB(143,163,190);
}

GVocalAudioProcessorEditor::GVocalAudioProcessorEditor(GVocalAudioProcessor& p)
 : AudioProcessorEditor(&p),proc(p)
{
    setSize(1000,700);
    styles.addItemList(juce::StringArray{"NATURAL","POP","BALLAD","WARM","POWERFUL"},1);
    addAndMakeVisible(styles);
    styles.setColour(juce::ComboBox::backgroundColourId,panel);
    styles.setColour(juce::ComboBox::textColourId,text);
    styles.setColour(juce::ComboBox::outlineColourId,border);
    styleAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.parameters,"style",styles);
    styles.onChange=[this] { proc.applyPreset(styles.getSelectedItemIndex()); };

    addAndMakeVisible(analyze);
    analyze.setColour(juce::TextButton::buttonColourId,mint);
    analyze.setColour(juce::TextButton::textColourOffId,bg);
    analyze.onClick=[this]
    {
        waitingForAnalysis=true;
        proc.beginAnalysis();
        status.setText("Анализ 7 секунд: запустите вокал в Ableton...",juce::dontSendNotification);
    };
    addAndMakeVisible(engine);
    engine.setClickingTogglesState(true);
    engine.setColour(juce::TextButton::buttonColourId,juce::Colour::fromRGB(48,72,93));
    engine.setColour(juce::TextButton::textColourOffId,text);
    engineAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.parameters,"engine",engine);
    engine.onClick=[this] { updateBypassLabel(); };
    updateBypassLabel();

    addAndMakeVisible(fix);
    fix.setColour(juce::TextButton::buttonColourId,juce::Colour::fromRGB(47,57,84));
    fix.onClick=[this]
    {
        const int style=juce::jlimit(0,4,styles.getSelectedItemIndex());
        proc.applyPreset(style,selectedStage,false);
        status.setText("Настройки выбранного этапа сброшены к пресету",juce::dontSendNotification);
    };
    subheader.setText("8 PROCESSORS  /  PER-STAGE ON-OFF  /  LOCK  /  MANUAL CONTROL",juce::dontSendNotification);
    subheader.setColour(juce::Label::textColourId,muted);
    subheader.setFont(juce::Font(12.f));
    addAndMakeVisible(subheader);
    status.setText("HYBRID MODE  ·  Нажмите ANALYZE и воспроизведите вокал",juce::dontSendNotification);
    status.setColour(juce::Label::textColourId,muted);
    status.setFont(juce::Font(13.f));
    addAndMakeVisible(status);

    for(int i=0;i<8;++i)
    {
        stageButtons[(size_t)i].setButtonText(juce::String(i+1)+"  "+GVocalAudioProcessor::stageName(i));
        stageButtons[(size_t)i].setColour(juce::TextButton::textColourOffId,text);
        stageButtons[(size_t)i].onClick=[this,i] { selectStage(i); };
        addAndMakeVisible(stageButtons[(size_t)i]);
        auto& on=stageEnabled[(size_t)i];
        on.setButtonText("ON"); on.setColour(juce::ToggleButton::textColourId,muted);
        addAndMakeVisible(on);
        onAttachments[(size_t)i]=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.parameters,"on"+juce::String(i),on);
        auto& lock=stageLocked[(size_t)i];
        lock.setButtonText("LOCK"); lock.setColour(juce::ToggleButton::textColourId,muted);
        addAndMakeVisible(lock);
        lockAttachments[(size_t)i]=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            proc.parameters,"lock"+juce::String(i),lock);
    }
    selectStage(selectedStage);
    startTimerHz(20);
}
GVocalAudioProcessorEditor::~GVocalAudioProcessorEditor() {stopTimer();}

void GVocalAudioProcessorEditor::selectStage(int i)
{
    selectedStage=juce::jlimit(0,7,i);
    for(int j=0;j<8;++j)
        stageButtons[(size_t)j].setColour(juce::TextButton::buttonColourId,j==selectedStage?
            juce::Colour::fromRGB(35,88,97):panel);
    createParameterControls();
    resized();
    repaint();
}
void GVocalAudioProcessorEditor::createParameterControls()
{
    sliderAttachments.clear(); controlLabels.clear(); controlSliders.clear();
    safetyAttachment.reset(); safetyToggle.reset();
    for(auto id : GVocalAudioProcessor::stageParams(selectedStage))
    {
        if(id=="safety")
        {
            safetyToggle=std::make_unique<juce::ToggleButton>("SAMPLE PEAK SAFETY (SOFT CLIP)");
            safetyToggle->setColour(juce::ToggleButton::textColourId,text);
            addAndMakeVisible(*safetyToggle);
            safetyAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                proc.parameters,id,*safetyToggle);
            continue;
        }
        auto slider=std::make_unique<juce::Slider>();
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,80,25);
        slider->setColour(juce::Slider::trackColourId,mint);
        slider->setColour(juce::Slider::thumbColourId,juce::Colours::white);
        slider->setColour(juce::Slider::textBoxTextColourId,text);
        slider->setColour(juce::Slider::textBoxOutlineColourId,border);
        slider->setColour(juce::Slider::backgroundColourId,border);
        addAndMakeVisible(*slider);
        auto label=std::make_unique<juce::Label>();
        label->setText(id.toUpperCase(),juce::dontSendNotification);
        label->setColour(juce::Label::textColourId,muted);
        label->setFont(juce::Font(13.f,juce::Font::bold));
        addAndMakeVisible(*label);
        sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            proc.parameters,id,*slider));
        controlSliders.push_back(std::move(slider));
        controlLabels.push_back(std::move(label));
    }
}
void GVocalAudioProcessorEditor::updateBypassLabel()
{
    const auto* raw=proc.parameters.getRawParameterValue("engine");
    const bool on=raw&&raw->load()>=.5f;
    engine.setButtonText(on?"● ENGINE ON":"○ BYPASS");
}
void GVocalAudioProcessorEditor::timerCallback()
{
    updateBypassLabel();
    if(waitingForAnalysis)
    {
        if(proc.analysisReady.exchange(false,std::memory_order_acquire))
        {
            waitingForAnalysis=false;
            proc.applyAnalysis(juce::jlimit(0,4,styles.getSelectedItemIndex()));
            status.setText("Анализ готов  |  RMS "+juce::String(proc.analysisRmsDb.load(),1)
                +" dBFS  |  Peak "+juce::String(proc.analysisPeakDb.load(),1)
                +" dBFS  |  цепочка настроена",juce::dontSendNotification);
        }
        else
            status.setText("АНАЛИЗ: "+juce::String((int)(100*proc.analysisProgress.load()))+
                           "%  ·  звучащий вокал  ·  7 секунд",juce::dontSendNotification);
    }
    repaint(juce::Rectangle<int>(320,165,660,135));
}
void GVocalAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    g.setColour(juce::Colour::fromRGB(14,25,40));
    g.fillRoundedRectangle(juce::Rectangle<float>(15,12,970,120),13);
    g.setColour(mint);
    g.fillRoundedRectangle(32.f,28.f,57.f,58.f,12.f);
    g.setColour(bg);g.setFont(juce::Font(44.f,juce::Font::bold));g.drawText("g",32,28,57,57,juce::Justification::centred);
    g.setColour(text);g.setFont(juce::Font(36.f,juce::Font::bold));g.drawText("g-vocal",105,24,300,50,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(juce::Font(12.f));g.drawText("HYBRID VOCAL PROCESSOR  ·  v0.1  ·  HAYRUNI",106,80,380,18,juce::Justification::centredLeft);

    g.setColour(panel);
    g.fillRoundedRectangle(juce::Rectangle<float>(15,149,288,530),13);
    g.fillRoundedRectangle(juce::Rectangle<float>(315,149,670,177),13);
    g.fillRoundedRectangle(juce::Rectangle<float>(315,339,670,340),13);
    g.setColour(border);
    g.drawRoundedRectangle(juce::Rectangle<float>(15,149,288,530),13,1);
    g.drawRoundedRectangle(juce::Rectangle<float>(315,149,670,177),13,1);
    g.drawRoundedRectangle(juce::Rectangle<float>(315,339,670,340),13,1);
    g.setColour(text);g.setFont(juce::Font(15.f,juce::Font::bold));
    g.drawText("PROCESSING CHAIN",32,163,260,24,juce::Justification::centredLeft);
    g.drawText("SIGNAL ANALYSIS   /   INPUT & OUTPUT",337,165,590,24,juce::Justification::centredLeft);
    g.drawText(juce::String(GVocalAudioProcessor::stageName(selectedStage))+"   /   MANUAL CONTROLS",
               337,356,440,26,juce::Justification::centredLeft);
    g.setColour(muted);g.setFont(juce::Font(12.f));
    g.drawText("ON       LOCK",202,192,100,16,juce::Justification::centredLeft);
    drawMeter(g,{339,205,613,33},proc.inputPeakDb.load(),"INPUT");
    drawMeter(g,{339,250,613,33},proc.outputPeakDb.load(),"OUTPUT");
    if(waitingForAnalysis)
    {
        g.setColour(border);g.fillRoundedRectangle(340.f,303.f,610.f,6.f,3.f);
        g.setColour(mint);g.fillRoundedRectangle(340.f,303.f,610*juce::jlimit(0.f,1.f,proc.analysisProgress.load()),6.f,3.f);
    }
    g.setColour(muted);g.setFont(juce::Font(11.f));
    g.drawText("Transparent processing: these are real-time DSP stages, not third-party plugins.",
              337,635,613,24,juce::Justification::centredLeft);
}
void GVocalAudioProcessorEditor::drawMeter(juce::Graphics& g,juce::Rectangle<int> r,float db,juce::String caption)
{
    g.setColour(muted);g.setFont(juce::Font(12.f,juce::Font::bold));
    g.drawText(caption,r.removeFromLeft(76),juce::Justification::centredLeft);
    auto value=r.removeFromRight(83);
    g.setColour(text);g.drawText(juce::String(db,1)+" dB",value,juce::Justification::centredRight);
    auto bar=r.reduced(0,9);
    g.setColour(border);g.fillRoundedRectangle(bar.toFloat(),5);
    const auto norm=juce::jlimit(0.f,1.f,(db+60.f)/60.f);
    g.setColour(db>-1?juce::Colours::orangered:mint);
    g.fillRoundedRectangle(juce::Rectangle<float>((float)bar.getX(),(float)bar.getY(),bar.getWidth()*norm,(float)bar.getHeight()),5);
}
void GVocalAudioProcessorEditor::resized()
{
    styles.setBounds(550,48,145,35);
    analyze.setBounds(706,48,129,35);
    engine.setBounds(845,48,125,35);
    subheader.setBounds(25,109,850,23);
    for(int i=0;i<8;++i)
    {
        const int y=215+i*55;
        stageButtons[(size_t)i].setBounds(29,y,164,42);
        stageEnabled[(size_t)i].setBounds(202,y+7,48,28);
        stageLocked[(size_t)i].setBounds(248,y+7,55,28);
    }
    fix.setBounds(788,358,170,29);
    int y=412;
    for(size_t i=0;i<controlSliders.size();++i)
    {
        controlLabels[i]->setBounds(342,y,172,26);
        controlSliders[i]->setBounds(505,y,443,30);
        y+=48;
    }
    if(safetyToggle)safetyToggle->setBounds(347,y+8,400,28);
    status.setBounds(345,290,608,25);
}
