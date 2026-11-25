/*
    This file is part of Helio music sequencer.
    Distributed under the GNU General Public License v3 or later.
*/
#pragma once

class EffectsParameter;

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
    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    };
    struct BiquadState { float z1 = 0.0f, z2 = 0.0f; };

    static Biquad makePeak(float sampleRate, float frequency, float gainDb, float q);
    static Biquad makeShelf(float sampleRate, float frequency, float gainDb, bool highShelf);
    static Biquad makePass(float sampleRate, float frequency, bool highPass);
    static float runBiquad(float input, const Biquad &coefficients, BiquadState &state);

    EffectsParameter *bass = nullptr, *mid = nullptr, *treble = nullptr;
    EffectsParameter *threshold = nullptr, *ratio = nullptr;
    EffectsParameter *delayMix = nullptr, *delayTime = nullptr, *delayFeedback = nullptr;
    EffectsParameter *reverbMix = nullptr, *chorusMix = nullptr, *drive = nullptr;
    EffectsParameter *outputGain = nullptr;
    EffectsParameter *highPass = nullptr, *lowPass = nullptr;
    EffectsParameter *gateThreshold = nullptr, *limiterThreshold = nullptr;
    EffectsParameter *tremoloDepth = nullptr, *bitDepth = nullptr;
    EffectsParameter *pan = nullptr, *stereoWidth = nullptr;

    EffectsParameter *eqLowFreq = nullptr, *eqLowGain = nullptr;
    EffectsParameter *eqHighFreq = nullptr, *eqHighGain = nullptr;
    std::array<EffectsParameter *, 5> eqFreq{}, eqGain{}, eqQ{};

    EffectsParameter *compressorAttack = nullptr, *compressorRelease = nullptr;
    EffectsParameter *compressorKnee = nullptr, *compressorMakeup = nullptr;
    EffectsParameter *flangerMix = nullptr, *flangerRate = nullptr, *flangerDepth = nullptr, *flangerFeedback = nullptr;
    EffectsParameter *phaserMix = nullptr, *phaserRate = nullptr, *phaserDepth = nullptr;
    EffectsParameter *vibratoMix = nullptr, *vibratoRate = nullptr, *vibratoDepth = nullptr;
    EffectsParameter *autoWahMix = nullptr, *autoWahSensitivity = nullptr;
    EffectsParameter *tremoloRate = nullptr, *delayPingPong = nullptr;
    EffectsParameter *reverbRoom = nullptr, *reverbDamping = nullptr, *reverbPreDelay = nullptr;

    std::vector<std::array<float, 2>> delayLine, reverbLine, chorusLine, flangerLine;
    size_t delayPosition = 0, reverbPosition = 0, chorusPosition = 0, flangerPosition = 0;
    double currentSampleRate = 44100.0;
    float envelope[2] = {}, gateEnvelope[2] = {};
    float chorusPhase = 0.0f, flangerPhase = 0.0f, phaserPhase = 0.0f;
    float vibratoPhase = 0.0f, tremoloPhase = 0.0f;
    float phaserState[2][4] = {};
    float wahLowState[2] = {}, reverbDampState[2] = {};
    std::array<Biquad, 9> eqCoefficients{};
    std::array<std::array<BiquadState, 2>, 9> eqStates{};
    Biquad highPassCoefficients, lowPassCoefficients;
    std::array<BiquadState, 2> highPassStates{}, lowPassStates{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BuiltInEffectsAudioPlugin)
};
