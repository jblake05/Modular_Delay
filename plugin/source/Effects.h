#include "../include/Modular_Delay/PluginProcessor.h"
#include "../include/Modular_Delay/PluginEditor.h"

void applyGain(float *out, float gainAmtDB);

void applyDistortion(float *out, float dist, float dw);

void bit_reduction(float *out, int num_bits);

void downSample(juce::AudioBuffer<float>& buffer, int numIn, int factor);

void applyLowPass(juce::AudioBuffer<float>& buffer, int numIn, double srate, double freq);
