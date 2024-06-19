#include "../include/Modular_Delay/PluginEditor.h"
#include "../include/Modular_Delay/PluginProcessor.h"
// #include "../include/Modular_Delay/juce_Slider.h"

// #include "PluginProcessor.h"
// #include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(
    AudioPluginAudioProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p) {
  juce::ignoreUnused(processorRef);

  bitSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
  bitSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 50);
  addAndMakeVisible(bitSlider);

  bitSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.treeState, "BITCRUSH_START", bitSlider);

  // linkSlider(&bitSlider, &bitSliderAttachment, "BITCRUSH_START");

  // Make sure that before the constructor has finished, you've set the
  // editor's size to whatever you need it to be.
  setSize(400, 300);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g) {

  // TODO: Parameter knobs for feedback and delay time
  // (Our component is opaque, so we must completely fill the background with a
  // solid colour)
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  // juce::Slider

  g.setColour(juce::Colours::white);
  g.setFont(15.0f);
  g.drawFittedText("Hello World!", getLocalBounds(),
                   juce::Justification::centred, 1);
}

void AudioPluginAudioProcessorEditor::resized() {
  // This is generally where you'll want to lay out the positions of any
  // subcomponents in your editor..
  bitSlider.setBounds(getWidth()/2 - 100, getHeight()/2 - 50, 200, 100);
}

// TODO: Add to editor.h file, add parameters for slider and attachment, extract bitslider attachment code into here, generalize to all sliders.
// void linkSlider(juce::Slider *slider, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment, string id) {
//   slider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
//   slider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 50);
//   addAndMakeVisible(slider);

//   attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processorRef.treeState, id, slider);
// }