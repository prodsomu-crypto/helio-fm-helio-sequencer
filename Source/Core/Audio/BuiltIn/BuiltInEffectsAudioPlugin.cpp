#include "Common.h"
#include "BuiltInEffectsAudioPlugin.h"
#include "BuiltInSynthsPluginFormat.h"

const String BuiltInEffectsAudioPlugin::pluginId = "helio.effects.rack";
const String BuiltInEffectsAudioPlugin::pluginName = "Helio Effects Rack";

BuiltInEffectsAudioPlugin::BuiltInEffectsAudioPlugin()
{
    this->setPlayConfigDetails(2, 2, 44100.0, 512);
    const NormalisableRange<float> dbRange(-18.0f, 18.0f, 0.1f);
    this->bass = new AudioParameterFloat("bass", "Bass", dbRange, 0.0f);
    this->mid = new AudioParameterFloat("mid", "Mid", dbRange, 0.0f);
    this->treble = new AudioParameterFloat("treble", "Treble", dbRange, 0.0f);
    this->threshold = new AudioParameterFloat("threshold", "Compressor threshold", -48.0f, 0.0f, -18.0f);
    this->ratio = new AudioParameterFloat("ratio", "Compressor ratio", 1.0f, 12.0f, 1.0f);
    this->delayMix = new AudioParameterFloat("delayMix", "Delay mix", 0.0f, 1.0f, 0.0f);
    this->delayTime = new AudioParameterFloat("delayTime", "Delay time (ms)", 1.0f, 1000.0f, 250.0f);
    this->delayFeedback = new AudioParameterFloat("delayFeedback", "Delay feedback", 0.0f, 0.9f, 0.25f);
    this->reverbMix = new AudioParameterFloat("reverbMix", "Reverb mix", 0.0f, 1.0f, 0.0f);
    this->chorusMix = new AudioParameterFloat("chorusMix", "Chorus mix", 0.0f, 1.0f, 0.0f);
    this->drive = new AudioParameterFloat("drive", "Distortion drive", 0.0f, 24.0f, 0.0f);
    this->outputGain = new AudioParameterFloat("outputGain", "Output gain (dB)", -24.0f, 12.0f, 0.0f);
    this->highPass = new AudioParameterFloat("highPass", "High-pass cutoff (Hz)", 20.0f, 2000.0f, 20.0f);
    this->lowPass = new AudioParameterFloat("lowPass", "Low-pass cutoff (Hz)", 100.0f, 20000.0f, 20000.0f);
    this->gateThreshold = new AudioParameterFloat("gateThreshold", "Noise gate threshold (dB)", -90.0f, -20.0f, -90.0f);
    this->limiterThreshold = new AudioParameterFloat("limiterThreshold", "Limiter ceiling (dB)", -12.0f, 0.0f, 0.0f);
    this->tremoloDepth = new AudioParameterFloat("tremoloDepth", "Tremolo depth", 0.0f, 1.0f, 0.0f);
    this->bitDepth = new AudioParameterFloat("bitDepth", "Bitcrusher depth (bits)", 4.0f, 16.0f, 16.0f);
    this->pan = new AudioParameterFloat("pan", "Pan", -1.0f, 1.0f, 0.0f);
    this->stereoWidth = new AudioParameterFloat("stereoWidth", "Stereo width", 0.0f, 2.0f, 1.0f);

    this->addParameter(this->bass);
    this->addParameter(this->mid);
    this->addParameter(this->treble);
    this->addParameter(this->threshold);
    this->addParameter(this->ratio);
    this->addParameter(this->delayMix);
    this->addParameter(this->delayTime);
    this->addParameter(this->delayFeedback);
    this->addParameter(this->reverbMix);
    this->addParameter(this->chorusMix);
    this->addParameter(this->drive);
    this->addParameter(this->outputGain);
    this->addParameter(this->highPass);
    this->addParameter(this->lowPass);
    this->addParameter(this->gateThreshold);
    this->addParameter(this->limiterThreshold);
    this->addParameter(this->tremoloDepth);
    this->addParameter(this->bitDepth);
    this->addParameter(this->pan);
    this->addParameter(this->stereoWidth);
}

const String BuiltInEffectsAudioPlugin::getName() const { return pluginName; }

void BuiltInEffectsAudioPlugin::fillInPluginDescription(PluginDescription &description) const
{
    description.name = this->getName();
    description.descriptiveName = description.name;
    description.uniqueId = description.name.hashCode();
    description.category = "Effect";
    description.pluginFormatName = BuiltInSynthsPluginFormat::formatName;
    description.fileOrIdentifier = BuiltInSynthsPluginFormat::formatIdentifier;
    description.manufacturerName = "Built-in";
    description.version = "1.0";
    description.isInstrument = false;
    description.numInputChannels = 2;
    description.numOutputChannels = 2;
}

void BuiltInEffectsAudioPlugin::prepareToPlay(double sampleRate, int estimatedSamplesPerBlock)
{
    this->currentSampleRate = jmax(1.0, sampleRate);
    const auto maxDelay = static_cast<size_t>(this->currentSampleRate * 1.1) + 4;
    this->delayLine.assign(maxDelay, {});
    this->reverbLine.assign(static_cast<size_t>(this->currentSampleRate * 0.19) + 4, {});
    this->chorusLine.assign(static_cast<size_t>(this->currentSampleRate * 0.06) + 4, {});
    this->delayPosition = this->reverbPosition = this->chorusPosition = 0;
    this->lowCoeff = 1.0f - std::exp(-2.0f * MathConstants<float>::pi * 250.0f / static_cast<float>(this->currentSampleRate));
    this->highCoeff = 1.0f - std::exp(-2.0f * MathConstants<float>::pi * 4200.0f / static_cast<float>(this->currentSampleRate));
    this->lowState[0] = this->lowState[1] = this->highState[0] = this->highState[1] = 0.0f;
    this->envelope[0] = this->envelope[1] = 0.0f;
    this->highPassState[0] = this->highPassState[1] = 0.0f;
    this->lowPassState[0] = this->lowPassState[1] = 0.0f;
    this->tremoloPhase = 0.0f;
}

void BuiltInEffectsAudioPlugin::releaseResources()
{
    this->delayLine.clear();
    this->reverbLine.clear();
    this->chorusLine.clear();
}

void BuiltInEffectsAudioPlugin::processBlock(AudioBuffer<float> &buffer, MidiBuffer &midiMessages)
{
    ScopedNoDenormals noDenormals;
    const int channels = jmin(2, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();
    if (channels == 0 || this->delayLine.empty() || this->reverbLine.empty() || this->chorusLine.empty())
        return;

    const float bassGain = Decibels::decibelsToGain(this->bass->get());
    const float midGain = Decibels::decibelsToGain(this->mid->get());
    const float trebleGain = Decibels::decibelsToGain(this->treble->get());
    const float thresholdGain = Decibels::decibelsToGain(this->threshold->get());
    const float compressionRatio = jmax(1.0f, this->ratio->get());
    const float dMix = this->delayMix->get();
    const float dFeedback = this->delayFeedback->get();
    const int dSamples = jlimit(1, static_cast<int>(this->delayLine.size()) - 1,
        roundToInt(this->delayTime->get() * 0.001f * static_cast<float>(this->currentSampleRate)));
    const float rMix = this->reverbMix->get();
    const float cMix = this->chorusMix->get();
    const float driveGain = Decibels::decibelsToGain(this->drive->get());
    const float outGain = Decibels::decibelsToGain(this->outputGain->get());
    const float hpHz = this->highPass->get();
    const float lpHz = this->lowPass->get();
    const float hpCoeff = 1.0f - std::exp(-2.0f * MathConstants<float>::pi * hpHz / static_cast<float>(this->currentSampleRate));
    const float lpCoeff = 1.0f - std::exp(-2.0f * MathConstants<float>::pi * lpHz / static_cast<float>(this->currentSampleRate));
    const float gateGain = Decibels::decibelsToGain(this->gateThreshold->get());
    const float limiterGain = Decibels::decibelsToGain(this->limiterThreshold->get());
    const float tremolo = this->tremoloDepth->get();
    const float quantizeLevels = std::pow(2.0f, jmax(4.0f, this->bitDepth->get()) - 1.0f);
    const float attack = std::exp(-1.0f / (0.010f * static_cast<float>(this->currentSampleRate)));
    const float release = std::exp(-1.0f / (0.120f * static_cast<float>(this->currentSampleRate)));
    const int reverbDelay = jmax(1, static_cast<int>(this->currentSampleRate * 0.071));
    const float chorusRate = 0.22f / static_cast<float>(this->currentSampleRate);

    for (int i = 0; i < samples; ++i)
    {
        const float lfo = 0.5f + 0.5f * std::sin(this->chorusPhase);
        this->chorusPhase += 2.0f * MathConstants<float>::pi * 0.32f / static_cast<float>(this->currentSampleRate);
        if (this->chorusPhase > 2.0f * MathConstants<float>::pi)
            this->chorusPhase -= 2.0f * MathConstants<float>::pi;

        for (int ch = 0; ch < channels; ++ch)
        {
            float x = buffer.getSample(ch, i);

            // Simple high-pass and low-pass tone filters.
            this->highPassState[ch] += hpCoeff * (x - this->highPassState[ch]);
            x = x - this->highPassState[ch];
            this->lowPassState[ch] += lpCoeff * (x - this->lowPassState[ch]);
            x = this->lowPassState[ch];

            // Three-band tone control using two inexpensive one-pole crossovers.
            this->lowState[ch] += this->lowCoeff * (x - this->lowState[ch]);
            this->highState[ch] += this->highCoeff * (x - this->highState[ch]);
            const float low = this->lowState[ch];
            const float high = x - this->highState[ch];
            const float middle = x - low - high;
            x = low * bassGain + middle * midGain + high * trebleGain;

            // Smooth peak-envelope compressor.
            const float magnitude = std::abs(x);
            const float coeff = magnitude > this->envelope[ch] ? attack : release;
            this->envelope[ch] = coeff * this->envelope[ch] + (1.0f - coeff) * magnitude;
            if (this->envelope[ch] > thresholdGain && compressionRatio > 1.0f)
            {
                const float compressed = thresholdGain + (this->envelope[ch] - thresholdGain) / compressionRatio;
                x *= compressed / jmax(0.00001f, this->envelope[ch]);
            }

            if (this->gateThreshold->get() > -89.9f && this->envelope[ch] < gateGain)
                x *= 0.15f;

            // Tempo-independent feedback delay.
            const size_t dRead = (this->delayPosition + this->delayLine.size() - static_cast<size_t>(dSamples)) % this->delayLine.size();
            const float delayed = this->delayLine[dRead][static_cast<size_t>(ch)];
            this->delayLine[this->delayPosition][static_cast<size_t>(ch)] = x + delayed * dFeedback;
            x = x * (1.0f - dMix) + delayed * dMix;

            // Compact room reverb: short cross-feedback stereo delay network.
            const size_t rRead = (this->reverbPosition + this->reverbLine.size() - static_cast<size_t>(reverbDelay)) % this->reverbLine.size();
            const float reverberated = this->reverbLine[rRead][static_cast<size_t>(ch)];
            const float cross = this->reverbLine[rRead][static_cast<size_t>(1 - ch)];
            this->reverbLine[this->reverbPosition][static_cast<size_t>(ch)] = x + (0.62f * reverberated + 0.08f * cross);
            x = x * (1.0f - rMix) + reverberated * rMix;

            // Modulated short delay for chorus.
            const int chorusDelay = jlimit(1, static_cast<int>(this->chorusLine.size()) - 1,
                roundToInt((0.012f + 0.018f * lfo) * static_cast<float>(this->currentSampleRate)));
            const size_t cRead = (this->chorusPosition + this->chorusLine.size() - static_cast<size_t>(chorusDelay)) % this->chorusLine.size();
            const float chorused = this->chorusLine[cRead][static_cast<size_t>(ch)];
            this->chorusLine[this->chorusPosition][static_cast<size_t>(ch)] = x;
            x = x * (1.0f - cMix) + (0.5f * (x + chorused)) * cMix;

            if (driveGain > 1.001f)
                x = std::tanh(x * driveGain) / std::tanh(driveGain);

            if (tremolo > 0.0001f)
                x *= 1.0f - tremolo * (0.5f + 0.5f * std::sin(this->tremoloPhase));

            if (this->bitDepth->get() < 15.99f)
                x = std::round(x * quantizeLevels) / quantizeLevels;

            x = jlimit(-limiterGain, limiterGain, x);
            buffer.setSample(ch, i, x * outGain);
        }

        this->delayPosition = (this->delayPosition + 1) % this->delayLine.size();
        this->reverbPosition = (this->reverbPosition + 1) % this->reverbLine.size();
        this->chorusPosition = (this->chorusPosition + 1) % this->chorusLine.size();
        this->tremoloPhase += 2.0f * MathConstants<float>::pi * 4.0f / static_cast<float>(this->currentSampleRate);
        if (this->tremoloPhase > 2.0f * MathConstants<float>::pi)
            this->tremoloPhase -= 2.0f * MathConstants<float>::pi;

        if (channels == 2)
        {
            const float left = buffer.getSample(0, i);
            const float right = buffer.getSample(1, i);
            const float mid = 0.5f * (left + right);
            const float side = 0.5f * (left - right) * this->stereoWidth->get();
            const float p = this->pan->get();
            buffer.setSample(0, i, (mid + side) * (p > 0.0f ? 1.0f - p : 1.0f));
            buffer.setSample(1, i, (mid - side) * (p < 0.0f ? 1.0f + p : 1.0f));
        }
    }
}

double BuiltInEffectsAudioPlugin::getTailLengthSeconds() const { return 2.5; }
bool BuiltInEffectsAudioPlugin::acceptsMidi() const { return false; }
bool BuiltInEffectsAudioPlugin::producesMidi() const { return false; }
AudioProcessorEditor *BuiltInEffectsAudioPlugin::createEditor() { return new GenericAudioProcessorEditor(*this); }
bool BuiltInEffectsAudioPlugin::hasEditor() const { return true; }
int BuiltInEffectsAudioPlugin::getNumPrograms() { return 1; }
int BuiltInEffectsAudioPlugin::getCurrentProgram() { return 0; }
void BuiltInEffectsAudioPlugin::setCurrentProgram(int) {}
const String BuiltInEffectsAudioPlugin::getProgramName(int) { return "Default"; }
void BuiltInEffectsAudioPlugin::changeProgramName(int, const String &) {}

void BuiltInEffectsAudioPlugin::getStateInformation(MemoryBlock &destData)
{
    XmlElement state("HelioEffects");
    const auto parameters = this->getParameters();
    for (int i = 0; i < parameters.size(); ++i)
        state.setAttribute("p" + String(i), parameters.getUnchecked(i)->getValue());
    copyXmlToBinary(state, destData);
}

void BuiltInEffectsAudioPlugin::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> state(getXmlFromBinary(data, sizeInBytes));
    if (state == nullptr || !state->hasTagName("HelioEffects"))
        return;
    const auto parameters = this->getParameters();
    for (int i = 0; i < parameters.size(); ++i)
    {
        const auto key = "p" + String(i);
        if (state->hasAttribute(key))
            parameters.getUnchecked(i)->setValueNotifyingHost(
                jlimit(0.0f, 1.0f, static_cast<float>(state->getDoubleAttribute(key))));
    }
}
