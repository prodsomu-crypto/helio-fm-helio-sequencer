/*
    This file is part of Helio music sequencer.
    Distributed under the GNU General Public License v3 or later.
*/
#pragma once

#include <array>
#include <vector>

class BuiltInEffectsAudioPlugin final : public AudioPluginInstance
{
public:
    BuiltInEffectsAudioPlugin();
    static const String pluginId;
    static const String pluginName;

    const String getName() const override;
    void fillInPluginDescription(PluginDescription &description) const override;
    void prepareToPlay(double sampleRate, int estimatedSamplesPerBlock) override;
    void releaseResources() override;
    void processBlock(AudioBuffer<float> &buffer, MidiBuffer &midiMessages) override;
    double getTailLengthSeconds() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const String getProgramName(int index) override;
    void changeProgramName(int index, const String &newName) override;
    void getStateInformation(MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

private:
    AudioParameterFloat *bass = nullptr, *mid = nullptr, *treble = nullptr;
    AudioParameterFloat *threshold = nullptr, *ratio = nullptr;
    AudioParameterFloat *delayMix = nullptr, *delayTime = nullptr, *delayFeedback = nullptr;
    AudioParameterFloat *reverbMix = nullptr, *chorusMix = nullptr, *drive = nullptr;
    AudioParameterFloat *outputGain = nullptr;

    std::vector<std::array<float, 2>> delayLine, reverbLine, chorusLine;
    size_t delayPosition = 0, reverbPosition = 0, chorusPosition = 0;
    double currentSampleRate = 44100.0;
    float lowState[2] = {}, highState[2] = {}, envelope[2] = {};
    float chorusPhase = 0.0f;
    float lowCoeff = 0.0f, highCoeff = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BuiltInEffectsAudioPlugin)
};
