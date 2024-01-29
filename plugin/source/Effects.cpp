#include "../include/Modular_Delay/PluginEditor.h"
#include "../include/Modular_Delay/PluginProcessor.h"
#include <math.h>

// Input: in (signal), gainAmtDB (desired gain in DB)
void applyGain(float *out, float gainAmtDB) {
  // Seems to increase it by double the desired gain.......
  *out = (float)(gainAmtDB >= 0 ? *out * pow(10, gainAmtDB / 10)
                                : *out / pow(10, -gainAmtDB / 10));
}

void applyDistortion(float *out, float dist, float dw) {
  *out = tanh((1 - dw + dist * dw) * *out);
}

// Quantizes using modulo
void bit_reduction(float *out, int num_bits) {
  int steps = (int)pow(2, num_bits);
  float stepSize = (float)2 / steps;
  *out -= fmod(*out, stepSize);

  // cout << *out;
  // *out = closestNum;
  // cout << ", " << *out << "\n";
}

void downSample(juce::AudioBuffer<float> &buffer, int numIn, int factor) {
  float prevSample[2];

  for (int channel = 0; channel < numIn; ++channel) {
    auto *channelData = buffer.getWritePointer(channel);
    juce::ignoreUnused(channelData);

    for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
      if (factor != 1) {
        if (sample % factor == 0)
          prevSample[channel] = channelData[sample];
        else
          channelData[sample] = prevSample[channel];
      }
    }
  }
}

// translated from
// https://en.wikipedia.org/wiki/Low-pass_filter#Simple_infinite_impulse_response_filter
void applyLowPass(juce::AudioBuffer<float> &buffer, double srate, int numIn,
                  double freq) {

  double speriod = 1 / srate;
  double alpha = (2 * M_PI * speriod * freq) / (2 * M_PI * speriod * freq + 1);

  double RC = speriod * ((1 - alpha) / alpha);

  for (int channel; channel < numIn; channel++) {
    auto *channelData = buffer.getWritePointer(channel);
    auto *channelRead = buffer.getReadPointer(channel);
    juce::ignoreUnused(channelData);

    float prevInput = channelData[0];
    channelData[0] *= alpha;

    for (int sample = 1; sample < buffer.getNumSamples(); sample++) {
      channelData[sample] =
          alpha * channelData[sample] + (1 - alpha) * channelData[sample - 1];
    }
  }
}

// auto* channel
//     short prevInput = buffer[channel][2];
// }