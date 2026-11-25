#include "Common.h"
#include "BuiltInEffectsAudioPlugin.h"
#include "BuiltInSynthsPluginFormat.h"

const String BuiltInEffectsAudioPlugin::pluginId = "helio.effects.rack";
const String BuiltInEffectsAudioPlugin::pluginName = "Helio Effects Rack";

namespace
{
    constexpr float twoPi = MathConstants<float>::twoPi;
    float safeFrequency(float f, float sr) { return jlimit(10.0f, sr * 0.45f, f); }
}

BuiltInEffectsAudioPlugin::Biquad BuiltInEffectsAudioPlugin::makePeak(float sr, float frequency, float gainDb, float q)
{
    const float f = safeFrequency(frequency, sr);
    const float a = std::pow(10.0f, gainDb / 40.0f);
    const float w = twoPi * f / sr;
    const float c = std::cos(w), alpha = std::sin(w) / (2.0f * jmax(0.1f, q));
    const float a0 = 1.0f + alpha / a;
    return { (1.0f + alpha * a) / a0, (-2.0f * c) / a0,
             (1.0f - alpha * a) / a0, (-2.0f * c) / a0,
             (1.0f - alpha / a) / a0 };
}

BuiltInEffectsAudioPlugin::Biquad BuiltInEffectsAudioPlugin::makeShelf(float sr, float frequency, float gainDb, bool highShelf)
{
    const float f = safeFrequency(frequency, sr);
    const float a = std::pow(10.0f, gainDb / 40.0f);
    const float w = twoPi * f / sr, c = std::cos(w), s = std::sin(w);
    const float alpha = s * 0.5f * std::sqrt(2.0f);
    const float beta = 2.0f * std::sqrt(a) * alpha;
    float b0, b1, b2, a0, a1, a2;
    if (!highShelf)
    {
        b0 = a * ((a + 1.0f) - (a - 1.0f) * c + beta);
        b1 = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * c);
        b2 = a * ((a + 1.0f) - (a - 1.0f) * c - beta);
        a0 = (a + 1.0f) + (a - 1.0f) * c + beta;
        a1 = -2.0f * ((a - 1.0f) + (a + 1.0f) * c);
        a2 = (a + 1.0f) + (a - 1.0f) * c - beta;
    }
    else
    {
        b0 = a * ((a + 1.0f) + (a - 1.0f) * c + beta);
        b1 = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * c);
        b2 = a * ((a + 1.0f) + (a - 1.0f) * c - beta);
        a0 = (a + 1.0f) - (a - 1.0f) * c + beta;
        a1 = 2.0f * ((a - 1.0f) - (a + 1.0f) * c);
        a2 = (a + 1.0f) - (a - 1.0f) * c - beta;
    }
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

BuiltInEffectsAudioPlugin::Biquad BuiltInEffectsAudioPlugin::makePass(float sr, float frequency, bool highPass)
{
    const float f = safeFrequency(frequency, sr);
    const float w = twoPi * f / sr, c = std::cos(w);
    const float alpha = std::sin(w) * 0.70710678f;
    const float a0 = 1.0f + alpha;
    const float sign = highPass ? -1.0f : 1.0f;
    return { highPass ? (1.0f + c) * 0.5f / a0 : (1.0f - c) * 0.5f / a0,
             highPass ? -(1.0f + c) / a0 : (1.0f - c) / a0,
             highPass ? (1.0f + c) * 0.5f / a0 : (1.0f - c) * 0.5f / a0,
             -2.0f * c / a0, (1.0f - alpha) / a0 };
}

float BuiltInEffectsAudioPlugin::runBiquad(float x, const Biquad &b, BiquadState &s)
{
    const float y = b.b0 * x + s.z1;
    s.z1 = b.b1 * x - b.a1 * y + s.z2;
    s.z2 = b.b2 * x - b.a2 * y;
    return y;
}

BuiltInEffectsAudioPlugin::BuiltInEffectsAudioPlugin()
{
    this->setPlayConfigDetails(2, 2, 44100.0, 512);
    const NormalisableRange<float> dbRange(-18.0f, 18.0f, 0.1f);
    this->bass = new AudioParameterFloat("bass", "Legacy bass", dbRange, 0.0f);
    this->mid = new AudioParameterFloat("mid", "Legacy mid", dbRange, 0.0f);
    this->treble = new AudioParameterFloat("treble", "Legacy treble", dbRange, 0.0f);
    this->threshold = new AudioParameterFloat("threshold", "Compressor threshold (dB)", -48.0f, 0.0f, -18.0f);
    this->ratio = new AudioParameterFloat("ratio", "Compressor ratio", 1.0f, 20.0f, 1.0f);
    this->delayMix = new AudioParameterFloat("delayMix", "Delay wet", 0.0f, 1.0f, 0.0f);
    this->delayTime = new AudioParameterFloat("delayTime", "Delay time (ms)", 1.0f, 1500.0f, 250.0f);
    this->delayFeedback = new AudioParameterFloat("delayFeedback", "Delay feedback", 0.0f, 0.92f, 0.25f);
    this->reverbMix = new AudioParameterFloat("reverbMix", "Reverb wet", 0.0f, 1.0f, 0.0f);
    this->chorusMix = new AudioParameterFloat("chorusMix", "Chorus wet", 0.0f, 1.0f, 0.0f);
    this->drive = new AudioParameterFloat("drive", "Saturation drive (dB)", 0.0f, 30.0f, 0.0f);
    this->outputGain = new AudioParameterFloat("outputGain", "Output gain (dB)", -24.0f, 12.0f, 0.0f);
    this->highPass = new AudioParameterFloat("highPass", "High-pass (Hz)", 20.0f, 2000.0f, 20.0f);
    this->lowPass = new AudioParameterFloat("lowPass", "Low-pass (Hz)", 100.0f, 20000.0f, 20000.0f);
    this->gateThreshold = new AudioParameterFloat("gateThreshold", "Gate threshold (dB)", -90.0f, -20.0f, -90.0f);
    this->limiterThreshold = new AudioParameterFloat("limiterThreshold", "Limiter ceiling (dB)", -12.0f, 0.0f, 0.0f);
    this->tremoloDepth = new AudioParameterFloat("tremoloDepth", "Tremolo depth", 0.0f, 1.0f, 0.0f);
    this->bitDepth = new AudioParameterFloat("bitDepth", "Bitcrusher depth (bits)", 4.0f, 16.0f, 16.0f);
    this->pan = new AudioParameterFloat("pan", "Pan", -1.0f, 1.0f, 0.0f);
    this->stereoWidth = new AudioParameterFloat("stereoWidth", "Stereo width", 0.0f, 2.0f, 1.0f);

    this->eqLowFreq = new AudioParameterFloat("eqLowFreq", "EQ low shelf frequency (Hz)", 30.0f, 500.0f, 100.0f);
    this->eqLowGain = new AudioParameterFloat("eqLowGain", "EQ low shelf gain (dB)", dbRange, 0.0f);
    this->eqHighFreq = new AudioParameterFloat("eqHighFreq", "EQ high shelf frequency (Hz)", 2000.0f, 18000.0f, 10000.0f);
    this->eqHighGain = new AudioParameterFloat("eqHighGain", "EQ high shelf gain (dB)", dbRange, 0.0f);
    const float frequencies[5] = { 120.0f, 350.0f, 1000.0f, 3500.0f, 9000.0f };
    for (int i = 0; i < 5; ++i)
    {
        const String suffix = String(i + 1);
        this->eqFreq[static_cast<size_t>(i)] = new AudioParameterFloat("eqFreq" + suffix, "EQ band " + suffix + " frequency (Hz)", 20.0f, 20000.0f, frequencies[i]);
        this->eqGain[static_cast<size_t>(i)] = new AudioParameterFloat("eqGain" + suffix, "EQ band " + suffix + " gain (dB)", dbRange, 0.0f);
        this->eqQ[static_cast<size_t>(i)] = new AudioParameterFloat("eqQ" + suffix, "EQ band " + suffix + " Q", 0.25f, 8.0f, 1.0f);
    }

    this->compressorAttack = new AudioParameterFloat("compAttack", "Compressor attack (ms)", 0.1f, 100.0f, 10.0f);
    this->compressorRelease = new AudioParameterFloat("compRelease", "Compressor release (ms)", 5.0f, 1000.0f, 120.0f);
    this->compressorKnee = new AudioParameterFloat("compKnee", "Compressor soft knee (dB)", 0.0f, 18.0f, 6.0f);
    this->compressorMakeup = new AudioParameterFloat("compMakeup", "Compressor makeup (dB)", -12.0f, 24.0f, 0.0f);

    this->flangerMix = new AudioParameterFloat("flangerMix", "Flanger wet", 0.0f, 1.0f, 0.0f);
    this->flangerRate = new AudioParameterFloat("flangerRate", "Flanger rate (Hz)", 0.05f, 5.0f, 0.25f);
    this->flangerDepth = new AudioParameterFloat("flangerDepth", "Flanger depth", 0.0f, 1.0f, 0.5f);
    this->flangerFeedback = new AudioParameterFloat("flangerFeedback", "Flanger feedback", -0.9f, 0.9f, 0.0f);
    this->phaserMix = new AudioParameterFloat("phaserMix", "Phaser wet", 0.0f, 1.0f, 0.0f);
    this->phaserRate = new AudioParameterFloat("phaserRate", "Phaser rate (Hz)", 0.05f, 5.0f, 0.3f);
    this->phaserDepth = new AudioParameterFloat("phaserDepth", "Phaser depth", 0.0f, 1.0f, 0.6f);
    this->vibratoMix = new AudioParameterFloat("vibratoMix", "Vibrato wet", 0.0f, 1.0f, 0.0f);
    this->vibratoRate = new AudioParameterFloat("vibratoRate", "Vibrato rate (Hz)", 0.1f, 10.0f, 5.0f);
    this->vibratoDepth = new AudioParameterFloat("vibratoDepth", "Vibrato depth (ms)", 0.2f, 12.0f, 3.0f);
    this->autoWahMix = new AudioParameterFloat("autoWahMix", "Auto-wah wet", 0.0f, 1.0f, 0.0f);
    this->autoWahSensitivity = new AudioParameterFloat("autoWahSens", "Auto-wah sensitivity", 0.05f, 1.0f, 0.35f);
    this->tremoloRate = new AudioParameterFloat("tremoloRate", "Tremolo rate (Hz)", 0.1f, 15.0f, 4.0f);
    this->delayPingPong = new AudioParameterFloat("delayPingPong", "Delay ping-pong", 0.0f, 1.0f, 0.0f);
    this->reverbRoom = new AudioParameterFloat("reverbRoom", "Reverb room size", 0.1f, 1.0f, 0.55f);
    this->reverbDamping = new AudioParameterFloat("reverbDamping", "Reverb damping", 0.0f, 1.0f, 0.35f);
    this->reverbPreDelay = new AudioParameterFloat("reverbPreDelay", "Reverb pre-delay (ms)", 0.0f, 120.0f, 18.0f);

    const auto add = [this](AudioParameterFloat *p) { this->addParameter(p); };
    for (auto *p : { this->bass, this->mid, this->treble, this->threshold, this->ratio,
        this->delayMix, this->delayTime, this->delayFeedback, this->reverbMix, this->chorusMix,
        this->drive, this->outputGain, this->highPass, this->lowPass, this->gateThreshold,
        this->limiterThreshold, this->tremoloDepth, this->bitDepth, this->pan, this->stereoWidth,
        this->eqLowFreq, this->eqLowGain, this->eqHighFreq, this->eqHighGain,
        this->compressorAttack, this->compressorRelease, this->compressorKnee, this->compressorMakeup,
        this->flangerMix, this->flangerRate, this->flangerDepth, this->flangerFeedback,
        this->phaserMix, this->phaserRate, this->phaserDepth, this->vibratoMix, this->vibratoRate,
        this->vibratoDepth, this->autoWahMix, this->autoWahSensitivity, this->tremoloRate,
        this->delayPingPong, this->reverbRoom, this->reverbDamping, this->reverbPreDelay })
        add(p);
    for (int i = 0; i < 5; ++i)
    {
        add(this->eqFreq[static_cast<size_t>(i)]);
        add(this->eqGain[static_cast<size_t>(i)]);
        add(this->eqQ[static_cast<size_t>(i)]);
    }
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
    description.version = "2.0";
    description.isInstrument = false;
    description.numInputChannels = 2;
    description.numOutputChannels = 2;
}

void BuiltInEffectsAudioPlugin::prepareToPlay(double sampleRate, int estimatedSamplesPerBlock)
{
    ignoreUnused(estimatedSamplesPerBlock);
    this->currentSampleRate = jmax(8000.0, sampleRate);
    const auto sr = static_cast<float>(this->currentSampleRate);
    const auto maxDelay = static_cast<size_t>(this->currentSampleRate * 1.6) + 8;
    this->delayLine.assign(maxDelay, {});
    this->reverbLine.assign(static_cast<size_t>(this->currentSampleRate * 0.25) + 8, {});
    this->chorusLine.assign(static_cast<size_t>(this->currentSampleRate * 0.06) + 8, {});
    this->flangerLine.assign(static_cast<size_t>(this->currentSampleRate * 0.04) + 8, {});
    this->delayPosition = this->reverbPosition = this->chorusPosition = this->flangerPosition = 0;
    this->highPassStates = {};
    this->lowPassStates = {};
    this->eqStates = {};
    this->envelope[0] = this->envelope[1] = 0.0f;
    this->gateEnvelope[0] = this->gateEnvelope[1] = 0.0f;
    this->phaserState[0][0] = this->phaserState[0][1] = this->phaserState[0][2] = this->phaserState[0][3] = 0.0f;
    this->phaserState[1][0] = this->phaserState[1][1] = this->phaserState[1][2] = this->phaserState[1][3] = 0.0f;
    this->wahLowState[0] = this->wahLowState[1] = 0.0f;
    this->chorusPhase = this->flangerPhase = this->phaserPhase = this->vibratoPhase = this->tremoloPhase = 0.0f;
    this->highPassCoefficients = makePass(sr, this->highPass->get(), true);
    this->lowPassCoefficients = makePass(sr, this->lowPass->get(), false);
}

void BuiltInEffectsAudioPlugin::releaseResources()
{
    this->delayLine.clear();
    this->reverbLine.clear();
    this->chorusLine.clear();
    this->flangerLine.clear();
}

void BuiltInEffectsAudioPlugin::processBlock(AudioBuffer<float> &buffer, MidiBuffer &midiMessages)
{
    ignoreUnused(midiMessages);
    ScopedNoDenormals noDenormals;
    const int channels = jmin(2, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();
    if (channels == 0 || this->delayLine.empty() || this->reverbLine.empty())
        return;

    const float sr = static_cast<float>(this->currentSampleRate);
    this->highPassCoefficients = makePass(sr, this->highPass->get(), true);
    this->lowPassCoefficients = makePass(sr, this->lowPass->get(), false);
    this->eqCoefficients[0] = makeShelf(sr, this->eqLowFreq->get(), this->eqLowGain->get(), false);
    for (int b = 0; b < 5; ++b)
        this->eqCoefficients[static_cast<size_t>(b + 1)] = makePeak(sr,
            this->eqFreq[static_cast<size_t>(b)]->get(),
            this->eqGain[static_cast<size_t>(b)]->get(),
            this->eqQ[static_cast<size_t>(b)]->get());
    this->eqCoefficients[6] = makeShelf(sr, this->eqHighFreq->get(), this->eqHighGain->get(), true);

    const float thresholdDb = this->threshold->get();
    const float ratio = jmax(1.0f, this->ratio->get());
    const float kneeDb = this->compressorKnee->get();
    const float attack = std::exp(-1.0f / (0.001f * this->compressorAttack->get() * sr));
    const float release = std::exp(-1.0f / (0.001f * this->compressorRelease->get() * sr));
    const float makeup = Decibels::decibelsToGain(this->compressorMakeup->get());
    const float gateThreshold = Decibels::decibelsToGain(this->gateThreshold->get());
    const float limiter = Decibels::decibelsToGain(this->limiterThreshold->get());
    const float delayMix = this->delayMix->get(), delayFeedback = this->delayFeedback->get();
    const int delaySamples = jlimit(1, static_cast<int>(this->delayLine.size()) - 2,
        roundToInt(this->delayTime->get() * 0.001f * sr));
    const float reverbMix = this->reverbMix->get(), room = this->reverbRoom->get();
    const float damping = this->reverbDamping->get();
    const int preDelay = roundToInt(this->reverbPreDelay->get() * 0.001f * sr);
    const float chorusMix = this->chorusMix->get();
    const float flangerMix = this->flangerMix->get(), flangerDepth = this->flangerDepth->get();
    const float phaserMix = this->phaserMix->get(), phaserDepth = this->phaserDepth->get();
    const float vibratoMix = this->vibratoMix->get();
    const float wahMix = this->autoWahMix->get();
    const float drive = Decibels::decibelsToGain(this->drive->get());
    const float outGain = Decibels::decibelsToGain(this->outputGain->get());
    const float bitLevels = std::pow(2.0f, jmax(4.0f, this->bitDepth->get()) - 1.0f);
    const float legacyBass = Decibels::decibelsToGain(this->bass->get());
    const float legacyMid = Decibels::decibelsToGain(this->mid->get());
    const float legacyTreble = Decibels::decibelsToGain(this->treble->get());

    for (int i = 0; i < samples; ++i)
    {
        const float chorusLfo = std::sin(this->chorusPhase);
        const float flangerLfo = std::sin(this->flangerPhase);
        const float phaserLfo = 0.5f + 0.5f * std::sin(this->phaserPhase);
        const float vibratoLfo = 0.5f + 0.5f * std::sin(this->vibratoPhase);
        const float tremoloLfo = 0.5f + 0.5f * std::sin(this->tremoloPhase);

        for (int ch = 0; ch < channels; ++ch)
        {
            float x = buffer.getSample(ch, i);

            x = runBiquad(x, this->highPassCoefficients, this->highPassStates[static_cast<size_t>(ch)]);
            x = runBiquad(x, this->lowPassCoefficients, this->lowPassStates[static_cast<size_t>(ch)]);
            for (int b = 0; b < 7; ++b)
                x = runBiquad(x, this->eqCoefficients[static_cast<size_t>(b)], this->eqStates[static_cast<size_t>(b)][static_cast<size_t>(ch)]);

            // Retain the original three-band tone controls for older project states.
            if (this->bass->get() != 0.0f || this->mid->get() != 0.0f || this->treble->get() != 0.0f)
                x *= (legacyBass * 0.25f + legacyMid * 0.5f + legacyTreble * 0.25f);

            const float magnitude = std::abs(x);
            const float detectorCoeff = magnitude > this->envelope[ch] ? attack : release;
            this->envelope[ch] = detectorCoeff * this->envelope[ch] + (1.0f - detectorCoeff) * magnitude;
            const float levelDb = Decibels::gainToDecibels(jmax(1.0e-7f, this->envelope[ch]));
            float reductionDb = 0.0f;
            const float over = levelDb - thresholdDb;
            if (kneeDb > 0.0f && over > -0.5f * kneeDb && over < 0.5f * kneeDb)
            {
                const float soft = over + 0.5f * kneeDb;
                reductionDb = (1.0f / ratio - 1.0f) * soft * soft / (2.0f * kneeDb);
            }
            else if (over >= 0.5f * kneeDb)
                reductionDb = (thresholdDb + over / ratio) - levelDb;
            const float compGain = Decibels::decibelsToGain(reductionDb);
            x *= compGain * makeup;

            const float gateCoeff = magnitude > this->gateEnvelope[ch] ? attack : release;
            this->gateEnvelope[ch] = gateCoeff * this->gateEnvelope[ch] + (1.0f - gateCoeff) * magnitude;
            if (this->gateThreshold->get() > -89.9f && this->gateEnvelope[ch] < gateThreshold)
                x *= 0.12f;

            // Stereo feedback delay, with optional cross-channel ping-pong feedback.
            const size_t dRead = (this->delayPosition + this->delayLine.size() - static_cast<size_t>(delaySamples)) % this->delayLine.size();
            const int other = 1 - ch;
            const float delayed = this->delayLine[dRead][static_cast<size_t>(ch)];
            const float cross = this->delayLine[dRead][static_cast<size_t>(other)];
            const float ping = this->delayPingPong->get();
            this->delayLine[this->delayPosition][static_cast<size_t>(ch)] =
                x + delayFeedback * (delayed * (1.0f - ping) + cross * ping);
            x = x * (1.0f - delayMix) + delayed * delayMix;

            // Damped, room-size-scaled stereo feedback reverb line.
            const int reverbSamples = jlimit(1, static_cast<int>(this->reverbLine.size()) - 2,
                roundToInt((0.055f + room * 0.13f) * sr) + preDelay);
            const size_t rRead = (this->reverbPosition + this->reverbLine.size() - static_cast<size_t>(reverbSamples)) % this->reverbLine.size();
            const float rv = this->reverbLine[rRead][static_cast<size_t>(ch)];
            const float rvCross = this->reverbLine[rRead][static_cast<size_t>(other)];
            const float damped = rv * (1.0f - damping) + this->wahLowState[ch] * damping;
            this->wahLowState[ch] = damped;
            this->reverbLine[this->reverbPosition][static_cast<size_t>(ch)] =
                x + (damped * (0.48f + room * 0.42f) + rvCross * 0.07f);
            x = x * (1.0f - reverbMix) + rv * reverbMix;

            // Chorus: modulated delay with dry/wet control.
            const int chorusDelay = jlimit(1, static_cast<int>(this->chorusLine.size()) - 2,
                roundToInt((0.018f + 0.009f * (1.0f + chorusLfo)) * sr));
            const size_t cRead = (this->chorusPosition + this->chorusLine.size() - static_cast<size_t>(chorusDelay)) % this->chorusLine.size();
            const float chorusWet = this->chorusLine[cRead][static_cast<size_t>(ch)];
            this->chorusLine[this->chorusPosition][static_cast<size_t>(ch)] = x;
            x = x * (1.0f - chorusMix) + (x * 0.5f + chorusWet * 0.5f) * chorusMix;

            // Flanger: short modulated delay with feedback and a true wet/dry mix.
            const int flangeDelay = jlimit(1, static_cast<int>(this->flangerLine.size()) - 2,
                roundToInt((0.001f + flangerDepth * (0.006f + 0.004f * (1.0f + flangerLfo))) * sr));
            const size_t fRead = (this->flangerPosition + this->flangerLine.size() - static_cast<size_t>(flangeDelay)) % this->flangerLine.size();
            const float flangeWet = this->flangerLine[fRead][static_cast<size_t>(ch)];
            this->flangerLine[this->flangerPosition][static_cast<size_t>(ch)] = x + flangeWet * this->flangerFeedback->get();
            x = x * (1.0f - flangerMix) + (x + flangeWet) * 0.5f * flangerMix;

            // Four cascaded all-pass stages produce the phaser's moving notches.
            const float ap = jlimit(0.05f, 0.85f, 0.12f + phaserDepth * (0.35f + phaserLfo * 0.35f));
            float phaseSignal = x;
            for (int stage = 0; stage < 4; ++stage)
            {
                const float y = -ap * phaseSignal + this->phaserState[ch][stage];
                this->phaserState[ch][stage] = phaseSignal + ap * y;
                phaseSignal = y;
            }
            x += (phaseSignal - x) * phaserMix;

            // Vibrato is a pitch-modulating delay (wet-only blend).
            const int vibratoDelay = jlimit(1, static_cast<int>(this->chorusLine.size()) - 2,
                roundToInt((0.002f + this->vibratoDepth->get() * 0.001f * vibratoLfo) * sr));
            const size_t vRead = (this->chorusPosition + this->chorusLine.size() - static_cast<size_t>(vibratoDelay)) % this->chorusLine.size();
            const float vibratoWet = this->chorusLine[vRead][static_cast<size_t>(ch)];
            x = x * (1.0f - vibratoMix) + vibratoWet * vibratoMix;

            // Envelope-following resonant low-pass approximates an auto-wah sweep.
            const float wahEnvelope = jlimit(0.0f, 1.0f, this->envelope[ch] * this->autoWahSensitivity->get() * 8.0f);
            const float wahCutoff = 350.0f + wahEnvelope * 3600.0f;
            const float wahCoeff = 1.0f - std::exp(-twoPi * wahCutoff / sr);
            this->wahLowState[ch] += wahCoeff * (x - this->wahLowState[ch]);
            const float wahBand = x - this->wahLowState[ch];
            x = x * (1.0f - wahMix) + (x + wahBand * 2.2f) * wahMix;

            if (drive > 1.001f)
                x = std::tanh(x * drive) / std::tanh(drive);
            if (this->bitDepth->get() < 15.99f)
                x = std::round(x * bitLevels) / bitLevels;
            x *= 1.0f - this->tremoloDepth->get() * tremoloLfo;
            x = jlimit(-limiter, limiter, x);
            buffer.setSample(ch, i, x * outGain);
        }

        this->delayPosition = (this->delayPosition + 1) % this->delayLine.size();
        this->reverbPosition = (this->reverbPosition + 1) % this->reverbLine.size();
        this->chorusPosition = (this->chorusPosition + 1) % this->chorusLine.size();
        this->flangerPosition = (this->flangerPosition + 1) % this->flangerLine.size();
        this->chorusPhase += twoPi * 0.32f / sr;
        this->flangerPhase += twoPi * this->flangerRate->get() / sr;
        this->phaserPhase += twoPi * this->phaserRate->get() / sr;
        this->vibratoPhase += twoPi * this->vibratoRate->get() / sr;
        this->tremoloPhase += twoPi * this->tremoloRate->get() / sr;
        if (this->chorusPhase > twoPi) this->chorusPhase -= twoPi;
        if (this->flangerPhase > twoPi) this->flangerPhase -= twoPi;
        if (this->phaserPhase > twoPi) this->phaserPhase -= twoPi;
        if (this->vibratoPhase > twoPi) this->vibratoPhase -= twoPi;
        if (this->tremoloPhase > twoPi) this->tremoloPhase -= twoPi;

        if (channels == 2)
        {
            const float left = buffer.getSample(0, i), right = buffer.getSample(1, i);
            const float mid = 0.5f * (left + right);
            const float side = 0.5f * (left - right) * this->stereoWidth->get();
            const float p = this->pan->get();
            buffer.setSample(0, i, (mid + side) * (p > 0.0f ? 1.0f - p : 1.0f));
            buffer.setSample(1, i, (mid - side) * (p < 0.0f ? 1.0f + p : 1.0f));
        }
    }
}

double BuiltInEffectsAudioPlugin::getTailLengthSeconds() const { return 4.0; }
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
