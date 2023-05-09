/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
APCW3ARPAudioProcessor::APCW3ARPAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                      #endif
                     #endif
                       ),
                       // Instantiating the constructor layout for the params object.
                       params (*this, nullptr, juce::Identifier("PARAMETERS"), createParameters())
#endif
{
}

APCW3ARPAudioProcessor::~APCW3ARPAudioProcessor()
{
}

//==============================================================================
const juce::String APCW3ARPAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool APCW3ARPAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool APCW3ARPAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool APCW3ARPAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double APCW3ARPAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int APCW3ARPAudioProcessor::getNumPrograms()
{
    return 1;
}

int APCW3ARPAudioProcessor::getCurrentProgram()
{
    return 0;
}

void APCW3ARPAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String APCW3ARPAudioProcessor::getProgramName (int index)
{
    return {};
}

void APCW3ARPAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void APCW3ARPAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Define preparatory settings for the plugin.
    noteArray.clear();
    currNoteIndex = 0;
    prevNoteIndex = -1; // The previous note index here is set to -1 to represent the previously outputted note.
    midiSampleIndex = 0;
    arpSkip = 0;
}

void APCW3ARPAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool APCW3ARPAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
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

void APCW3ARPAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    jassert (buffer.getNumChannels() == 0); // Force 0 channels for output.
    
    fillNoteArray(midiMessages); // Fill the note array from the MIDI input.
    checkMidiOutput(buffer, midiMessages); // Check whether the MIDI output needs updating, based on tempo information.
}

//==============================================================================
// Fills an array with the pressed notes.
void APCW3ARPAudioProcessor::fillNoteArray(juce::MidiBuffer& midiMessages)
{
    // For each note event in the MIDI buffer, add 'Note On' data to the sorted set of notes, and remove 'Note Off' data;
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage(); // Gets incoming MIDI stream messages.

        // This conditional adds pressed notes to the note array, and removes any unpressed ones.
        if (message.isNoteOn())
            noteArray.add (message.getNoteNumber());
        else if (message.isNoteOff())
            noteArray.removeValue (message.getNoteNumber());
    }
    
    // Clear the MIDI buffer...
    midiMessages.clear();
}

//==============================================================================
// This section organises the time of each note in relation to sample length, and checks when to trigger MIDIon and MIDIoff values.
// If the time counter and audio buffer samples is greater than or equal to note duration, then a note change occurs during the block.
void APCW3ARPAudioProcessor::checkMidiOutput(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Initialise parameters...
    auto noteDuration = static_cast<int> ((60 * getSampleRate()) / params.getRawParameterValue("TEMPO")->load());
    auto numSamples = buffer.getNumSamples();
    
    // This requires further calculation, within this conditional...
    if ((midiSampleIndex + numSamples) >= noteDuration)
    {
        // The note change sample point is calculated via a buffer offset value.
        auto offset = juce::jmax (0, juce::jmin ((int) (noteDuration - midiSampleIndex), numSamples - 1));
        
        // If the previous note is on, then a note off message must be sent at the offsetted sample point.
        if (prevNoteIndex > 0)
        {
            midiMessages.addEvent (juce::MidiMessage::noteOff (1, prevNoteIndex), offset);
            prevNoteIndex = -1; // Previous note value is reset to -1.
        }

        // If there are notes pressed (thus stored in the note array), then MIDI output messages are sent...
        if (noteArray.size() > 0)
        {
            assignArpMode(); // This function calculates the note index to output, based on the selected arp mode (definition is below).
            prevNoteIndex = noteArray[noteOutIndex];
            midiMessages.addEvent (juce::MidiMessage::noteOn  (1, prevNoteIndex, (juce::uint8) 127), offset); // Output the selected note from the array.
        }
    }
    
    midiSampleIndex = (midiSampleIndex + numSamples) % noteDuration; // Tick the sample index.
}

//==============================================================================
// Organising the note set using a switch/case
void APCW3ARPAudioProcessor::assignArpMode()
{
    int mode = static_cast<int>(params.getRawParameterValue("MODE")->load());
    
    switch (mode)
    {
        // In the 'Up' case, positive arp polarity is enforced...
        case 0:
            currNoteIndex = (currNoteIndex + 1) % noteArray.size(); // This simply increments through the note array.
            noteOutIndex = currNoteIndex; // Index increments alongside the note array.
            break;
        
        // In the 'Down' case, negative arp polarity is enforced...
        case 1:
            currNoteIndex = (currNoteIndex + 1) % noteArray.size();
            noteOutIndex = noteArray.indexOf(noteArray.getLast()) - currNoteIndex; // This shuffles the index backwards through the array.
            break;
        
        //In the 'UpDown' case, arp polarity switches at the start and end of the note array...
        case 2:
            // If the note index is 0...
            if (currNoteIndex == noteArray.indexOf(noteArray.getFirst()))
                arpPolarity = 1; // Polarity is set to ascending
            // If the note index is the final note in the array...
            else if (currNoteIndex == noteArray.indexOf(noteArray.getLast()))
                arpPolarity = -1; // Polarity is set to descending
            
            currNoteIndex = (currNoteIndex + arpPolarity) % noteArray.size();
            noteOutIndex = currNoteIndex;
            break;
            
        // In the 'Skip' case, the arpeggiator jumps up two notes, then falls back down one note.
        case 3:
            // This conditional chooses whether the arp should +2 notes, or -1 note. This will alternate on each pass.
            if (arpSkip == 2)
                arpSkip = -1;
            else
                arpSkip = 2;
            
            currNoteIndex = (currNoteIndex + arpSkip) % noteArray.size();
            // Special conditional required if the note index drops to -1, in which the top note is selected instead.
            if (currNoteIndex < 0)
                noteOutIndex = noteArray.indexOf(noteArray.getLast());
            else
                noteOutIndex = currNoteIndex;
            break;
    }
}

//==============================================================================
bool APCW3ARPAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* APCW3ARPAudioProcessor::createEditor()
{
    //return new APCW3ARPAudioProcessorEditor (*this); // NOTE: for debug purposes
    // By calling upon the GenericAudioProcessorEditor, a basic UI can be outputted, neatly displaying parameters, and adjusting for the desired DAW UI.
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void APCW3ARPAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
}

void APCW3ARPAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new APCW3ARPAudioProcessor();
    
}

//==============================================================================
// Value tree state object. Returns a vector with all parameter elements.
juce::AudioProcessorValueTreeState::ParameterLayout APCW3ARPAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    params.push_back (std::make_unique<juce::AudioParameterInt>(juce::ParameterID("TEMPO", 1), "Tempo", 40, 300, 200, "BPM"));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("MODE", 1), "Mode", juce::StringArray{"Up", "Down", "UpDown", "Skip"}, 0));
    
    return {params.begin(), params.end()};
}
