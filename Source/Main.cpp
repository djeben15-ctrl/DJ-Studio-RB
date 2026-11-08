#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <atomic>
#include <limits>

class VirtualJogWheel : public juce::Component
{
public:
    std::function<void()> onScratchStart;
    std::function<void(double, double)> onScratchMove;
    std::function<void(double)> onScratchEnd;

    void setPosition(double newPosition)
    {
        position = juce::jlimit(0.0, 1.0, newPosition);
        if (!dragging)
            repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(5.0f);
        const auto centre = area.getCentre();
        const float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;

        g.setColour(juce::Colour(9, 12, 18));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(juce::Colour(70, 83, 98));
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 3.0f);

        for (int ring = 1; ring <= 4; ++ring)
        {
            const float r = radius * (0.28f + 0.13f * ring);
            g.setColour(juce::Colour(35 + ring * 5, 48 + ring * 5, 61 + ring * 5));
            g.drawEllipse(centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, 1.2f);
        }

        g.setColour(juce::Colour(35, 210, 205));
        const float markerAngle = static_cast<float>(position * juce::MathConstants<double>::twoPi);
        const juce::Point<float> marker(centre.x + std::sin(markerAngle) * radius * 0.82f,
                                        centre.y - std::cos(markerAngle) * radius * 0.82f);
        g.drawLine(centre.x, centre.y, marker.x, marker.y, 3.0f);
        g.fillEllipse(marker.x - 4.0f, marker.y - 4.0f, 8.0f, 8.0f);

        const float hub = radius * 0.27f;
        g.setColour(juce::Colour(37, 49, 62));
        g.fillEllipse(centre.x - hub, centre.y - hub, hub * 2.0f, hub * 2.0f);
        g.setColour(juce::Colour(190, 205, 220));
        g.drawEllipse(centre.x - hub, centre.y - hub, hub * 2.0f, hub * 2.0f, 1.5f);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawFittedText("DJ", getLocalBounds(), juce::Justification::centred, 1);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        dragging = true;
        startAngle = angleAt(event.position);
        startPosition = position;
        lastDragTime = juce::Time::getMillisecondCounterHiRes();
        if (onScratchStart)
            onScratchStart();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!dragging)
            return;

        double delta = angleAt(event.position) - startAngle;
        if (delta > juce::MathConstants<double>::pi)
            delta -= juce::MathConstants<double>::twoPi;
        else if (delta < -juce::MathConstants<double>::pi)
            delta += juce::MathConstants<double>::twoPi;

        const double previousPosition = position;
        position = juce::jlimit(0.0, 1.0,
                                startPosition + delta / juce::MathConstants<double>::twoPi * 0.30);
        const double now = juce::Time::getMillisecondCounterHiRes();
        const double elapsed = juce::jmax(0.001, (now - lastDragTime) / 1000.0);
        const double speed = (position - previousPosition) / elapsed;
        lastDragTime = now;
        if (onScratchMove)
            onScratchMove(position, speed);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        dragging = false;
        repaint();
        if (onScratchEnd)
            onScratchEnd(position);
    }

private:
    double position = 0.0;
    double startPosition = 0.0;
    double startAngle = 0.0;
    bool dragging = false;
    double lastDragTime = 0.0;

    double angleAt(juce::Point<float> point) const
    {
        const auto centre = getLocalBounds().toFloat().getCentre();
        return std::atan2(static_cast<double>(point.y - centre.y),
                          static_cast<double>(point.x - centre.x));
    }
};

class DJStudioRB : public juce::AudioAppComponent, private juce::Timer
{
public:
    DJStudioRB()
    {
        formatManager.registerBasicFormats();

        setSize(1100, 700);
        setAudioChannels(0, 2);

        title.setText("DJ STUDIO RB", juce::dontSendNotification);
        title.setFont(juce::Font(30.0f, juce::Font::bold));
        title.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(title);

        setupDeck(deckA, "DECK A", loadA, playA, stopA, trackA);
        setupDeck(deckB, "DECK B", loadB, playB, stopB, trackB);

        loadA.onClick = [this] { loadTrack(0); };
        loadB.onClick = [this] { loadTrack(1); };
        playA.onClick = [this] { startDeck(0); };
        playB.onClick = [this] { startDeck(1); };
        stopA.onClick = [this] { stopDeck(0); };
        stopB.onClick = [this] { stopDeck(1); };

        crossfader.setRange(0.0, 1.0, 0.001);
        crossfader.setValue(0.5);
        crossfader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        crossfader.setColour(juce::Slider::thumbColourId, juce::Colour(35, 210, 205));
        addAndMakeVisible(crossfader);

        mixerLabel.setText("CROSSFADER  A  ◀──────●──────▶  B", juce::dontSendNotification);
        mixerLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(mixerLabel);

        status.setText("Listo. Carga una canción en cada deck.", juce::dontSendNotification);
        status.setJustificationType(juce::Justification::centred);
        status.setColour(juce::Label::textColourId, juce::Colour(180, 190, 205));
        addAndMakeVisible(status);

        setupDeckControls();
        startTimerHz(60);
    }

    ~DJStudioRB() override
    {
        shutdownAudio();
        transportA.setSource(nullptr);
        transportB.setSource(nullptr);
    }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        transportA.prepareToPlay(samplesPerBlockExpected, sampleRate);
        transportB.prepareToPlay(samplesPerBlockExpected, sampleRate);
        outputSampleRate = sampleRate;
        lowAlpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 250.0f / static_cast<float>(sampleRate));
        midAlpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 4000.0f / static_cast<float>(sampleRate));
        scratchA.setSize(2, samplesPerBlockExpected);
        scratchB.setSize(2, samplesPerBlockExpected);
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        if (info.buffer == nullptr)
            return;

        const int channels = juce::jmin(2, info.buffer->getNumChannels());
        if (info.numSamples > scratchA.getNumSamples())
        {
            scratchA.setSize(2, info.numSamples, false, false, true);
            scratchB.setSize(2, info.numSamples, false, false, true);
        }

        scratchA.clear(0, info.numSamples);
        scratchB.clear(0, info.numSamples);

        juce::AudioSourceChannelInfo blockA(&scratchA, 0, info.numSamples);
        juce::AudioSourceChannelInfo blockB(&scratchB, 0, info.numSamples);
        transportA.getNextAudioBlock(blockA);
        transportB.getNextAudioBlock(blockB);

        // During a scratch gesture, render directly from the decoded track buffer.
        // This supports forward and reverse motion without repeatedly seeking the file decoder.
        renderScratchDeck(scratchA, 0, info.numSamples);
        renderScratchDeck(scratchB, 1, info.numSamples);

        const float blend = static_cast<float>(crossfader.getValue());
        applyThreeBandEQ(scratchA, info.numSamples,
                         static_cast<float>(eqLowA.getValue()),
                         static_cast<float>(eqMidA.getValue()),
                         static_cast<float>(eqHighA.getValue()), lowStateA, midStateA);
        applyThreeBandEQ(scratchB, info.numSamples,
                         static_cast<float>(eqLowB.getValue()),
                         static_cast<float>(eqMidB.getValue()),
                         static_cast<float>(eqHighB.getValue()), lowStateB, midStateB);

        const float gainA = (1.0f - blend) * static_cast<float>(volumeA.getValue());
        const float gainB = blend * static_cast<float>(volumeB.getValue());

        // A jog seek is performed only after its deck fades to silence.
        // The message-thread timer then changes the transport position and requests a fade-in.
        if (jogResumeRequestA.exchange(false))
        {
            jogFadingOutA = false;
            jogFadingInA = true;
            jogFadeRemainingA = jogFadeLength;
            jogGainA = 0.0f;
        }
        if (jogResumeRequestB.exchange(false))
        {
            jogFadingOutB = false;
            jogFadingInB = true;
            jogFadeRemainingB = jogFadeLength;
            jogGainB = 0.0f;
        }

        if (jogSeekRequestedA.exchange(false))
        {
            if (jogGainA > 0.0001f && !jogFadingOutA)
            {
                jogFadingInA = false;
                jogFadingOutA = true;
                jogFadeRemainingA = jogFadeLength;
            }
            else if (jogGainA <= 0.0001f)
                jogSeekReadyA.store(true);
        }
        if (jogSeekRequestedB.exchange(false))
        {
            if (jogGainB > 0.0001f && !jogFadingOutB)
            {
                jogFadingInB = false;
                jogFadingOutB = true;
                jogFadeRemainingB = jogFadeLength;
            }
            else if (jogGainB <= 0.0001f)
                jogSeekReadyB.store(true);
        }

        const float fadeStep = 1.0f / static_cast<float>(jogFadeLength);
        for (int sample = 0; sample < info.numSamples; ++sample)
        {
            for (int channel = 0; channel < channels; ++channel)
            {
                const float sampleA = scratchA.getSample(channel, sample) * gainA * jogGainA;
                const float sampleB = scratchB.getSample(channel, sample) * gainB * jogGainB;
                info.buffer->setSample(channel, info.startSample + sample, sampleA + sampleB);
            }

            if (jogFadingOutA && jogFadeRemainingA > 0)
            {
                jogGainA = juce::jmax(0.0f, jogGainA - fadeStep);
                if (--jogFadeRemainingA <= 0 || jogGainA <= 0.0001f)
                {
                    jogGainA = 0.0f;
                    jogFadingOutA = false;
                    jogSeekReadyA.store(true);
                }
            }
            else if (jogFadingInA && jogFadeRemainingA > 0)
            {
                jogGainA = juce::jmin(1.0f, jogGainA + fadeStep);
                if (--jogFadeRemainingA <= 0 || jogGainA >= 0.9999f)
                {
                    jogGainA = 1.0f;
                    jogFadingInA = false;
                }
            }

            if (jogFadingOutB && jogFadeRemainingB > 0)
            {
                jogGainB = juce::jmax(0.0f, jogGainB - fadeStep);
                if (--jogFadeRemainingB <= 0 || jogGainB <= 0.0001f)
                {
                    jogGainB = 0.0f;
                    jogFadingOutB = false;
                    jogSeekReadyB.store(true);
                }
            }
            else if (jogFadingInB && jogFadeRemainingB > 0)
            {
                jogGainB = juce::jmin(1.0f, jogGainB + fadeStep);
                if (--jogFadeRemainingB <= 0 || jogGainB >= 0.9999f)
                {
                    jogGainB = 1.0f;
                    jogFadingInB = false;
                }
            }
        }
    }

    void releaseResources() override
    {
        transportA.releaseResources();
        transportB.releaseResources();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(18, 20, 27));
        g.setColour(juce::Colour(35, 39, 49));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(15), 15.0f);
    }

    void resized() override
    {
        title.setBounds(0, 18, getWidth(), 55);
        deckA.setBounds(35, 100, 480, 470);
        deckB.setBounds(585, 100, 480, 470);

        loadA.setBounds(65, 155, 120, 42);
        playA.setBounds(195, 155, 100, 42);
        stopA.setBounds(305, 155, 100, 42);
        trackA.setBounds(65, 210, 410, 48);
        timeA.setBounds(65, 265, 410, 24);
        seekA.setBounds(65, 290, 410, 24);
        cueA.setBounds(65, 330, 90, 34);
        volumeLabelA.setBounds(170, 330, 90, 34);
        volumeA.setBounds(260, 330, 215, 34);
        eqTitleA.setBounds(65, 372, 410, 20);
        eqLowA.setBounds(65, 398, 125, 28);
        eqMidA.setBounds(205, 398, 125, 28);
        eqHighA.setBounds(345, 398, 130, 28);
        jogWheelA.setBounds(220, 432, 110, 110);

        loadB.setBounds(615, 155, 120, 42);
        playB.setBounds(745, 155, 100, 42);
        stopB.setBounds(855, 155, 100, 42);
        trackB.setBounds(615, 210, 410, 48);
        timeB.setBounds(615, 265, 410, 24);
        seekB.setBounds(615, 290, 410, 24);
        cueB.setBounds(615, 330, 90, 34);
        volumeLabelB.setBounds(720, 330, 90, 34);
        volumeB.setBounds(810, 330, 215, 34);
        eqTitleB.setBounds(615, 372, 410, 20);
        eqLowB.setBounds(615, 398, 125, 28);
        eqMidB.setBounds(755, 398, 125, 28);
        eqHighB.setBounds(895, 398, 130, 28);
        jogWheelB.setBounds(770, 432, 110, 110);

        mixerLabel.setBounds(350, 575, 400, 25);
        crossfader.setBounds(300, 600, 500, 40);
        status.setBounds(100, 650, 900, 25);
    }

private:
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportA, transportB;
    std::unique_ptr<juce::AudioFormatReaderSource> readerA, readerB;
    juce::AudioBuffer<float> scratchA, scratchB;
    std::shared_ptr<juce::AudioBuffer<float>> trackAudioA, trackAudioB;
    std::atomic<bool> scratchingA{ false }, scratchingB{ false };
    std::atomic<double> scratchPositionA{ 0.0 }, scratchPositionB{ 0.0 };
    std::atomic<double> scratchSpeedA{ 0.0 }, scratchSpeedB{ 0.0 };
    double outputSampleRate = 44100.0;
    double sourceSampleRateA = 44100.0, sourceSampleRateB = 44100.0;
    static constexpr int jogFadeLength = 3840; // about 80 ms at 48 kHz
    std::atomic<double> jogPendingPositionA{ 0.0 }, jogPendingPositionB{ 0.0 };
    std::atomic<bool> jogSeekRequestedA{ false }, jogSeekRequestedB{ false };
    std::atomic<bool> jogSeekReadyA{ false }, jogSeekReadyB{ false };
    std::atomic<bool> jogResumeRequestA{ false }, jogResumeRequestB{ false };
    std::atomic<bool> jogResumePlaybackA{ false }, jogResumePlaybackB{ false };
    // These values are owned by the audio callback thread.
    float jogGainA = 1.0f, jogGainB = 1.0f;
    int jogFadeRemainingA = 0, jogFadeRemainingB = 0;
    bool jogFadingOutA = false, jogFadingOutB = false;
    bool jogFadingInA = false, jogFadingInB = false;
    std::unique_ptr<juce::FileChooser> chooserA, chooserB;

    juce::Label title, mixerLabel, status;
    juce::GroupComponent deckA, deckB;
    juce::TextButton loadA{"CARGAR"};
    juce::TextButton playA{"PLAY"};
    juce::TextButton stopA{"STOP"};
    juce::TextButton loadB{"CARGAR"};
    juce::TextButton playB{"PLAY"};
    juce::TextButton stopB{"STOP"};
    juce::Label trackA, trackB;
    juce::Slider crossfader, seekA, seekB, volumeA, volumeB;
    juce::TextButton cueA{"CUE / INICIO"};
    juce::TextButton cueB{"CUE / INICIO"};
    juce::Label timeA, timeB, volumeLabelA, volumeLabelB, eqTitleA, eqTitleB;
    juce::Slider eqLowA, eqMidA, eqHighA, eqLowB, eqMidB, eqHighB;
    VirtualJogWheel jogWheelA, jogWheelB;
    float lowAlpha = 0.03f, midAlpha = 0.3f;
    float lowStateA[2] = { 0.0f, 0.0f }, midStateA[2] = { 0.0f, 0.0f };
    float lowStateB[2] = { 0.0f, 0.0f }, midStateB[2] = { 0.0f, 0.0f };

    void renderScratchDeck(juce::AudioBuffer<float>& destination, int deckNumber, int numSamples)
    {
        const bool active = (deckNumber == 0 ? scratchingA.load() : scratchingB.load());
        if (!active)
            return;

        auto audio = (deckNumber == 0 ? std::atomic_load(&trackAudioA)
                                      : std::atomic_load(&trackAudioB));
        if (!audio || audio->getNumSamples() < 2)
            return;

        const double sampleRate = deckNumber == 0 ? sourceSampleRateA : sourceSampleRateB;
        double positionSeconds = deckNumber == 0 ? scratchPositionA.load() : scratchPositionB.load();
        const double speed = deckNumber == 0 ? scratchSpeedA.load() : scratchSpeedB.load();
        const int sourceChannels = audio->getNumChannels();
        const int destinationChannels = destination.getNumChannels();
        const int totalSamples = audio->getNumSamples();
        const double increment = speed * sampleRate / juce::jmax(1.0, outputSampleRate);

        for (int i = 0; i < numSamples; ++i)
        {
            const double samplePosition = juce::jlimit(0.0, static_cast<double>(totalSamples - 2),
                                                       positionSeconds * sampleRate);
            const int index = static_cast<int>(samplePosition);
            const float fraction = static_cast<float>(samplePosition - index);

            for (int channel = 0; channel < destinationChannels; ++channel)
            {
                const int sourceChannel = juce::jmin(channel, sourceChannels - 1);
                const float s0 = audio->getSample(sourceChannel, index);
                const float s1 = audio->getSample(sourceChannel, index + 1);
                destination.setSample(channel, i, s0 + (s1 - s0) * fraction);
            }

            positionSeconds += increment / sampleRate;
            positionSeconds = juce::jlimit(0.0, static_cast<double>(totalSamples - 2) / sampleRate,
                                           positionSeconds);
        }

        if (deckNumber == 0)
            scratchPositionA.store(positionSeconds);
        else
            scratchPositionB.store(positionSeconds);
    }

    void applyThreeBandEQ(juce::AudioBuffer<float>& buffer, int numSamples,
                          float lowGain, float midGain, float highGain,
                          float* lowState, float* midState)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* samples = buffer.getWritePointer(channel);
            float low = lowState[channel];
            float midLowPass = midState[channel];

            for (int i = 0; i < numSamples; ++i)
            {
                const float input = samples[i];
                low += lowAlpha * (input - low);
                midLowPass += midAlpha * (input - midLowPass);
                const float lowBand = low;
                const float midBand = midLowPass - low;
                const float highBand = input - midLowPass;
                samples[i] = lowBand * lowGain + midBand * midGain + highBand * highGain;
            }

            lowState[channel] = low;
            midState[channel] = midLowPass;
        }
    }

    void setupDeckControls()
    {
        auto setupSeek = [this](juce::Slider& slider)
        {
            slider.setRange(0.0, 1.0, 0.001);
            slider.setValue(0.0);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setColour(juce::Slider::trackColourId, juce::Colour(35, 210, 205));
            addAndMakeVisible(slider);
        };
        setupSeek(seekA);
        setupSeek(seekB);

        auto setupVolume = [this](juce::Slider& slider)
        {
            slider.setRange(0.0, 1.0, 0.01);
            slider.setValue(1.0);
            slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 48, 24);
            addAndMakeVisible(slider);
        };
        setupVolume(volumeA);
        setupVolume(volumeB);

        auto setupEq = [this](juce::Slider& slider, juce::Label& label, const juce::String& text)
        {
            label.setText(text, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setColour(juce::Label::textColourId, juce::Colour(190, 200, 215));
            addAndMakeVisible(label);
            slider.setRange(0.0, 2.0, 0.01);
            slider.setValue(1.0);
            slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            slider.setColour(juce::Slider::trackColourId, juce::Colour(35, 210, 205));
            addAndMakeVisible(slider);
        };
        setupEq(eqLowA, eqTitleA, "ECUALIZADOR  LOW / MID / HIGH");
        setupEq(eqMidA, eqTitleA, "ECUALIZADOR  LOW / MID / HIGH");
        setupEq(eqHighA, eqTitleA, "ECUALIZADOR  LOW / MID / HIGH");
        setupEq(eqLowB, eqTitleB, "ECUALIZADOR  LOW / MID / HIGH");
        setupEq(eqMidB, eqTitleB, "ECUALIZADOR  LOW / MID / HIGH");
        setupEq(eqHighB, eqTitleB, "ECUALIZADOR  LOW / MID / HIGH");

        cueA.onClick = [this] { cueDeck(0); };
        cueB.onClick = [this] { cueDeck(1); };

        jogWheelA.onScratchStart = [this]
        {
            const double length = transportA.getLengthInSeconds();
            if (length > 0.0)
            {
                jogResumePlaybackA.store(transportA.isPlaying());
                scratchPositionA.store(transportA.getCurrentPosition());
                scratchSpeedA.store(0.0);
                scratchingA.store(true);
            }
        };
        jogWheelA.onScratchMove = [this](double p, double speed)
        {
            const double length = transportA.getLengthInSeconds();
            if (length > 0.0)
            {
                scratchPositionA.store(p * length);
                scratchSpeedA.store(juce::jlimit(-4.0, 4.0, speed * length));
            }
        };
        jogWheelA.onScratchEnd = [this](double p)
        {
            const double length = transportA.getLengthInSeconds();
            scratchingA.store(false);
            scratchSpeedA.store(0.0);
            if (length > 0.0)
            {
                jogPendingPositionA.store(p * length);
                jogSeekRequestedA.store(true);
            }
        };

        jogWheelB.onScratchStart = [this]
        {
            const double length = transportB.getLengthInSeconds();
            if (length > 0.0)
            {
                jogResumePlaybackB.store(transportB.isPlaying());
                scratchPositionB.store(transportB.getCurrentPosition());
                scratchSpeedB.store(0.0);
                scratchingB.store(true);
            }
        };
        jogWheelB.onScratchMove = [this](double p, double speed)
        {
            const double length = transportB.getLengthInSeconds();
            if (length > 0.0)
            {
                scratchPositionB.store(p * length);
                scratchSpeedB.store(juce::jlimit(-4.0, 4.0, speed * length));
            }
        };
        jogWheelB.onScratchEnd = [this](double p)
        {
            const double length = transportB.getLengthInSeconds();
            scratchingB.store(false);
            scratchSpeedB.store(0.0);
            if (length > 0.0)
            {
                jogPendingPositionB.store(p * length);
                jogSeekRequestedB.store(true);
            }
        };
        addAndMakeVisible(jogWheelA);
        addAndMakeVisible(jogWheelB);
        for (auto* button : { &cueA, &cueB })
        {
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(45, 55, 70));
            button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            addAndMakeVisible(button);
        }

        timeA.setJustificationType(juce::Justification::centredLeft);
        timeB.setJustificationType(juce::Justification::centredLeft);
        volumeLabelA.setText("VOLUME", juce::dontSendNotification);
        volumeLabelB.setText("VOLUME", juce::dontSendNotification);
        for (auto* label : { &timeA, &timeB, &volumeLabelA, &volumeLabelB })
        {
            label->setColour(juce::Label::textColourId, juce::Colour(190, 200, 215));
            addAndMakeVisible(label);
        }

        seekA.onValueChange = [this] { seekDeckFromSlider(0); };
        seekB.onValueChange = [this] { seekDeckFromSlider(1); };
    }

    static juce::String formatTime(double seconds)
    {
        const int total = juce::jmax(0, (int) seconds);
        return juce::String(total / 60).paddedLeft('0', 2) + ":" +
               juce::String(total % 60).paddedLeft('0', 2);
    }

    void timerCallback() override
    {
        // Apply each seek only after the audio thread has faded that deck out.
        if (jogSeekReadyA.exchange(false))
        {
            // Stop the decoder before changing its read position. This avoids
            // racing the audio callback while the reader jumps to another frame.
            transportA.stop();
            transportA.setPosition(jogPendingPositionA.load());
            lowStateA[0] = lowStateA[1] = 0.0f;
            midStateA[0] = midStateA[1] = 0.0f;
            if (jogResumePlaybackA.exchange(false))
                transportA.start();
            jogResumeRequestA.store(true);
        }
        if (jogSeekReadyB.exchange(false))
        {
            transportB.stop();
            transportB.setPosition(jogPendingPositionB.load());
            lowStateB[0] = lowStateB[1] = 0.0f;
            midStateB[0] = midStateB[1] = 0.0f;
            if (jogResumePlaybackB.exchange(false))
                transportB.start();
            jogResumeRequestB.store(true);
        }

        updateDeckDisplay(0);
        updateDeckDisplay(1);
    }

    void updateDeckDisplay(int deckNumber)
    {
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        auto& slider = (deckNumber == 0 ? seekA : seekB);
        auto& label = (deckNumber == 0 ? timeA : timeB);
        const double length = transport.getLengthInSeconds();
        const double position = transport.getCurrentPosition();
        auto& jogWheel = (deckNumber == 0 ? jogWheelA : jogWheelB);
        jogWheel.setPosition(length > 0.0 ? position / length : 0.0);

        if (!slider.isMouseButtonDown())
            slider.setValue(length > 0.0 ? position / length : 0.0,
                            juce::dontSendNotification);

        label.setText(formatTime(position) + " / " + formatTime(length),
                      juce::dontSendNotification);
    }

    void seekDeckFromSlider(int deckNumber)
    {
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        const double length = transport.getLengthInSeconds();
        if (length > 0.0)
        {
            const auto& slider = (deckNumber == 0 ? seekA : seekB);
            transport.setPosition(slider.getValue() * length);
        }
    }

    void nudgeDeck(int deckNumber, double seconds)
    {
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        const double length = transport.getLengthInSeconds();
        if (length <= 0.0)
        {
            status.setText(deckNumber == 0 ? "DECK A: carga una pista antes de usar JOG."
                                           : "DECK B: carga una pista antes de usar JOG.",
                           juce::dontSendNotification);
            return;
        }

        const double nextPosition = juce::jlimit(0.0, length,
                                                  transport.getCurrentPosition() + seconds);
        transport.setPosition(nextPosition);
        status.setText(deckNumber == 0 ? "DECK A: ajuste JOG de 0.10 segundos."
                                       : "DECK B: ajuste JOG de 0.10 segundos.",
                       juce::dontSendNotification);
    }

    void cueDeck(int deckNumber)
    {
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        transport.stop();
        transport.setPosition(0.0);
        auto& playButton = (deckNumber == 0 ? playA : playB);
        playButton.setButtonText("PLAY");
        status.setText(deckNumber == 0 ? "DECK A: posición inicial." :
                                         "DECK B: posición inicial.",
                       juce::dontSendNotification);
    }

    void setupDeck(juce::GroupComponent& deck, const juce::String& name,
                   juce::TextButton& load, juce::TextButton& play,
                   juce::TextButton& stop, juce::Label& track)
    {
        deck.setText(name);
        addAndMakeVisible(deck);

        for (auto* button : { &load, &play, &stop })
        {
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(45, 55, 70));
            button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            addAndMakeVisible(button);
        }

        track.setText("Ninguna pista cargada", juce::dontSendNotification);
        track.setJustificationType(juce::Justification::centred);
        track.setColour(juce::Label::textColourId, juce::Colour(190, 200, 215));
        addAndMakeVisible(track);
    }

    void startDeck(int deckNumber)
    {
        auto* reader = (deckNumber == 0 ? readerA.get() : readerB.get());
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        auto& playButton = (deckNumber == 0 ? playA : playB);

        if (reader == nullptr)
        {
            status.setText(deckNumber == 0
                ? "DECK A: primero carga una canción."
                : "DECK B: primero carga una canción.", juce::dontSendNotification);
            return;
        }

        if (transport.getLengthInSeconds() > 0.0 &&
            transport.getCurrentPosition() >= transport.getLengthInSeconds())
            transport.setPosition(0.0);

        transport.start();
        playButton.setButtonText("PLAYING");
        status.setText(deckNumber == 0
            ? "DECK A: orden de reproducción enviada al motor de audio."
            : "DECK B: orden de reproducción enviada al motor de audio.",
            juce::dontSendNotification);
    }

    void stopDeck(int deckNumber)
    {
        auto& transport = (deckNumber == 0 ? transportA : transportB);
        auto& playButton = (deckNumber == 0 ? playA : playB);
        transport.stop();
        transport.setPosition(0.0);
        playButton.setButtonText("PLAY");
        status.setText(deckNumber == 0 ? "DECK A detenido." : "DECK B detenido.",
                       juce::dontSendNotification);
    }

    void loadTrack(int deckNumber)
    {
        auto& chooser = (deckNumber == 0 ? chooserA : chooserB);
        chooser = std::make_unique<juce::FileChooser>(
            "Selecciona una canción de audio",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.mp3;*.wav;*.aif;*.aiff;*.flac;*.ogg");

        chooser->launchAsync(
            juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
            [this, deckNumber](const juce::FileChooser& fileChooser)
            {
                const juce::File file = fileChooser.getResult();
                if (file.existsAsFile())
                    loadTrackFromFile(deckNumber, file);
            });
    }

    void loadTrackFromFile(int deckNumber, const juce::File& file)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

        if (reader == nullptr)
        {
            status.setText("No se pudo abrir ese formato de audio.", juce::dontSendNotification);
            return;
        }

        const double sourceSampleRate = reader->sampleRate;
        if (reader->lengthInSamples < 2 ||
            reader->lengthInSamples > static_cast<juce::int64>(std::numeric_limits<int>::max()))
        {
            status.setText("La pista es demasiado larga para el modo scratch en memoria.", juce::dontSendNotification);
            return;
        }

        const int decodedSamples = static_cast<int>(reader->lengthInSamples);
        auto decodedAudio = std::make_shared<juce::AudioBuffer<float>>(
            static_cast<int>(reader->numChannels), decodedSamples);
        if (!reader->read(decodedAudio.get(), 0, decodedSamples, 0, true, true))
        {
            status.setText("No se pudo preparar el audio para scratch.", juce::dontSendNotification);
            return;
        }

        std::unique_ptr<juce::AudioFormatReaderSource> newSource(
            new juce::AudioFormatReaderSource(reader.release(), true));

        if (deckNumber == 0)
        {
            transportA.stop();
            std::atomic_store(&trackAudioA, decodedAudio);
            sourceSampleRateA = sourceSampleRate;
            scratchingA.store(false);
            transportA.setSource(nullptr);
            readerA.reset();
            readerA = std::move(newSource);
            transportA.setSource(readerA.get(), 0, nullptr, sourceSampleRate);
            trackA.setText(file.getFileName(), juce::dontSendNotification);
            playA.setButtonText("PLAY");
        }
        else
        {
            transportB.stop();
            std::atomic_store(&trackAudioB, decodedAudio);
            sourceSampleRateB = sourceSampleRate;
            scratchingB.store(false);
            transportB.setSource(nullptr);
            readerB.reset();
            readerB = std::move(newSource);
            transportB.setSource(readerB.get(), 0, nullptr, sourceSampleRate);
            trackB.setText(file.getFileName(), juce::dontSendNotification);
            playB.setButtonText("PLAY");
        }

        status.setText("Pista cargada. Pulsa PLAY para reproducir.", juce::dontSendNotification);
    }
};

class DJStudioRBApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "DJ Studio RB"; }
    const juce::String getApplicationVersion() override { return "0.6.0"; }

    void initialise(const juce::String&) override { window.reset(new MainWindow()); }
    void shutdown() override { window = nullptr; }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow()
            : DocumentWindow("DJ Studio RB", juce::Colour(18, 20, 27),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new DJStudioRB(), true);
            centreWithSize(1100, 700);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION(DJStudioRBApplication)
