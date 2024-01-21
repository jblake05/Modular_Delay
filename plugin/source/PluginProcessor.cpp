#include "../include/Modular_Delay/PluginProcessor.h"
#include "../include/Modular_Delay/PluginEditor.h"
#include <queue>
#include <vector>
#include <math.h>
#include <time.h>
#include <chrono>
#include <omp.h>

using namespace std;

/*
Author: Jeff Blake <jtblake@middlebury.edu>
*/

juce::AudioParameterFloat* feedback;
juce::AudioParameterInt* delay;
juce::AudioParameterFloat* dist_ramp;
juce::AudioParameterFloat* dist_dw;
juce::AudioParameterBool* bypass;

chrono::microseconds avgBlock;
int avgCount = 0;
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
        0.1f,
        0.01f
    ));

    addParameter(dist_dw = new juce::AudioParameterFloat(
        "dist_dw",
        "dist_dw",
        0.0f,
        1.0f,
        0.1f
    ));

    addParameter(bypass = new juce::AudioParameterBool(
        "bypass",
        "Bypass",
        false
    ));

    // Define threads here
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    // Break threads down
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
void applyGain(float *out, float gainAmtDB) {
    // Seems to increase it by double the desired gain.......
    *out = (float) (gainAmtDB >= 0 ? *out * pow(10, gainAmtDB/10) : *out / pow(10, -gainAmtDB/10));
}

void applyDistortion(float *out, float dist) {
    *out = tanh((1 - *dist_dw + dist * *dist_dw) * *out);
}

// Quantizes using modulo
void bit_reduction(float *out, int num_bits){
    int steps = (int) pow(2, num_bits);
    float stepSize = (float) 2/steps;
    *out -= fmod(*out, stepSize);

    // cout << *out;
    // *out = closestNum;
    // cout << ", " << *out << "\n";
}

void downSample(juce::AudioBuffer<float>& buffer, int numIn, int factor) {

    float prevSample[2];

    for (int channel = 0; channel < numIn; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        juce::ignoreUnused(channelData);
        
        for (int sample = 0; sample < buffer.getNumSamples(); sample++){
            if (factor != 1) {
                if (sample % factor == 0)
                    prevSample[channel] = channelData[sample];
                else
                    channelData[sample] = prevSample[channel];
            }
        }
    }

}

// void applyLowPass(juce::AudioBuffer<float>& buffer, int channel, double alpha) {
//     double speriod = 1/srate;
//     double RC = speriod * ((1 - alpha)/alpha);

// auto* channel
//     short prevInput = buffer[channel][2];
// }

// just kind of adds buzz, needs white noise
// float applyNoise(float in, float noiseAmt) {
//     return in + ((rand()/RAND_MAX) * noiseAmt - (2*noiseAmt));
// } 

// converts ms to (most accurate, typically won't be exact) number of samples
int sizeInSamples(int msecs) {
    return (int) (srate * ((float) msecs/1000));
}

/* 
Parallelization approches:
OMP
Interleaving samples
Work queue (id numbers to put back into buffer)
Busy/sleepy wait ^--
*/

// float mapDistDw(float dw, float dist) {
//     // return 1/dist + ((1 - 1/dist) * dw);

//     // = 1/dist + dw - dw/dist
//     // = 1-dw/dist + dw
// }

// TODO: Make based on numIns
queue<float> delayBuffers[2];
float dist = 3.0f;

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    // timing help (library reccomendation) from: https://stackoverflow.com/questions/11062804/measuring-the-runtime-of-a-c-code
    auto start = chrono::system_clock::now();

    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;

    // By default, numIns is 2
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    // float FEEDBACK = *feedback;

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


        // cout << omp_get_thread_num;

    // cout << "Here\n";
    // Retool to take user (knob?) input in future
  
    // int maxDelaySize = sizeInSamples(*delay);

    // downSample(buffer, totalNumInputChannels, 16);
    
    // omp_set_num_threads(4);
    // #pragma omp parallel
    // {
    //     cout << omp_get_thread_num() << "\n";
    // }
    
    // cout << omp_get_thread_num;

    // omp parallel for overhead at this level much slower........ 166 muS serial vs ~ 500 muS parallel
    // #pragma omp parallel for
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        juce::ignoreUnused(channelData);
      
        if (!*bypass) {
            for (int sample = 0; sample < buffer.getNumSamples(); sample++){
                bit_reduction(&channelData[sample], 6);
                // if (delayBuffers[channel].size() >= maxDelaySize) {
                //     // add delayed sound, push back into buffer
                //     float out = delayBuffers[channel].front() * FEEDBACK;
                //     // bit_reduction(&out, 8);

                //     // applyDistortion(&out, dist);
                //     // applyGain(&out, 1);
                //     // TODO: out = applyEffect(out, effectName)
                //     // out = applyNoise(out, 0.25f); 
                //     // Using apply noise is making the output cut out after the delay time is up??

                //     channelData[sample] += out;

                //     // Helps with changing the parameter for delay time, otherwise the buffer just stays full because one more is added each time,
                //     // might be a better idea to take away two or three each iteration
                //     while (delayBuffers[channel].size() > maxDelaySize)
                //         delayBuffers[channel].pop();
                // }
                // delayBuffers[channel].push(channelData[sample]);
            }
        // if (*dist_dw > 0 && *dist_ramp > 0)
        //     dist += *dist_ramp;
        }
    }
    // if (!*bypass){
    //     auto end = chrono::system_clock::now();
    //     auto elapsed =  end - start;
    //     chrono::microseconds elapsedMillis = chrono::duration_cast< chrono::microseconds >(elapsed);
    //     avgBlock += elapsedMillis;
    //     avgCount++;
    //     cout << avgBlock.count()/avgCount << "\n";
    // }
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
    juce::MemoryOutputStream(destData, true).writeFloat(*dist_dw);
    juce::MemoryOutputStream(destData, true).writeBool(*bypass);

}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    // juce::ignoreUnused (data, sizeInBytes);
    *feedback = juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readFloat();
    *delay = juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readInt();
    *dist_ramp = juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readFloat();
    *dist_dw = juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readFloat();
    *bypass = juce::MemoryInputStream(data, static_cast<size_t> (sizeInBytes), false).readBool();

}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
