/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleVocoderProcessor::SimpleVocoderProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    prepareToPlay (44100, 4);
    
    for(int i=0; i<256; i++)
    {
        window[i] = 0.5f - 0.5f * cosf(i * 0x1p-8f * 2.0f * M_PI);
    }
}

SimpleVocoderProcessor::~SimpleVocoderProcessor()
{
}

//==============================================================================
const juce::String SimpleVocoderProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleVocoderProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleVocoderProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleVocoderProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleVocoderProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleVocoderProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleVocoderProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleVocoderProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleVocoderProcessor::getProgramName (int index)
{
    return {};
}

void SimpleVocoderProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleVocoderProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    memset(X,0,sizeof(X));
    memset(Y,0,sizeof(X));
    memset(R,0,sizeof(X));

    phase = 0;
    delta = 440.0f * exp2f((60-69)/12.0f) * 0x1p32f / 44100.0f;
    
    pos = 0;
}

void SimpleVocoderProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NewProjectAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SimpleVocoderProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for(auto m : midiMessages)
    {
        auto msg = m.getMessage();
        if(msg.isNoteOn()) delta = 440.0f * exp2f((msg.getNoteNumber()-69)/12.0f) * 0x1p32f / 44100.0f;
    }
    
    const float *src = buffer.getReadPointer(0);              //input
    float *dst = buffer.getWritePointer(0);                   //output
    
    for(int i=0; i<buffer.getNumSamples(); i++)               // loop over samples
    {
        X[pos] =  *src++;
        
        unsigned char rd = pos;
        for(int lag=0; lag<128; lag++)  R[lag] = R[lag] * 0.9975f + 0.0025f * X[pos] * X[rd--]; //leaky autocorrelation

        phase += delta;             // wrapped phase
        if(phase < delta)           // cheap pitch synchronous operation
        {
            unsigned char wy = pos;                          //output write position
            const float scale = 1.0f/sqrtf(R[0] + 0x1p-10f); //gain correction
            for(int k=1; k<128; k++)  Y[wy++] += R[128-k] * window[k] * scale;  //add backwards
            for(int k=0; k<128; k++)  Y[wy++] += R[k] * window[k+128] * scale;  //add forward
        }
        *dst++ = Y[pos];              //copy output sample
        Y[pos++] = 0.0f;              //clear output sample in ringbuffer
    }
    for (int i = 1; i < getTotalNumOutputChannels(); ++i) buffer.copyFrom(i, 0, buffer, 0, 0, buffer.getNumSamples()) ;
}

//==============================================================================
bool SimpleVocoderProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* SimpleVocoderProcessor::createEditor()
{
    return new SimpleVocoderEditor (*this);
}

//==============================================================================
void SimpleVocoderProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SimpleVocoderProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleVocoderProcessor();
}
