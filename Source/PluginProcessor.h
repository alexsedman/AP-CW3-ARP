/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class APCW3ARPAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    APCW3ARPAudioProcessor();
    ~APCW3ARPAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void extracted(int noteDuration, int numSamples);
    
void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
private:
    //==============================================================================
    // Initialise variables for MIDI control.
    int currNoteIndex, prevNoteIndex, noteOutIndex, arpPolarity; // Note indexing info for the note array.
    int midiSampleIndex; // A variable relating the sample stream to MIDI stream.
    int arpSkip; // Variable for the 'skip' mode of the arpeggiator.
    juce::SortedSet<int> noteArray; // The note array.

    //==============================================================================
    // Arpeggiator functions.
    void fillNoteArray(juce::MidiBuffer& midiMessages);
    void checkMidiOutput(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    void assignArpMode();
    
    //==============================================================================
    // Declare the createParameter() function, which will set up all parameters.
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    // Naming the APVTS object.
    juce::AudioProcessorValueTreeState params;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (APCW3ARPAudioProcessor)
};
