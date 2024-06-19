#pragma once

#include "PluginProcessor.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    juce::Slider bitSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bitSliderAttachment;

    // void linkSlider(juce::Slider *slider, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment, string id);

    AudioPluginAudioProcessor& processorRef;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
