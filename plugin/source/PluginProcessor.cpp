#include "../include/Modular_Delay/PluginProcessor.h"
#include "../include/Modular_Delay/PluginEditor.h"
#include <queue>
#include <vector>

using namespace std;

/*
Author: Jeff Blake <jtblake@middlebury.edu>
*/

juce::AudioParameterFloat* feedback;
juce::AudioParameterInt* delay;
juce::AudioParameterFloat* dist_ramp;
double srate;

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
{
    // documented (along with get/set stateInformation) here: https://docs.juce.com/master/tutorial_audio_parameter.html
    addParameter(feedback = new juce::AudioParameterFloat(
        "feedback",
        "Feedback",
        0.0f,
        .95f,
        0.5f
    ));

    addParameter(delay = new juce::AudioParameterInt(
        "delay",
        "Delay",
        50,
        2000,
        500
    ));

    addParameter(dist_ramp = new juce::AudioParameterFloat(
        "d_ramp",
        "dist_ramp",
        0.0f,
        5.0f,
        0.1f
    ));
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioPluginAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================

// tip to grab samplerate here from https://forum.juce.com/t/how-to-access-the-audio-streams-sampling-rate-in-juce-program/35844/6
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (sampleRate, samplesPerBlock);
    srate = sampleRate;
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

// Input: in (signal), gainAmtDB (desired gain in DB)
float applyGain(float in, float gainAmtDB) {
    // Seems to increase it by double the desired gain.......
    return (float) (gainAmtDB >= 0 ? in * pow(10, gainAmtDB/10) : in / pow(10, -gainAmtDB/10));
}

// just kind of adds buzz, needs white noise
// float applyNoise(float in, float noiseAmt) {
//     return in + ((rand()/RAND_MAX) * noiseAmt - (2*noiseAmt));
// } 

// converts ms to (most accurate, typically won't be exact) number of samples
int sizeInSamples(int msecs) {
    return (int) (srate * ((float) msecs/1000));
}

// TODO: Make based on numIns
queue<float> delayBuffers[2];
float dist_start = 3.0f;

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;

    // By default, numIns is 2
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    float FEEDBACK = *feedback;

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    
    // queue<float> delayBuffers[totalNumInputChannels];
    // vector<queue<float>> delayBuffers;


    // cout << "Here\n";
    // Retool to take user (knob?) input in future
  
    int maxDelaySize = sizeInSamples(*delay);
    
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        juce::ignoreUnused(channelData);
      
        for (int sample = 0; sample < buffer.getNumSamples(); sample++){
            if (delayBuffers[channel].size() >= maxDelaySize) {
                // add delayed sound, push back into buffer
                float out = delayBuffers[channel].front() * FEEDBACK;
                out = tanh(dist_start * out);

                // TODO: out = applyEffect(out, effectName)
                // out = applyNoise(out, 0.25f); 
                // Using apply noise is making the output cut out after the delay time is up??

                channelData[sample] += out;

                // Helps with changing the parameter for delay time, otherwise the buffer just stays full because one more is added each time,
                // might be a better idea to take away two or three each iteration
                while (delayBuffers[channel].size() > maxDelaySize)
                    delayBuffers[channel].pop();
            }
            delayBuffers[channel].push(channelData[sample]);
        }
    }
    dist_start += *dist_ramp;
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    // return new AudioPluginAudioProcessorEditor (*this);
    // generic for now......
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    // juce::ignoreUnused (destData);
    juce::MemoryOutputStream(destData, true).writeFloat(*feedback);
    juce::MemoryOutputStream(destData, true).writeInt(*delay);
    juce::MemoryOutputStream(destData, true).writeFloat(*dist_ramp);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    // juce::ignoreUnused (data, sizeInBytes);
    *feedback = juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readFloat();
    *delay = juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readInt();
    *dist_ramp = juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readFloat();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
