/*
    This file is part of Helio music sequencer.
    Distributed under the GNU General Public License v3 or later.
*/
#pragma once
#include <vector>
#include <array>

class BuiltInEffectModuleParameter;

class BuiltInEffectModuleAudioPlugin final : public AudioPluginInstance
{
public:
    enum class Type { equalizer, compressor, gate, limiter, reverb, delay, chorus, flanger,
        phaser, vibrato, tremolo, saturation, bitcrusher, stereo, autoWah };
    static constexpr int moduleCount = 15;

    explicit BuiltInEffectModuleAudioPlugin(Type type);
    static Type getTypeForIndex(int index);
    static String getModuleIdentifier(int index);
    static String getModuleName(Type type);

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
    struct Biquad { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct BiquadState { float z1 = 0, z2 = 0; };
    void addParameter(const String &, const String &, float, float, float);
    float parameter(int index) const noexcept;
    static Biquad makePeak(float, float, float, float);
    static float runBiquad(float, const Biquad &, BiquadState &);

    const Type type;
    std::vector<BuiltInEffectModuleParameter *> parameters;
    std::vector<std::array<float, 2>> delayBuffer;
    size_t writePosition = 0;
    double currentSampleRate = 44100.0;
    float envelope[2] = {}, filterState[2] = {}, dampingState[2] = {};
    float allpassState[2][4] = {}, heldSamples[2] = {};
    float phase = 0;
    int holdCounter = 0;
    Biquad eqPeak;
    std::array<BiquadState, 2> eqPeakState{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BuiltInEffectModuleAudioPlugin)
};
