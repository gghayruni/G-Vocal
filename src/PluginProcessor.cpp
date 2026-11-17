#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
constexpr float pi = juce::MathConstants<float>::pi;
float clampf(float v, float a, float b) noexcept { return juce::jlimit(a,b,v); }
float dbToAmp(float db) noexcept { return juce::Decibels::decibelsToGain(db); }
float safeDb(float amp) noexcept { return juce::Decibels::gainToDecibels(juce::jmax(amp,1.0e-8f)); }
const char* const names[] = {"CLEAN", "SMART EQ", "COMPRESSOR", "DE-ESSER", "SATURATION", "REVERB", "DELAY", "OUTPUT"};
const juce::StringArray params[] = {
    {"lowCut", "gate"},
    {"body", "presence", "air"},
    {"compThresh", "ratio", "attack", "release"},
    {"essThresh", "essReduction"},
    {"drive", "satMix"},
    {"room", "revWet", "damping"},
    {"delayTime", "feedback", "delayMix"},
    {"outputGain", "safety"}
};
}

void GVocalAudioProcessor::Biquad::coefficients(float n0,float n1,float n2,float d0,float d1,float d2) noexcept
{
    const auto k=1.0f/d0;
    b0=n0*k; b1=n1*k; b2=n2*k; a1=d1*k; a2=d2*k;
}
void GVocalAudioProcessor::Biquad::highPass(double fs,float hz) noexcept
{
    const float w=2*pi*hz/(float)fs, c=std::cos(w), s=std::sin(w), a=s/(2*0.70710678f);
    coefficients((1+c)/2,-(1+c),(1+c)/2,1+a,-2*c,1-a);
}
void GVocalAudioProcessor::Biquad::peak(double fs,float hz,float db) noexcept
{
    const float a=std::pow(10.f,db/40.f), w=2*pi*hz/(float)fs, c=std::cos(w), s=std::sin(w);
    const float alpha=s/(2*0.9f);
    coefficients(1+alpha*a,-2*c,1-alpha*a,1+alpha/a,-2*c,1-alpha/a);
}
void GVocalAudioProcessor::Biquad::lowShelf(double fs,float hz,float db) noexcept
{
    const float a=std::pow(10.f,db/40.f), w=2*pi*hz/(float)fs, c=std::cos(w), alpha=std::sin(w)*0.70710678f;
    const float two=2*std::sqrt(a)*alpha;
    coefficients(a*((a+1)-(a-1)*c+two),2*a*((a-1)-(a+1)*c),a*((a+1)-(a-1)*c-two),
                 ((a+1)+(a-1)*c+two),-2*((a-1)+(a+1)*c),((a+1)+(a-1)*c-two));
}
void GVocalAudioProcessor::Biquad::highShelf(double fs,float hz,float db) noexcept
{
    const float a=std::pow(10.f,db/40.f), w=2*pi*hz/(float)fs, c=std::cos(w), alpha=std::sin(w)*0.70710678f;
    const float two=2*std::sqrt(a)*alpha;
    coefficients(a*((a+1)+(a-1)*c+two),-2*a*((a-1)+(a+1)*c),a*((a+1)+(a-1)*c-two),
                 ((a+1)-(a-1)*c+two),2*((a-1)-(a+1)*c),((a+1)-(a-1)*c-two));
}

juce::AudioProcessorValueTreeState::ParameterLayout GVocalAudioProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    auto f=[&](const char* id, const char* name, float min,float max,float start,float step=0.1f)
    {
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,
            juce::NormalisableRange<float>(min,max,step),start));
    };
    for(int i=0;i<stageCount;++i)
    {
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"on"+juce::String(i),1},
            "Stage " + juce::String(i+1) + " enabled",true));
        p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"lock"+juce::String(i),1},
            "Stage " + juce::String(i+1) + " lock",false));
    }
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"engine",1},"Engine on",true));
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"style",1},"Vocal style",
          juce::StringArray{"NATURAL","POP","BALLAD","WARM","POWERFUL"},0));
    f("lowCut","Low cut Hz",50,180,85,1); f("gate","Gate dB",-70,-20,-60);
    f("body","Body dB",-9,9,-1); f("presence","Presence dB",-9,9,1); f("air","Air dB",-9,9,1);
    f("compThresh","Compressor threshold dB",-42,-3,-22); f("ratio","Compressor ratio",1,8,3);
    f("attack","Attack ms",2,80,12); f("release","Release ms",40,450,130,1);
    f("essThresh","De-esser threshold dB",-50,-8,-28); f("essReduction","De-esser max reduction dB",0,12,5);
    f("drive","Saturation drive dB",0,15,2); f("satMix","Saturation mix %",0,100,18,1);
    f("room","Reverb room %",0,100,38,1); f("revWet","Reverb wet %",0,60,12,1);
    f("damping","Reverb damping %",0,100,55,1);
    f("delayTime","Delay time ms",60,900,310,1); f("feedback","Delay feedback %",0,65,20,1);
    f("delayMix","Delay mix %",0,50,10,1); f("outputGain","Output dB",-18,12,0);
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"safety",1},"Sample peak safety",true));
    return p;
}

GVocalAudioProcessor::GVocalAudioProcessor()
  : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                     .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    parameters(*this,nullptr,"G-VocalState",makeParameters()) {}

void GVocalAudioProcessor::prepareToPlay(double rate,int maxSamples)
{
    sr=juce::jmax(8000.0,rate);
    for(int c=0;c<2;++c)
    {
        hp[c].reset();low[c].reset();mid[c].reset();high[c].reset();
        gateEnv[c]=compEnv[c]=essLow[c]=essEnv[c]=0;
        gateGain[c]=compGain[c]=1;
        delayBuffer[c].assign(static_cast<size_t>(sr*2.0)+2,0.f);
    }
    dryBuffer.setSize(2,juce::jmax(64,maxSamples));
    reverb.setSampleRate(sr); reverb.reset();
    delayWrite=0;
    engineMix=value("engine")>=0.5f?1.f:0.f;
}

bool GVocalAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto x=l.getMainOutputChannelSet();
    return (x==juce::AudioChannelSet::mono()||x==juce::AudioChannelSet::stereo())&&
            x==l.getMainInputChannelSet();
}
float GVocalAudioProcessor::value(const char* id) const noexcept
{
    if(auto* v=parameters.getRawParameterValue(id)) return v->load();
    return 0.f;
}
bool GVocalAudioProcessor::enabled(int i) const noexcept {return value(("on"+juce::String(i)).toRawUTF8())>=0.5f;}
bool GVocalAudioProcessor::locked(int i) const noexcept {return value(("lock"+juce::String(i)).toRawUTF8())>=0.5f;}

void GVocalAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int ch=juce::jmin(2,buffer.getNumChannels()), n=buffer.getNumSamples();
    if(ch<=0||n<=0)return;
    if(dryBuffer.getNumSamples()<n) dryBuffer.setSize(2,n,false,false,true);
    for(int c=0;c<ch;++c) dryBuffer.copyFrom(c,0,buffer,c,0,n);

    if(analysisRequested.exchange(false))
    {
        analyzedSamples=0; analyzedSquares=analyzedBright=analyzedLevel=0;
        analyzedPeak=previousInput=0;
        analysisReady.store(false);
        analysisProgress.store(0);
    }
    const auto limit=(uint64_t)(sr*7.0);
    float pkIn=0;
    for(int i=0;i<n;++i)
    {
        float mono=0;
        for(int c=0;c<ch;++c) {const float x=buffer.getSample(c,i);pkIn=juce::jmax(pkIn,std::abs(x));mono+=x;}
        mono/=ch;
        if(analyzedSamples<limit)
        {
            analyzedSquares+=(double)mono*mono;
            analyzedBright+=(double)(mono-previousInput)*(mono-previousInput);
            analyzedLevel+=std::abs(mono);
            analyzedPeak=juce::jmax(analyzedPeak,std::abs(mono));
            previousInput=mono;
            ++analyzedSamples;
            if(analyzedSamples==limit)
            {
                analysisRmsDb.store(safeDb(std::sqrt((float)(analyzedSquares/juce::jmax((uint64_t)1,analyzedSamples)))));
                analysisPeakDb.store(safeDb(analyzedPeak));
                analysisBrightness.store(clampf((float)(analyzedBright/(analyzedSquares+1.0e-12)),0,3));
                analysisProgress.store(1.f);
                analysisReady.store(true,std::memory_order_release);
            }
        }
    }
    if(analyzedSamples<limit)analysisProgress.store((float)((double)analyzedSamples/(double)limit));
    inputPeakDb.store(safeDb(pkIn));

    bool on[8];
    for(int i=0;i<8;++i)on[i]=enabled(i);
    const auto cut=value("lowCut"), body=value("body"), pres=value("presence"), air=value("air");
    for(int c=0;c<ch;++c)
    {
        hp[c].highPass(sr,cut);
        low[c].lowShelf(sr,210,body);
        mid[c].peak(sr,3100,pres);
        high[c].highShelf(sr,juce::jmin(9500.f,0.45f*(float)sr),air);
    }
    const float gateThreshold=dbToAmp(value("gate"));
    const float t=dbToAmp(value("compThresh"));
    const float ratio=juce::jmax(1.f,value("ratio"));
    const float att=std::exp(-1.f/(float)(sr*value("attack")*0.001));
    const float rel=std::exp(-1.f/(float)(sr*value("release")*0.001));
    const float essThreshold=dbToAmp(value("essThresh"));
    const float essMax=value("essReduction");
    const float drive=dbToAmp(value("drive"));
    const float satMix=value("satMix")*.01f;
    const float gateSmoothing=1.f-std::exp(-1.f/(float)(sr*.025));
    const float compSmoothing=1.f-std::exp(-1.f/(float)(sr*.008));
    const float essSmoothing=1.f-std::exp(-1.f/(float)(sr*.008));
    const float essLP=1.f-std::exp(-2*pi*5500.f/(float)sr);

    for(int c=0;c<ch;++c)
    {
        float* p=buffer.getWritePointer(c);
        for(int i=0;i<n;++i)
        {
            float x=p[i];
            if(on[0])
            {
                x=hp[c].process(x);
                gateEnv[c]+=(std::abs(x)-gateEnv[c])*.01f;
                const float target=gateEnv[c]<gateThreshold?0.18f:1.f;
                gateGain[c]+=(target-gateGain[c])*gateSmoothing;
                x*=gateGain[c];
            }
            if(on[1])x=high[c].process(mid[c].process(low[c].process(x)));
            if(on[2])
            {
                const float level=std::abs(x);
                const float coeff=level>compEnv[c]?att:rel;
                compEnv[c]=coeff*compEnv[c]+(1-coeff)*level;
                const float env=juce::jmax(compEnv[c],1.e-8f);
                const float target=env>t?std::pow(t/env,1.f-1.f/ratio):1.f;
                compGain[c]+=(target-compGain[c])*compSmoothing;
                x*=compGain[c];
            }
            if(on[3])
            {
                essLow[c]+=essLP*(x-essLow[c]);
                const float sib=std::abs(x-essLow[c]);
                essEnv[c]+=(sib-essEnv[c])*essSmoothing;
                const float over=essEnv[c]>essThreshold?juce::jmin(1.f,essEnv[c]/essThreshold-1.f):0.f;
                x*=dbToAmp(-essMax*over);
            }
            if(on[4])
            {
                const auto saturated=std::tanh(x*drive)/juce::jmax(.1f,std::tanh(drive));
                x=x*(1.f-satMix)+saturated*satMix;
            }
            p[i]=x;
        }
    }
    if(on[5])
    {
        juce::Reverb::Parameters r;
        r.roomSize=value("room")*.01f;
        r.damping=value("damping")*.01f;
        r.wetLevel=value("revWet")*.01f;
        r.dryLevel=1.f-r.wetLevel;
        r.width=1.f;
        reverb.setParameters(r);
        if(ch==2) reverb.processStereo(buffer.getWritePointer(0),buffer.getWritePointer(1),n);
        else reverb.processMono(buffer.getWritePointer(0),n);
    }
    if(!delayBuffer[0].empty())
    {
        const int length=(int)delayBuffer[0].size();
        const int lag=juce::jlimit(1,length-1,(int)(sr*value("delayTime")*.001));
        const float wet=value("delayMix")*.01f, feedback=value("feedback")*.01f;
        for(int i=0;i<n;++i)
        {
            const int read=(delayWrite-lag+length)%length;
            for(int c=0;c<ch;++c)
            {
                float& x=buffer.getWritePointer(c)[i];
                const auto echo=delayBuffer[c][(size_t)read];
                delayBuffer[c][(size_t)delayWrite]=x+echo*feedback;
                if(on[6])x=x*(1.f-wet)+echo*wet;
            }
            delayWrite=(delayWrite+1)%length;
        }
    }
    const float outGain=on[7]?dbToAmp(value("outputGain")):1.f;
    const bool safety=on[7]&&value("safety")>=.5f;
    const float targetMix=value("engine")>=.5f?1.f:0.f;
    const float step=1.f/(float)juce::jmax(1.,sr*.012);
    float pkOut=0;
    for(int i=0;i<n;++i)
    {
        if(engineMix<targetMix)engineMix=juce::jmin(targetMix,engineMix+step);
        if(engineMix>targetMix)engineMix=juce::jmax(targetMix,engineMix-step);
        for(int c=0;c<ch;++c)
        {
            float wet=buffer.getSample(c,i)*outGain;
            if(safety)wet=std::tanh(wet);  // sample safety: soft clip, NOT true-peak limiting
            const auto y=wet*engineMix+dryBuffer.getSample(c,i)*(1.f-engineMix);
            buffer.setSample(c,i,y);
            pkOut=juce::jmax(pkOut,std::abs(y));
        }
    }
    outputPeakDb.store(safeDb(pkOut));
}

void GVocalAudioProcessor::beginAnalysis() noexcept
{
    analysisReady.store(false);
    analysisProgress.store(0);
    analysisRequested.store(true);
}

void GVocalAudioProcessor::setValue(const char* id,float v)
{
    if(auto* p=dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(id)))
        p->setValueNotifyingHost(p->convertTo0to1(v)); // Call only on JUCE message thread.
}

void GVocalAudioProcessor::applyPreset(int style,int onlyStage,bool respectLocks)
{
    // 5 original editable starting points; locked stages are never overwritten.
    const float eq[][3]={{-1,1,1},{-1,2.5f,2},{0,1,1.5f},{2,0,-.5f},{-2,3.5f,2.5f}};
    const float cp[][4]={{-22,3,12,130},{-25,3.5f,8,105},{-21,2.5f,22,190},{-20,2.7f,18,155},{-28,4.5f,6,95}};
    style=juce::jlimit(0,4,style);
    auto s=[&](int stage,const char* id,float val)
    {if((onlyStage<0||onlyStage==stage)&&(!respectLocks||!locked(stage)))setValue(id,val);};
    s(0,"lowCut",style==4?95:85); s(0,"gate",-60);
    s(1,"body",eq[style][0]);s(1,"presence",eq[style][1]);s(1,"air",eq[style][2]);
    s(2,"compThresh",cp[style][0]);s(2,"ratio",cp[style][1]);
    s(2,"attack",cp[style][2]);s(2,"release",cp[style][3]);
    s(3,"essThresh",-28);s(3,"essReduction",style==4?7:5);
    s(4,"drive",style==4?4:2);s(4,"satMix",style==3?24:18);
    s(5,"room",style==2?55:38);s(5,"revWet",style==2?20:12);s(5,"damping",55);
    s(6,"delayTime",310);s(6,"feedback",20);s(6,"delayMix",style==2?14:10);
    s(7,"outputGain",0);
}

void GVocalAudioProcessor::applyAnalysis(int style)
{
    applyPreset(style,-1,true);
    const float rms=analysisRmsDb.load();
    if(!locked(2))setValue("compThresh",clampf(rms-5.f,-42,-3));
    if(!locked(3))setValue("essThresh",clampf(rms-8.f,-50,-8));
    if(!locked(0))setValue("gate",clampf(rms-32.f,-70,-20));
    // Transient-difference ratio: a crude brightness proxy, not an ML/FFT model.
    const float brightness=analysisBrightness.load();
    if(!locked(1))setValue("air",clampf(value("air")+(brightness<.06f?1.f:(brightness>.32f?-1.f:0.f)),-9,9));
}

const char* GVocalAudioProcessor::stageName(int index) noexcept
{return names[juce::jlimit(0,stageCount-1,index)];}
const juce::StringArray& GVocalAudioProcessor::stageParams(int index)
{return params[juce::jlimit(0,stageCount-1,index)];}

void GVocalAudioProcessor::getStateInformation(juce::MemoryBlock& out)
{
    if(auto xml=parameters.copyState().createXml())copyXmlToBinary(*xml,out);
}
void GVocalAudioProcessor::setStateInformation(const void* data,int size)
{
    if(auto xml=getXmlFromBinary(data,size))
        if(xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorEditor* GVocalAudioProcessor::createEditor()
{return new GVocalAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{return new GVocalAudioProcessor();}
