#include "Common.h"
#include "BuiltInEffectModuleAudioPlugin.h"
#include "BuiltInSynthsPluginFormat.h"
#include <atomic>
#include <cmath>

class BuiltInEffectModuleParameter final : public AudioPluginInstance::HostedParameter
{
public:
    BuiltInEffectModuleParameter(const String &i, const String &n, float lo, float hi, float def)
        : id(i), name(n), range(lo, hi), defaultValue(range.convertTo0to1(def)), value(defaultValue) {}
    String getParameterID() const override { return id; }
    float getValue() const override { return value.load(std::memory_order_relaxed); }
    void setValue(float v) override
    {
        const float normalized = jlimit(0.0f, 1.0f, v);
        value.store(normalized, std::memory_order_relaxed);
        sendValueChangedMessageToListeners(normalized);
    }
    float getDefaultValue() const override { return defaultValue; }
    String getName(int maxLength) const override { return name.substring(0, jmax(0, maxLength)); }
    String getLabel() const override { return {}; }
    String getText(float v, int maxLength) const override
    {
        const float actual = range.convertFrom0to1(jlimit(0.0f, 1.0f, v));
        return String(actual, (range.end - range.start) > 100.0f ? 0 : 2)
            .substring(0, jmax(0, maxLength));
    }
    float getValueForText(const String &s) const override
    {
        return range.convertTo0to1(jlimit(range.start, range.end, s.getFloatValue()));
    }
    float get() const noexcept { return range.convertFrom0to1(value.load(std::memory_order_relaxed)); }
private:
    const String id, name;
    const NormalisableRange<float> range;
    const float defaultValue;
    std::atomic<float> value;
};

namespace
{
    constexpr float effectTwoPi = MathConstants<float>::effectTwoPi;
    constexpr const char *moduleIds[BuiltInEffectModuleAudioPlugin::moduleCount] = {
        "helio.effect.eq", "helio.effect.compressor", "helio.effect.gate",
        "helio.effect.limiter", "helio.effect.reverb", "helio.effect.delay",
        "helio.effect.chorus", "helio.effect.flanger", "helio.effect.phaser",
        "helio.effect.vibrato", "helio.effect.tremolo", "helio.effect.saturation",
        "helio.effect.bitcrusher", "helio.effect.stereo", "helio.effect.auto-wah"
    };
}

BuiltInEffectModuleAudioPlugin::Type BuiltInEffectModuleAudioPlugin::getTypeForIndex(int i)
{
    return static_cast<Type>(jlimit(0, moduleCount - 1, i));
}
String BuiltInEffectModuleAudioPlugin::getModuleIdentifier(int i)
{
    return String(moduleIds[jlimit(0, moduleCount - 1, i)]);
}
String BuiltInEffectModuleAudioPlugin::getModuleName(Type t)
{
    switch (t)
    {
        case Type::equalizer: return "Helio EQ";
        case Type::compressor: return "Helio Compressor";
        case Type::gate: return "Helio Noise Gate";
        case Type::limiter: return "Helio Limiter";
        case Type::reverb: return "Helio Reverb";
        case Type::delay: return "Helio Delay";
        case Type::chorus: return "Helio Chorus";
        case Type::flanger: return "Helio Flanger";
        case Type::phaser: return "Helio Phaser";
        case Type::vibrato: return "Helio Vibrato";
        case Type::tremolo: return "Helio Tremolo";
        case Type::saturation: return "Helio Saturation";
        case Type::bitcrusher: return "Helio Bitcrusher";
        case Type::stereo: return "Helio Stereo";
        case Type::autoWah: return "Helio Auto-Wah";
    }
    return "Helio Effect";
}

BuiltInEffectModuleAudioPlugin::BuiltInEffectModuleAudioPlugin(Type t) : type(t)
{
    setPlayConfigDetails(2, 2, 44100.0, 512);
    switch (type)
    {
        case Type::equalizer:
            addParameter("cutoff", "Low cut (Hz)", 20, 1000, 25);
            addParameter("low", "Low shelf (dB)", -18, 18, 0);
            addParameter("midfreq", "Mid frequency (Hz)", 100, 10000, 1200);
            addParameter("midgain", "Mid gain (dB)", -18, 18, 0);
            addParameter("high", "High shelf (dB)", -18, 18, 0); break;
        case Type::compressor:
            addParameter("threshold", "Threshold (dB)", -48, 0, -18);
            addParameter("ratio", "Ratio", 1, 20, 3);
            addParameter("attack", "Attack (ms)", 0.1f, 100, 10);
            addParameter("release", "Release (ms)", 5, 1000, 120);
            addParameter("makeup", "Makeup gain (dB)", -12, 24, 0); break;
        case Type::gate:
            addParameter("threshold", "Threshold (dB)", -90, -20, -60);
            addParameter("release", "Release (ms)", 5, 800, 100);
            addParameter("floor", "Closed level", 0, 1, 0.05f); break;
        case Type::limiter:
            addParameter("ceiling", "Ceiling (dB)", -12, 0, -1);
            addParameter("release", "Release (ms)", 5, 500, 60); break;
        case Type::reverb:
            addParameter("room", "Room size", 0.1f, 1, 0.55f);
            addParameter("damping", "Damping", 0, 1, 0.35f);
            addParameter("mix", "Wet", 0, 1, 0.25f); break;
        case Type::delay:
            addParameter("time", "Time (ms)", 1, 1500, 250);
            addParameter("feedback", "Feedback", 0, 0.92f, 0.3f);
            addParameter("mix", "Wet", 0, 1, 0.25f);
            addParameter("pingpong", "Ping-pong", 0, 1, 0); break;
        case Type::chorus:
            addParameter("rate", "Rate (Hz)", 0.05f, 5, 0.3f);
            addParameter("depth", "Depth", 0, 1, 0.5f);
            addParameter("mix", "Wet", 0, 1, 0.25f); break;
        case Type::flanger:
            addParameter("rate", "Rate (Hz)", 0.05f, 5, 0.25f);
            addParameter("depth", "Depth", 0, 1, 0.5f);
            addParameter("feedback", "Feedback", -0.9f, 0.9f, 0);
            addParameter("mix", "Wet", 0, 1, 0.35f); break;
        case Type::phaser:
            addParameter("rate", "Rate (Hz)", 0.05f, 5, 0.3f);
            addParameter("depth", "Depth", 0, 1, 0.6f);
            addParameter("mix", "Wet", 0, 1, 0.5f); break;
        case Type::vibrato:
            addParameter("rate", "Rate (Hz)", 0.1f, 10, 5);
            addParameter("depth", "Depth (ms)", 0.2f, 12, 3);
            addParameter("mix", "Wet", 0, 1, 0.65f); break;
        case Type::tremolo:
            addParameter("rate", "Rate (Hz)", 0.1f, 15, 4);
            addParameter("depth", "Depth", 0, 1, 0.5f); break;
        case Type::saturation:
            addParameter("drive", "Drive (dB)", 0, 30, 8);
            addParameter("mix", "Wet", 0, 1, 1);
            addParameter("output", "Output (dB)", -12, 12, 0); break;
        case Type::bitcrusher:
            addParameter("bits", "Bit depth", 4, 16, 8);
            addParameter("hold", "Sample reduction", 1, 32, 1); break;
        case Type::stereo:
            addParameter("width", "Stereo width", 0, 2, 1);
            addParameter("pan", "Pan", -1, 1, 0); break;
        case Type::autoWah:
            addParameter("sensitivity", "Sensitivity", 0.05f, 1, 0.35f);
            addParameter("mix", "Wet", 0, 1, 0.5f); break;
    }
}

void BuiltInEffectModuleAudioPlugin::addParameter(const String &id, const String &name,
    float lo, float hi, float def)
{
    auto *p = new BuiltInEffectModuleParameter(id, name, lo, hi, def);
    parameters.push_back(p);
    addHostedParameter(std::unique_ptr<AudioPluginInstance::HostedParameter>(p));
}
float BuiltInEffectModuleAudioPlugin::parameter(int i) const noexcept
{
    return i >= 0 && i < static_cast<int>(parameters.size()) ?
        parameters[static_cast<size_t>(i)]->get() : 0.0f;
}
const String BuiltInEffectModuleAudioPlugin::getName() const { return getModuleName(type); }

void BuiltInEffectModuleAudioPlugin::fillInPluginDescription(PluginDescription &d) const
{
    d.name = getName();
    d.descriptiveName = d.name;
    d.uniqueId = d.name.hashCode();
    d.category = "Effect";
    d.pluginFormatName = BuiltInSynthsPluginFormat::formatName;
    d.fileOrIdentifier = BuiltInSynthsPluginFormat::formatIdentifier;
    d.manufacturerName = "Helio";
    d.version = "1.0";
    d.isInstrument = false;
    d.numInputChannels = 2;
    d.numOutputChannels = 2;
}

void BuiltInEffectModuleAudioPlugin::prepareToPlay(double sampleRate, int blockSize)
{
    ignoreUnused(blockSize);
    currentSampleRate = jmax(8000.0, sampleRate);
    delayBuffer.assign(static_cast<size_t>(currentSampleRate * 2.1) + 16, {});
    writePosition = 0;
    envelope[0] = envelope[1] = filterState[0] = filterState[1] = 0;
    dampingState[0] = dampingState[1] = heldSamples[0] = heldSamples[1] = 0;
    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < 4; ++s) allpassState[ch][s] = 0;
    phase = 0;
    holdCounter = 0;
    eqPeak = makePeak(static_cast<float>(currentSampleRate), parameter(2), parameter(3), 0.85f);
    eqPeakState = {};
}
void BuiltInEffectModuleAudioPlugin::releaseResources() { delayBuffer.clear(); }

BuiltInEffectModuleAudioPlugin::Biquad BuiltInEffectModuleAudioPlugin::makePeak(
    float sr, float frequency, float gainDb, float q)
{
    const float f = jlimit(10.0f, sr * 0.45f, frequency);
    const float a = std::pow(10.0f, gainDb / 40.0f);
    const float w = effectTwoPi * f / sr, c = std::cos(w);
    const float alpha = std::sin(w) / (2.0f * jmax(0.1f, q));
    const float a0 = 1.0f + alpha / a;
    return { (1.0f + alpha * a) / a0, -2.0f * c / a0,
        (1.0f - alpha * a) / a0, -2.0f * c / a0,
        (1.0f - alpha / a) / a0 };
}
float BuiltInEffectModuleAudioPlugin::runBiquad(float x, const Biquad &b, BiquadState &s)
{
    const float y = b.b0 * x + s.z1;
    s.z1 = b.b1 * x - b.a1 * y + s.z2;
    s.z2 = b.b2 * x - b.a2 * y;
    return y;
}

void BuiltInEffectModuleAudioPlugin::processBlock(AudioBuffer<float> &buffer, MidiBuffer &midi)
{
    ignoreUnused(midi);
    ScopedNoDenormals noDenormals;
    const int channels = jmin(2, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();
    if (channels == 0 || delayBuffer.empty()) return;
    const float sr = static_cast<float>(currentSampleRate);
    const size_t size = delayBuffer.size();

    if (type == Type::equalizer)
        eqPeak = makePeak(sr, parameter(2), parameter(3), 0.85f);

    const float attackMs = type == Type::compressor ? parameter(2) : 5.0f;
    const float releaseMs = type == Type::compressor ? parameter(3) :
        (type == Type::gate || type == Type::limiter ? parameter(1) : 100.0f);
    const float attack = std::exp(-1.0f / (jmax(0.1f, attackMs) * 0.001f * sr));
    const float release = std::exp(-1.0f / (jmax(1.0f, releaseMs) * 0.001f * sr));

    for (int i = 0; i < samples; ++i)
    {
        const float lfo = 0.5f + 0.5f * std::sin(phase);
        const float inL = buffer.getSample(0, i);
        const float inR = channels > 1 ? buffer.getSample(1, i) : inL;

        for (int ch = 0; ch < channels; ++ch)
        {
            float x = buffer.getSample(ch, i);
            const float magnitude = std::abs(x);
            switch (type)
            {
                case Type::equalizer:
                {
                    const float hpAlpha = 1.0f - std::exp(-effectTwoPi * parameter(0) / sr);
                    filterState[ch] += hpAlpha * (x - filterState[ch]);
                    x -= filterState[ch];
                    const float lowAlpha = 1.0f - std::exp(-effectTwoPi * 180.0f / sr);
                    dampingState[ch] += lowAlpha * (x - dampingState[ch]);
                    x += dampingState[ch] * (Decibels::decibelsToGain(parameter(1)) - 1.0f);
                    const float highAlpha = 1.0f - std::exp(-effectTwoPi * 3500.0f / sr);
                    filterState[ch] += highAlpha * (x - filterState[ch]);
                    x += (x - filterState[ch]) * (Decibels::decibelsToGain(parameter(4)) - 1.0f);
                    x = runBiquad(x, eqPeak, eqPeakState[static_cast<size_t>(ch)]);
                    break;
                }
                case Type::compressor:
                {
                    const float coeff = magnitude > envelope[ch] ? attack : release;
                    envelope[ch] = coeff * envelope[ch] + (1.0f - coeff) * magnitude;
                    const float level = Decibels::gainToDecibels(jmax(1.0e-7f, envelope[ch]));
                    const float over = level - parameter(0);
                    const float ratio = jmax(1.0f, parameter(1));
                    const float reduction = over > 0 ? -over * (1.0f - 1.0f / ratio) : 0;
                    x *= Decibels::decibelsToGain(reduction + parameter(4));
                    break;
                }
                case Type::gate:
                {
                    const float coeff = magnitude > envelope[ch] ? 0.0f : release;
                    envelope[ch] = coeff * envelope[ch] + (1.0f - coeff) * magnitude;
                    if (envelope[ch] < Decibels::decibelsToGain(parameter(0)))
                        x *= parameter(2);
                    break;
                }
                case Type::limiter:
                {
                    const float coeff = magnitude > envelope[ch] ? 0.0f : release;
                    envelope[ch] = coeff * envelope[ch] + (1.0f - coeff) * magnitude;
                    const float ceiling = Decibels::decibelsToGain(parameter(0));
                    if (envelope[ch] > ceiling) x *= ceiling / jmax(1.0e-7f, envelope[ch]);
                    break;
                }
                case Type::reverb:
                {
                    const int d1 = roundToInt(sr * 0.0297f), d2 = roundToInt(sr * 0.0371f);
                    const int d3 = roundToInt(sr * 0.0411f);
                    const size_t r1 = (writePosition + size - static_cast<size_t>(d1)) % size;
                    const size_t r2 = (writePosition + size - static_cast<size_t>(d2)) % size;
                    const size_t r3 = (writePosition + size - static_cast<size_t>(d3)) % size;
                    const float wet = (delayBuffer[r1][ch] + delayBuffer[r2][ch] + delayBuffer[r3][ch]) / 3.0f;
                    dampingState[ch] += (wet - dampingState[ch]) * (1.0f - parameter(1) * 0.92f);
                    delayBuffer[writePosition][ch] = x + dampingState[ch] * (0.48f + parameter(0) * 0.38f);
                    x = x * (1.0f - parameter(2)) + wet * parameter(2);
                    break;
                }
                case Type::delay:
                {
                    const int d = jlimit(1, static_cast<int>(size) - 2,
                        roundToInt(parameter(0) * 0.001f * sr));
                    const size_t read = (writePosition + size - static_cast<size_t>(d)) % size;
                    const float delayed = delayBuffer[read][ch];
                    const float cross = delayBuffer[read][1 - ch];
                    delayBuffer[writePosition][ch] = x + parameter(1) *
                        (delayed * (1.0f - parameter(3)) + cross * parameter(3));
                    x = x * (1.0f - parameter(2)) + delayed * parameter(2);
                    break;
                }
                case Type::chorus:
                case Type::flanger:
                case Type::vibrato:
                {
                    float delayMs = 14.0f + 12.0f * parameter(1) * lfo;
                    float mix = parameter(2);
                    if (type == Type::flanger)
                    {
                        delayMs = 1.0f + 9.0f * parameter(1) * lfo;
                        mix = parameter(3);
                    }
                    else if (type == Type::vibrato)
                        delayMs = 2.0f + parameter(1) * lfo;
                    const int d = jlimit(1, static_cast<int>(size) - 2,
                        roundToInt(delayMs * 0.001f * sr));
                    const size_t read = (writePosition + size - static_cast<size_t>(d)) % size;
                    const float delayed = delayBuffer[read][ch];
                    const float feedback = type == Type::flanger ? parameter(2) : 0.0f;
                    delayBuffer[writePosition][ch] = x + delayed * feedback;
                    x = type == Type::vibrato ? x * (1.0f - mix) + delayed * mix :
                        x * (1.0f - mix) + ((x + delayed) * 0.5f) * mix;
                    break;
                }
                case Type::phaser:
                {
                    float wet = x;
                    const float a = jlimit(0.05f, 0.85f, 0.12f + parameter(1) * (0.35f + lfo * 0.35f));
                    for (int stage = 0; stage < 4; ++stage)
                    {
                        const float y = -a * wet + allpassState[ch][stage];
                        allpassState[ch][stage] = wet + a * y;
                        wet = y;
                    }
                    x += (wet - x) * parameter(2);
                    break;
                }
                case Type::tremolo:
                    x *= 1.0f - parameter(1) * lfo;
                    break;
                case Type::saturation:
                {
                    const float drive = Decibels::decibelsToGain(parameter(0));
                    const float wet = std::tanh(x * drive) / jmax(0.001f, std::tanh(drive));
                    x = (x * (1.0f - parameter(1)) + wet * parameter(1)) *
                        Decibels::decibelsToGain(parameter(2));
                    break;
                }
                case Type::bitcrusher:
                {
                    if (holdCounter == 0) heldSamples[ch] = x;
                    const float levels = std::pow(2.0f, parameter(0) - 1.0f);
                    x = std::round(heldSamples[ch] * levels) / levels;
                    break;
                }
                case Type::stereo: break;
                case Type::autoWah:
                {
                    const float coeff = magnitude > envelope[ch] ? 0.02f : 0.001f;
                    envelope[ch] += coeff * (magnitude - envelope[ch]);
                    const float cutoff = 350.0f + jlimit(0.0f, 1.0f,
                        envelope[ch] * parameter(0) * 8.0f) * 3600.0f;
                    const float alpha = 1.0f - std::exp(-effectTwoPi * cutoff / sr);
                    filterState[ch] += alpha * (x - filterState[ch]);
                    const float wet = x + (x - filterState[ch]) * 2.2f;
                    x = x * (1.0f - parameter(1)) + wet * parameter(1);
                    break;
                }
            }
            buffer.setSample(ch, i, x);
        }

        if (type == Type::stereo && channels > 1)
        {
            const float mid = 0.5f * (inL + inR);
            const float side = 0.5f * (inL - inR) * parameter(0);
            const float pan = parameter(1);
            buffer.setSample(0, i, (mid + side) * (pan > 0 ? 1.0f - pan : 1.0f));
            buffer.setSample(1, i, (mid - side) * (pan < 0 ? 1.0f + pan : 1.0f));
        }

        if (type == Type::bitcrusher)
            holdCounter = (holdCounter + 1) % jmax(1, roundToInt(parameter(1)));
        if (type == Type::reverb || type == Type::delay || type == Type::chorus ||
            type == Type::flanger || type == Type::vibrato)
            writePosition = (writePosition + 1) % size;
        if (type == Type::chorus || type == Type::flanger || type == Type::phaser ||
            type == Type::vibrato || type == Type::tremolo)
        {
            phase += effectTwoPi * parameter(0) / sr;
            if (phase >= effectTwoPi) phase -= effectTwoPi;
        }
    }
}

double BuiltInEffectModuleAudioPlugin::getTailLengthSeconds() const
{
    return type == Type::reverb ? 3.0 : (type == Type::delay ? 2.0 : 0.1);
}
bool BuiltInEffectModuleAudioPlugin::acceptsMidi() const { return false; }
bool BuiltInEffectModuleAudioPlugin::producesMidi() const { return false; }
AudioProcessorEditor *BuiltInEffectModuleAudioPlugin::createEditor()
{
    return new GenericAudioProcessorEditor(*this);
}
bool BuiltInEffectModuleAudioPlugin::hasEditor() const { return true; }
int BuiltInEffectModuleAudioPlugin::getNumPrograms() { return 1; }
int BuiltInEffectModuleAudioPlugin::getCurrentProgram() { return 0; }
void BuiltInEffectModuleAudioPlugin::setCurrentProgram(int) {}
const String BuiltInEffectModuleAudioPlugin::getProgramName(int) { return "Default"; }
void BuiltInEffectModuleAudioPlugin::changeProgramName(int, const String &) {}
void BuiltInEffectModuleAudioPlugin::getStateInformation(MemoryBlock &destData)
{
    XmlElement state("HelioEffectModule");
    for (size_t i = 0; i < parameters.size(); ++i)
        state.setAttribute("p" + String(static_cast<int>(i)), parameters[i]->getValue());
    copyXmlToBinary(state, destData);
}
void BuiltInEffectModuleAudioPlugin::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> state(getXmlFromBinary(data, sizeInBytes));
    if (state == nullptr || !state->hasTagName("HelioEffectModule")) return;
    for (size_t i = 0; i < parameters.size(); ++i)
    {
        const String key = "p" + String(static_cast<int>(i));
        if (state->hasAttribute(key))
            parameters[i]->setValueNotifyingHost(
                jlimit(0.0f, 1.0f, static_cast<float>(state->getDoubleAttribute(key))));
    }
}
