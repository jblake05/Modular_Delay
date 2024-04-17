#include "../include/Modular_Delay/PluginProcessor.h"
#include "../include/Modular_Delay/PluginEditor.h"
// #include "Effects.h"
#define _USE_MATH_DEFINES
#include <chrono>
#include <cmath>
#include <math.h>
#include <omp.h>
#include <queue>
#include <thread>
#include <time.h>
#include <vector>

using namespace std;

/*
Author: Jeff Blake <jtblake@middlebury.edu>
*/

//============================================ Delay
juce::AudioParameterFloat *feedback;
juce::AudioParameterInt *delay;
juce::AudioParameterBool *delay_bypass;
//============================================ End Delay

//============================================ Distortion
juce::AudioParameterFloat* dist_start;
juce::AudioParameterFloat* ramp;
juce::AudioParameterFloat* dist_dryWet;
juce::AudioParameterBool *dist_bypass;

float dist;
//============================================ End Distortion

//============================================ Bitcrush
juce::AudioParameterInt* downSrate;
juce::AudioParameterInt* bitStart;
juce::AudioParameterFloat* bitRamp;
juce::AudioParameterFloat* bc_dryWet;
juce::AudioParameterBool *bit_bypass;

int bitDepth;
float bitDepthFloat;
//============================================ End Bitcrush

//============================================ Lowpass
juce::AudioParameterInt* fcut;
juce::AudioParameterBool *filter_bypass;
//============================================ End Lowpass

chrono::microseconds avgBlock;
std::mutex mutex;
int avgCount = 0;
double srate;

int purge_count = 0;
// Maybe make a bit higher
const int PURGE_LIMIT = 200;

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ) {
  // documented (along with get/set stateInformation) here:
  // https://docs.juce.com/master/tutorial_audio_parameter.html
  addParameter(feedback = new juce::AudioParameterFloat("feedback", "Feedback", 0.0f, .95f, 0.5f));
  addParameter(delay = new juce::AudioParameterInt("delay", "Delay", 10, 2000, 500));
  addParameter(delay_bypass = new juce::AudioParameterBool("del_bypass", "Delay Bypass", false));
  
  addParameter(dist_start = new juce::AudioParameterFloat("dStart", "Distortion Start", 0.0f, 100.0f, 0.5f));
  addParameter(ramp = new juce::AudioParameterFloat("DRamp", "Distortion Ramp", 0.0f, 0.1f, 0.01f));
  addParameter(dist_dryWet = new juce::AudioParameterFloat("DistDW", "Distortion Dry/Wet", 0.0f, 1.0f, 0.0f));
  addParameter(dist_bypass = new juce::AudioParameterBool("dist_bypass", "Distortion Bypass", false));
  dist = *dist_start;

  addParameter(downSrate = new juce::AudioParameterInt("DSRate", "Downsample Rate", 1024, 48000, 48000));
  addParameter(bitStart = new juce::AudioParameterInt("bitStart", "Bit Start", 0, 32, 32));
  addParameter(bitRamp = new juce::AudioParameterFloat("bRamp", "Bit Ramp", 0.0f, 4.0f, 1.0f));
  addParameter(bc_dryWet = new juce::AudioParameterFloat("BCDW", "Bitcrush Dry/Wet", 0.0f, 1.0f, 0.0f));
  addParameter(bit_bypass = new juce::AudioParameterBool("bit_bypass", "Bitcrush Bypass", false));
  bitDepth = *bitStart;
  bitDepthFloat = (float) *bitStart;
  
  addParameter(fcut = new juce::AudioParameterInt("fcut", "Filter Cutoff", 100, 20000, 20000));
  addParameter(filter_bypass = new juce::AudioParameterBool("filter_bypass", "Filter Bypass", false));

  // distortion = Distort::Distort(ramp, dist_dryWet);
  // bitcrush = Bitcrush::Bitcrush(downSrate, bitDepth, bc_dryWet);
  // Define threads here
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() {
  // Break threads down
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const {
  return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AudioPluginAudioProcessor::getNumPrograms() {
  return 1; // NB: some hosts don't cope very well if you tell them there are 0
            // programs, so this should be at least 1, even if you're not really
            // implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }

void AudioPluginAudioProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index,
                                                  const juce::String &newName) {
  juce::ignoreUnused(index, newName);
}

//==============================================================================

// tip to grab samplerate here from
// https://forum.juce.com/t/how-to-access-the-audio-streams-sampling-rate-in-juce-program/35844/6
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate,
                                              int samplesPerBlock) {
  // Use this method as the place to do any pre-playback
  // initialisation that you need..
  juce::ignoreUnused(sampleRate, samplesPerBlock);
  srate = sampleRate;
}

void AudioPluginAudioProcessor::releaseResources() {
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}

// just kind of adds buzz, needs white noise
// float applyNoise(float in, float noiseAmt) {
//     return in + ((rand()/RAND_MAX) * noiseAmt - (2*noiseAmt));
// }

// converts ms to (most accurate, typically won't be exact) number of samples
int sizeInSamples(int msecs) { return (int)(srate * ((float)msecs / 1000)); }

// Input: in (signal), gainAmtDB (desired gain in DB)
void applyGain(float *out, float gainAmtDB) {
  // Seems to increase it by double the desired gain.......
  *out = (float)(gainAmtDB >= 0 ? *out * pow(10, gainAmtDB / 10)
                                : *out / pow(10, -gainAmtDB / 10));
}

float applyDistortion(float out) {
  return tanh((1 - *dist_dryWet + dist * *dist_dryWet) * out);
}

// Quantizes using modulo
float bit_reduction(float out, int num_bits) {
  int steps = (int)pow(2, num_bits);
  float stepSize = (float)2 / steps;
  return out - fmod(out, stepSize);
}

// void bitcrush(juce::AudioBuffer<float> &buffer, int numIn) {
//   static float prevSample[2];

//   float factor = (float) srate / *downSrate;

//   for (int channel = 0; channel < numIn; ++channel) {
//     auto *channelData = buffer.getWritePointer(channel);
//     juce::ignoreUnused(channelData);

//     for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
//       if (factor > 1) {
//         if (fmod((float) sample, factor) < 1) {
//           // cout << sample << "\nfactor: " << factor << "\nfmod: " << fmod((float) sample, factor) << "\nsample: ";
//           prevSample[channel] = channelData[sample];
//         }
//         // channelData[sample] = prevSample[channel] * (1 - *bc_dryWet) + bit_reduction(prevSample[channel], *bitDepth) * *bc_dryWet;
//         channelData[sample] = bit_reduction(prevSample[channel], bitDepth);
//       }
//       else {
//         // channelData[sample] = channelData[sample] * (1 - *bc_dryWet) + bit_reduction(channelData[sample], *bitDepth) * *bc_dryWet;
//         channelData[sample] = bit_reduction(channelData[sample], bitDepth);
//       }
//     }
//   }
// }

float sinc(float x) {
    float a = (float) sin(M_PI*x);
    float b = (float) M_PI*x;
    return a/b;
}

// translated from basic, changing Hamming window to Blackman for better freq response and added conversion b/t Hz and other frequency unit
// https://www.analog.com/media/en/technical-documentation/dsp-book/dsp_book_Ch16.pdf
void applyLowPass(juce::AudioBuffer<float> &buffer, int numIn, int fc) {

  // const int wLength = 64;
  // float window[wLength] = {};
  int wLength = 2 * buffer.getNumSamples() / 3;
  vector<float> window(wLength);
  // vector<vector<float>> window(2)

  // hold over 
  // TODO: input num stereo
  static vector<vector<float>> preBuffer(numIn);

  for (int i = 0; i < numIn; i++) {
    preBuffer[i].resize(wLength);
  }
  
  // [wLength] = {};


  float cutoff = (float) fc / (float) srate;
  // cutoff+=1.0;

  
  // buffer.getWritePointer(0);

  // cout << srate << ", " << fc << "\n";

// sinc(2fc(n−(N−1)/2)) = sin(2*pi*fc*(n-(N-1)/2))/(2*pi*fc*(n−(N−1)/2))
  // init window
  for (int i = 0; i < wLength; i++) {
    //  if (i == wLength/2)
    //   window[i] = (float) sin(2*M_PI*cutoff);
    //  else
    window[i] = sinc(2*cutoff*(i - (wLength - 1)/2.0f));
    window[i] *= (float) (0.42 - 0.5*cos(2*M_PI*i/wLength) + 0.08*cos(4*M_PI*i/wLength));

    // if (i == wLength/2)
    //   window[i] = (float) sin(2*M_PI*cutoff);
    // else
    //   window[i] = (float) (sin(2*M_PI*cutoff*(i-wLength/2))/(i-wLength/2));
  }

  float sum = 0;
  for (int i = 0; i < wLength; i++) {
    sum += window[i];
  }

  for (int i = 0; i < wLength; i++) {
    window[i] /= sum;
  }

  // cout << "[";
  // for (int i = 0 ; i < wLength; i++) { 
  //   cout << window[i] << ", ";
  // }
  // cout << "]\n\n\n";

  juce::AudioBuffer<float> inputBuffer(numIn, buffer.getNumSamples());

  for (int channel = 0; channel < numIn; channel++) {
    auto *channelData = buffer.getWritePointer(channel);

    inputBuffer.copyFrom(channel, 0, buffer, channel, 0, buffer.getNumSamples());
    auto *input = inputBuffer.getReadPointer(channel);
    juce::ignoreUnused(channelData);

    for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
      channelData[sample] = 0;
      for (int i = 0; i < wLength; i++) {
        if (sample < i) {
          // cout << "Sample: " << sample << ", prebuffer index: " << wLength + (sample - i) << ", i: " << i << "\n\n";
          channelData[sample] = channelData[sample] + preBuffer[channel][wLength + (sample - i)] * window[i];
        }
        else {
          // cout << preBuffer[wLength + (sample - i)] << "input(head) <- \n";
          // cout << "Sample: " << sample << ", sample - i: " << sample - i << ", i: " << i << "\n\n";
          channelData[sample] = channelData[sample] + input[sample - i] * window[i];
        }
      }
      // cout << input[sample] << "in(head) <- \n";
      // cout << channelData[sample] << "out(head) <- \n";
    }

    // for (int sample = wLength; sample < buffer.getNumSamples(); sample++) {
    //   channelData[sample] = 0;

    //   for (int i = 0; i < wLength; i++) {
    //     channelData[sample] = channelData[sample] + input[sample - i] * window[i];
    //   }
    //   // cout << input[sample] << "in(tail) <- \n";
    //   // cout << channelData[sample] << "out(tail) <- \n";

    // }

    // for (int i = 0; i < wLength; i++) {
    //   preBuffer[channel][i] = 0;
    // }
    
    for (int sample = buffer.getNumSamples() - wLength; sample < buffer.getNumSamples(); sample++) {
          preBuffer[channel][sample - (buffer.getNumSamples() - wLength)] = input[sample];
    }
  }


}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages) {
  // timing help (library reccomendation) from:
  // https://stackoverflow.com/questions/11062804/measuring-the-runtime-of-a-c-code
  auto start = chrono::system_clock::now();
  juce::ignoreUnused(midiMessages);

  juce::ScopedNoDenormals noDenormals;

  // By default, numIns is 2
  auto totalNumInputChannels = getTotalNumInputChannels();
  auto totalNumOutputChannels = getTotalNumOutputChannels();
  
  float FEEDBACK = *feedback;
  static vector<queue<float>> delayBuffers(totalNumInputChannels);

  // counts samples for effects timing purposes
  static int numSamples = 0;

  // In case we have more outputs than inputs, this code clears any output
  // channels that didn't contain input data, (because these aren't
  // guaranteed to be empty - they may contain garbage).
  // This is here to avoid people getting screaming feedback
  // when they first compile a plugin, but obviously you don't need to keep
  // this code if your algorithm always overwrites all the output channels.
  for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
    buffer.clear(i, 0, buffer.getNumSamples());

  // This is the place where you'd normally do the guts of your plugin's
  // audio processing...
  // Make sure to reset the state if your inner loop is processing
  // the samples and the outer loop is handling the channels.
  // Alternatively, you can process the samples with the channels
  // interleaved by keeping the same state.

  // TODO: Currently separate from delay. Incorporate the sample and hold and bitcrush in the delay loop
  // Probably just going to have to incorporate most of this (bitcrush, lowpass) directly into the loop and have toggle options
  // bitcrush(buffer, totalNumInputChannels);

  // Retool to take user (knob?) input in future
  int maxDelaySize = sizeInSamples(*delay);

  // change to vector with numIn
  float prevSample[2];
  float factor = (float) srate / *downSrate;

  // for (int channel = 0; channel < totalNumInputChannels; ++channel) {
  //   auto *channelData = buffer.getWritePointer(channel);
  //   for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
  //     channelData[sample] = 1.0f/384 * sample;
  //   }
  // }

  if (!*filter_bypass)
    applyLowPass(buffer, totalNumInputChannels, *fcut);

  for (int channel = 0; channel < totalNumInputChannels; ++channel) {
    auto *channelData = buffer.getWritePointer(channel);
    juce::ignoreUnused(channelData);

      for (int sample = 0; sample < buffer.getNumSamples(); sample++) {

        // ==================================== Bitcrush
        if (!*bit_bypass) {        
          if (factor > 1) {
            if (fmod((float) sample, factor) < 1) {
              prevSample[channel] = channelData[sample];
            }
            channelData[sample] = bit_reduction(prevSample[channel], bitDepth);
            // channelData[sample] = bit_reduction(prevSample[channel], *bitDepth);
          }
          else {
            channelData[sample] = bit_reduction(channelData[sample], bitDepth);
          }
        }
        // ==================================== End Bitcrush

        // ==================================== Distortion
        if (!*dist_bypass)
          channelData[sample] = applyDistortion(channelData[sample]);
        // ==================================== End Distortion

        // ==================================== Delay
        if (!*delay_bypass) {
          if (delayBuffers[channel].size() >= maxDelaySize) {
            // add delayed sound, push back into buffer
            float out = delayBuffers[channel].front() * FEEDBACK;
            if (!*bit_bypass)
              out = bit_reduction(out, bitDepth);
            if (!*dist_bypass)
              out = applyDistortion(out);

            channelData[sample] += out;

            // avoids long pauses when changing the delay buffer and windows of
            // samples that would otherwise get clogged in the delay buffer (e.g.
            // with a check like dBuff[size] > max + 5)
            while (delayBuffers[channel].size() > maxDelaySize) {
              if (purge_count < PURGE_LIMIT) {
                delayBuffers[channel].pop();
                purge_count++;
              } else {
                purge_count = 0;
                break;
              }
            }
          }
          delayBuffers[channel].push(channelData[sample]);

          numSamples++;
          if (!*bit_bypass && numSamples % maxDelaySize == 0 && bitDepthFloat - *bitRamp > 1) {
            // Iterate secondary parameters
            bitDepthFloat -= *bitRamp;
            if ((int) bitDepthFloat < bitDepth)
              bitDepth = (int) bitDepthFloat;
            numSamples = 0;

            // TODO: Add clamp parameter for how low the bitdepth can go
          }
        }        
        else if (!delayBuffers[channel].empty()){
          while (!delayBuffers[channel].empty()) { delayBuffers[channel].pop(); }
        }
        else {
          if (bitDepth != *bitStart) {
            bitDepth = *bitStart;
            bitDepthFloat = (float) *bitStart;
          }
          if (dist != *dist_start)
            dist = *dist_start;
          // cout << "Reset!\n\n";
        }
        // ==================================== End Delay

        // ==================================== Resets
        if (*bit_bypass && bitDepth != *bitStart) {
          bitDepthFloat = (float) *bitStart;
          bitDepth = *bitStart;
        }
        if (*dist_bypass && dist != *dist_start) {
          dist = *dist_start;
          cout << dist << "\n";
        }
        
      }
    //   if (*dist_dw > 0 && *dist_ramp > 0)
    //     dist += *dist_ramp;
  }

  // if (!*bypass){
  //   auto end = chrono::system_clock::now();
  //   auto elapsed =  end - start;
  //   chrono::microseconds elapsedMillis = chrono::duration_cast<
  //   chrono::microseconds >(elapsed); avgBlock += elapsedMillis; avgCount++;
  //   cout << avgBlock.count()/avgCount << "\n";
  // }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const {
  return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor() {
  // return new AudioPluginAudioProcessorEditor (*this);
  // generic for now......
  return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  // You should use this method to store your parameters in the memory block.
  // You could do that either as raw data, or use the XML or ValueTree classes
  // as intermediaries to make it easy to save and load complex data.
  // juce::ignoreUnused (destData);
  juce::MemoryOutputStream(destData, true).writeFloat(*feedback);
  juce::MemoryOutputStream(destData, true).writeInt(*delay);
  juce::MemoryOutputStream(destData, true).writeBool(*delay_bypass);

  juce::MemoryOutputStream(destData, true).writeFloat(*dist_start);
  juce::MemoryOutputStream(destData, true).writeFloat(*ramp);
  juce::MemoryOutputStream(destData, true).writeFloat(*dist_dryWet);
  juce::MemoryOutputStream(destData, true).writeBool(*dist_bypass);

  juce::MemoryOutputStream(destData, true).writeInt(*downSrate);
  juce::MemoryOutputStream(destData, true).writeInt(*bitStart);
  juce::MemoryOutputStream(destData, true).writeFloat(*bitRamp);
  juce::MemoryOutputStream(destData, true).writeFloat(*bc_dryWet);
  juce::MemoryOutputStream(destData, true).writeBool(*bit_bypass);

  juce::MemoryOutputStream(destData, true).writeInt(*fcut);
  juce::MemoryOutputStream(destData, true).writeBool(*filter_bypass);
}

void AudioPluginAudioProcessor::setStateInformation(const void *data,
                                                    int sizeInBytes) {
  // You should use this method to restore your parameters from this memory
  // block, whose contents will have been created by the getStateInformation()
  // call. juce::ignoreUnused (data, sizeInBytes);
  *feedback =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readFloat();
  *delay =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readInt();
  *delay_bypass =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readBool();

  *dist_start =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readFloat();
  *ramp =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readFloat();
  *dist_dryWet =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readFloat();
  *dist_bypass =
        juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readBool();
  
  *downSrate =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readInt();
  *bitStart =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readInt();
  *bitRamp =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readFloat();
  *bc_dryWet =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readFloat();
  *bit_bypass = 
        juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readBool();

  *fcut =
      juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readInt();
  *filter_bypass =
        juce::MemoryInputStream(data, static_cast<size_t>(sizeInBytes), false)
          .readBool();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new AudioPluginAudioProcessor();
}
