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

class MusicLibraryPanel : public juce::Component, private juce::ListBoxModel
{
public:
    std::function<void(int, const juce::File&)> onLoadTrack;
    std::function<void()> onClose;

    MusicLibraryPanel()
    {
        heading.setText("BIBLIOTECA MUSICAL", juce::dontSendNotification);
        heading.setFont(juce::Font(25.0f, juce::Font::bold));
        heading.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(heading);

        chooseFolder.setButtonText("ELEGIR CARPETA MUSICAL");
        searchLabel.setText("BUSCAR", juce::dontSendNotification);
        search.setTextToShowWhenEmpty("Nombre de canción o artista...", juce::Colours::grey);
        search.onTextChange = [this] { refreshFilteredTracks(); };
        addAndMakeVisible(chooseFolder);
        addAndMakeVisible(searchLabel);
        addAndMakeVisible(search);
        chooseFolder.onClick = [this] { chooseMusicFolder(); };

        tracks.setModel(this);
        tracks.setColour(juce::ListBox::backgroundColourId, juce::Colour(24, 29, 38));
        tracks.setColour(juce::ListBox::outlineColourId, juce::Colour(70, 83, 98));
        addAndMakeVisible(tracks);

        loadA.setButtonText("CARGAR SELECCIÓN EN DECK A");
        loadB.setButtonText("CARGAR SELECCIÓN EN DECK B");
        close.setButtonText("VOLVER A MEZCLADORA");
        addAndMakeVisible(loadA);
        addAndMakeVisible(loadB);
        addAndMakeVisible(close);

        loadA.onClick = [this] { loadSelected(0); };
        loadB.onClick = [this] { loadSelected(1); };
        close.onClick = [this] { if (onClose) onClose(); };

        info.setText("Elige una carpeta para buscar canciones de audio.", juce::dontSendNotification);
        info.setColour(juce::Label::textColourId, juce::Colour(180, 190, 205));
        info.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(info);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(28);
        heading.setBounds(area.removeFromTop(45));
        area.removeFromTop(12);
        auto top = area.removeFromTop(42);
        chooseFolder.setBounds(top.removeFromLeft(245));
        top.removeFromLeft(18);
        searchLabel.setBounds(top.removeFromLeft(75));
        search.setBounds(top);
        area.removeFromTop(12);
        info.setBounds(area.removeFromBottom(30));
        area.removeFromBottom(8);
        auto buttons = area.removeFromBottom(42);
        loadA.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2 - 8));
        buttons.removeFromLeft(16);
        loadB.setBounds(buttons.removeFromLeft(buttons.getWidth() - 190));
        buttons.removeFromLeft(10);
        close.setBounds(buttons);
        area.removeFromBottom(8);
        tracks.setBounds(area);
    }
private:
    juce::Label heading, searchLabel, info;
    juce::TextButton chooseFolder, loadA, loadB, close;
    juce::TextEditor search;
    juce::ListBox tracks;
    std::unique_ptr<juce::FileChooser> folderChooser;
    juce::Array<juce::File> allTracks, filteredTracks;

    int getNumRows() override { return filteredTracks.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (!juce::isPositiveAndBelow(row, filteredTracks.size()))
            return;
        if (selected)
            g.fillAll(juce::Colour(35, 95, 100));
        else if (row % 2 == 0)
        {
            g.setColour(juce::Colour(29, 35, 45));
            g.fillRect(0, 0, width, height);
        }
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f));
        g.drawText(filteredTracks.getReference(row).getFileName(), 12, 0, width - 24, height,
                   juce::Justification::centredLeft, true);
    }

    void chooseMusicFolder()
    {
        folderChooser = std::make_unique<juce::FileChooser>(
            "Selecciona tu carpeta de música",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory));
        folderChooser->launchAsync(juce::FileBrowserComponent::openMode |
                                   juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& chooser)
            {
                const auto folder = chooser.getResult();
                if (folder.isDirectory())
                    scanFolder(folder);
            });
    }

    void scanFolder(const juce::File& folder)
    {
        allTracks.clear();
        juce::Array<juce::File> found;
        folder.findChildFiles(found, juce::File::findFiles, true);
        for (const auto& file : found)
        {
            const auto ext = file.getFileExtension().toLowerCase();
            if (ext == ".mp3" || ext == ".wav" || ext == ".aif" ||
                ext == ".aiff" || ext == ".flac" || ext == ".ogg")
                allTracks.add(file);
        }
        refreshFilteredTracks();
        info.setText(juce::String(allTracks.size()) + " canciones encontradas en: " +
                     folder.getFileName(), juce::dontSendNotification);
    }

    void refreshFilteredTracks()
    {
        filteredTracks.clear();
        const auto query = search.getText().trim().toLowerCase();
        for (const auto& file : allTracks)
            if (query.isEmpty() || file.getFileName().toLowerCase().contains(query))
                filteredTracks.add(file);
        tracks.updateContent();
        tracks.deselectAllRows();
    }

    void loadSelected(int deck)
    {
        const int row = tracks.getSelectedRow();
        if (!juce::isPositiveAndBelow(row, filteredTracks.size()))
        {
            info.setText("Selecciona primero una canción de la lista.", juce::dontSendNotification);
            return;
        }
        if (onLoadTrack)
            onLoadTrack(deck, filteredTracks.getReference(row));
    }
};

class VUMeter : public juce::Component
{
public:
    void setLevel(float newLevel)
    {
        const float target = juce::jlimit(0.0f, 1.0f, newLevel);
        level = target > level ? target : juce::jmax(target, level * 0.88f);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(juce::Colour(8, 12, 18));
        g.fillRoundedRectangle(bounds, 4.0f);
        constexpr int segments = 20;
        constexpr float gap = 1.6f;
        const float segmentHeight = (bounds.getHeight() - gap * (segments - 1)) / segments;
        const int lit = juce::roundToInt(level * segments);
        for (int i = 0; i < segments; ++i)
        {
            const int fromBottom = segments - 1 - i;
            // Pioneer-inspired signal colors: green for normal, yellow for hot,
            // and red only when the signal approaches its peak.
            const juce::Colour colour = fromBottom >= 18 ? juce::Colour(245, 72, 65)
                                      : fromBottom >= 15 ? juce::Colour(245, 190, 55)
                                      : juce::Colour(35, 220, 145);
            g.setColour(fromBottom < lit ? colour : juce::Colour(48, 58, 67));
            const float y = bounds.getY() + i * (segmentHeight + gap);
            g.fillRoundedRectangle(bounds.getX() + 1.0f, y,
                                   bounds.getWidth() - 2.0f, segmentHeight, 2.0f);
        }
    }

private:
    float level = 0.0f;
};

class DJStudioRB : public juce::AudioAppComponent, private juce::Timer, public juce::FileDragAndDropTarget, public juce::MidiInputCallback
{
public:
    DJStudioRB()
    {
        formatManager.registerBasicFormats();

        setSize(1280, 720);
        setAudioChannels(0, 4);

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

        mixerPanel.setText("MIXER");
        addAndMakeVisible(mixerPanel);

        auto setupMixerKnob = [this](juce::Slider& slider)
        {
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                       juce::MathConstants<float>::pi * 2.8f, true);
            slider.setRange(0.0, 1.0, 0.01);
            slider.setValue(1.0);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 42, 18);
            slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(35, 210, 205));
            slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
            slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
            addAndMakeVisible(slider);
        };
        auto setupVerticalFader = [this](juce::Slider& slider)
        {
            slider.setSliderStyle(juce::Slider::LinearVertical);
            slider.setRange(0.0, 1.0, 0.01);
            slider.setValue(1.0);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 42, 18);
            slider.setColour(juce::Slider::trackColourId, juce::Colour(35, 210, 205));
            slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
            slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
            addAndMakeVisible(slider);
        };
        setupMixerKnob(channelFaderA);
        setupMixerKnob(channelFaderB);
        setupMixerKnob(masterFader);
        masterFader.setValue(0.85);
        setupMixerKnob(filterA);
        setupMixerKnob(filterB);
        filterA.setRange(-1.0, 1.0, 0.01);
        filterA.setValue(0.0);
        filterB.setRange(-1.0, 1.0, 0.01);
        filterB.setValue(0.0);

        crossfader.setSliderStyle(juce::Slider::LinearHorizontal);
        crossfader.setRange(0.0, 1.0, 0.001);
        crossfader.setValue(0.5);
        crossfader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        crossfader.setColour(juce::Slider::trackColourId, juce::Colour(35, 210, 205));
        crossfader.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        addAndMakeVisible(crossfader);

        mixerLabel.setText("CROSSFADER   A  ◀────●────▶  B", juce::dontSendNotification);
        mixerLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(mixerLabel);
        channelLabelA.setText("A", juce::dontSendNotification);
        channelLabelB.setText("B", juce::dontSendNotification);
        masterLabel.setText("MASTER", juce::dontSendNotification);
        clipLabel.setText("CLIP", juce::dontSendNotification);
        clipLabel.setJustificationType(juce::Justification::centred);
        clipLabel.setFont(juce::Font(11.0f, juce::Font::bold));
        clipLabel.setColour(juce::Label::textColourId, juce::Colour(105, 112, 122));
        addAndMakeVisible(clipLabel);
        vuLabelA.setText("VU A", juce::dontSendNotification);
        vuLabelB.setText("VU B", juce::dontSendNotification);
        for (auto* label : { &vuLabelA, &vuLabelB })
        {
            label->setJustificationType(juce::Justification::centred);
            label->setColour(juce::Label::textColourId, juce::Colour(190, 200, 215));
            label->setFont(juce::Font(10.0f, juce::Font::bold));
            addAndMakeVisible(label);
        }
        addAndMakeVisible(vuMeterA);
        addAndMakeVisible(vuMeterB);
        for (auto* label : { &channelLabelA, &channelLabelB, &masterLabel })
        {
            label->setJustificationType(juce::Justification::centred);
            label->setColour(juce::Label::textColourId, juce::Colour(190, 200, 215));
            addAndMakeVisible(label);
        }

        status.setText("Listo. Carga una canción en cada deck.", juce::dontSendNotification);
        status.setJustificationType(juce::Justification::centred);
        status.setColour(juce::Label::textColourId, juce::Colour(180, 190, 205));
        addAndMakeVisible(status);

        setupDeckControls();

        audioSettingsButton.setButtonText("AUDIO");
        audioSettingsButton.onClick = [this] { showAudioSettings(); };
        addAndMakeVisible(audioSettingsButton);

        midiLabel.setText("MIDI DDJ-RB", juce::dontSendNotification);
        midiLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(midiLabel);
        midiDeviceBox.setTextWhenNothingSelected("Seleccionar controlador MIDI");
        midiDeviceBox.onChange = [this] { connectMidiInput(); };
        addAndMakeVisible(midiDeviceBox);
        refreshMidiButton.setButtonText("BUSCAR MIDI");
        refreshMidiButton.onClick = [this] { refreshMidiDevices(); };
        addAndMakeVisible(refreshMidiButton);
        refreshMidiDevices();

        libraryButton.setButtonText("BIBLIOTECA");
        libraryButton.onClick = [this]
        {
            libraryPanel.setVisible(true);
            libraryPanel.toFront(true);
        };
        addAndMakeVisible(libraryButton);
        libraryPanel.onLoadTrack = [this](int deck, const juce::File& file)
        {
            loadTrackFromFile(deck, file);
            libraryPanel.setVisible(false);
        };
        libraryPanel.onClose = [this] { libraryPanel.setVisible(false); };
        addAndMakeVisible(libraryPanel);
        libraryPanel.setVisible(false);

        startTimerHz(60);
    }

    ~DJStudioRB() override
    {
        midiInput.reset();
        shutdownAudio();
        transportA.setSource(nullptr);
        transportB.setSource(nullptr);
    }

    // Accept audio files dragged from Finder/Explorer directly onto either deck.
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (const auto& path : files)
        {
            const juce::String extension = juce::File(path).getFileExtension().toLowerCase();
            if (extension == ".mp3" || extension == ".wav" || extension == ".aif" ||
                extension == ".aiff" || extension == ".flac" || extension == ".ogg")
                return true;
        }
        return false;
    }

    void fileDragEnter(const juce::StringArray&, int, int) override
    {
        status.setText("Suelta la canción dentro de DECK A o DECK B.",
                       juce::dontSendNotification);
    }

    void fileDragExit(const juce::StringArray&) override
    {
        status.setText("Listo. Arrastra una canción a uno de los decks.",
                       juce::dontSendNotification);
    }

    void filesDropped(const juce::StringArray& files, int x, int y) override
    {
        if (files.isEmpty())
            return;

        const juce::Point<int> dropPosition(x, y);
        const int deckNumber = deckA.getBounds().contains(dropPosition) ? 0
                            : deckB.getBounds().contains(dropPosition) ? 1 : -1;

        if (deckNumber < 0)
        {
            status.setText("Suelta el archivo dentro del recuadro DECK A o DECK B.",
                           juce::dontSendNotification);
            return;
        }

        const juce::File audioFile(files[0]);
        if (!audioFile.existsAsFile())
        {
            status.setText("No se encontró el archivo de audio.",
                           juce::dontSendNotification);
            return;
        }

        loadTrackFromFile(deckNumber, audioFile);
        if (files.size() > 1)
            status.setText("Se cargó la primera canción. Arrastra otra al otro deck.",
                           juce::dontSendNotification);
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

        const int outputChannels = info.buffer->getNumChannels();
        const int channels = juce::jmin(2, outputChannels);
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
        applyFilter(scratchA, info.numSamples, static_cast<float>(filterA.getValue()), filterStateA);
        applyFilter(scratchB, info.numSamples, static_cast<float>(filterB.getValue()), filterStateB);

        const float gainA = (1.0f - blend) * static_cast<float>(volumeA.getValue())
                         * static_cast<float>(channelFaderA.getValue())
                         * static_cast<float>(masterFader.getValue());
        const float gainB = blend * static_cast<float>(volumeB.getValue())
                         * static_cast<float>(channelFaderB.getValue())
                         * static_cast<float>(masterFader.getValue());

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

        float peakA = 0.0f, peakB = 0.0f;
        const float fadeStep = 1.0f / static_cast<float>(jogFadeLength);
        for (int sample = 0; sample < info.numSamples; ++sample)
        {
            for (int channel = 0; channel < channels; ++channel)
            {
                const float deckA = scratchA.getSample(channel, sample);
                const float deckB = scratchB.getSample(channel, sample);
                const float sampleA = deckA * gainA * jogGainA;
                const float sampleB = deckB * gainB * jogGainB;
                const float mixedSample = sampleA + sampleB;
                if (std::abs(mixedSample) >= 0.999f)
                    clipHoldTicks.store(60); // Hold the CLIP warning for about one second.
                info.buffer->setSample(channel, info.startSample + sample, mixedSample);

                // DDJ-RB USB audio: outputs 1/2 feed MASTER RCA; outputs 3/4 feed headphones.
                // Headphone CUE is independent of the crossfader and master output level.
                if (outputChannels >= 4)
                {
                    const float cueA = headphoneCueA.load() ? deckA
                        * static_cast<float>(volumeA.getValue())
                        * static_cast<float>(channelFaderA.getValue()) : 0.0f;
                    const float cueB = headphoneCueB.load() ? deckB
                        * static_cast<float>(volumeB.getValue())
                        * static_cast<float>(channelFaderB.getValue()) : 0.0f;
                    info.buffer->setSample(channel + 2, info.startSample + sample,
                                           (cueA + cueB) * 0.8f);
                }
                peakA = juce::jmax(peakA, std::abs(sampleA));
                peakB = juce::jmax(peakB, std::abs(sampleB));
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
        meterLevelA.store(peakA);
        meterLevelB.store(peakB);
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

    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) override
    {
        midiMessageCount.fetch_add(1);
        midiLastStatus.store(message.getRawDataSize() > 0 ? message.getRawData()[0] : 0);
        midiLastData1.store(message.getRawDataSize() > 1 ? message.getRawData()[1] : 0);
        midiLastData2.store(message.getRawDataSize() > 2 ? message.getRawData()[2] : 0);
    }

    void refreshMidiDevices()
    {
        midiInput.reset();
        midiDeviceBox.clear(juce::dontSendNotification);
        const auto devices = juce::MidiInput::getAvailableDevices();
        for (int i = 0; i < devices.size(); ++i)
            midiDeviceBox.addItem(devices[i].name, i + 1);
        if (devices.isEmpty())
        {
            midiLabel.setText("No se detectan dispositivos MIDI", juce::dontSendNotification);
            return;
        }
        midiDeviceBox.setSelectedId(1, juce::dontSendNotification);
        midiLabel.setText("Controladores MIDI detectados: " + juce::String(devices.size()),
                          juce::dontSendNotification);
    }

    void connectMidiInput()
    {
        midiInput.reset();
        const auto devices = juce::MidiInput::getAvailableDevices();
        const int index = midiDeviceBox.getSelectedId() - 1;
        if (index < 0 || index >= devices.size())
            return;
        midiInput = juce::MidiInput::openDevice(devices[index].identifier, this);
        if (midiInput != nullptr)
        {
            midiInput->start();
            midiLabel.setText("MIDI conectado: " + devices[index].name, juce::dontSendNotification);
            status.setText("Mueve un control físico para comprobar la señal MIDI.", juce::dontSendNotification);
        }
        else
            midiLabel.setText("No se pudo abrir el dispositivo MIDI.", juce::dontSendNotification);
    }

    void resized() override
    {
        const int w = getWidth();
        const int margin = 18;
        const int gap = 14;
        const int mixerW = juce::jlimit(286, 320, w / 4);
        const int deckW = juce::jmax(360, (w - 2 * margin - mixerW - 2 * gap) / 2);
        const int deckY = 100;
        const int deckH = 470;
        const int mixerX = margin + deckW + gap;
        const int deckBX = mixerX + mixerW + gap;

        title.setBounds(0, 18, w, 55);
        libraryButton.setBounds(w - margin - 140, 28, 140, 34);
        audioSettingsButton.setBounds(w - margin - 220, 28, 70, 34);
        midiLabel.setBounds(margin + 10, 62, 120, 22);
        midiDeviceBox.setBounds(margin + 132, 62, 230, 24);
        refreshMidiButton.setBounds(margin + 370, 62, 110, 24);
        libraryPanel.setBounds(getLocalBounds());

        // Equal-width decks, mirrored around the centered mixer.
        deckA.setBounds(margin, deckY, deckW, deckH);
        deckB.setBounds(deckBX, deckY, deckW, deckH);

        const int buttonY = 155;
        const int buttonGap = 10;
        const int loadW = 112, playW = 92, stopW = 92;
        const int buttonsWidth = loadW + playW + stopW + 2 * buttonGap;

        const int buttonsAX = margin + (deckW - buttonsWidth) / 2;
        loadA.setBounds(buttonsAX, buttonY, loadW, 42);
        playA.setBounds(buttonsAX + loadW + buttonGap, buttonY, playW, 42);
        stopA.setBounds(buttonsAX + loadW + buttonGap + playW + buttonGap, buttonY, stopW, 42);
        trackA.setBounds(margin + 20, 210, deckW - 40, 48);
        timeA.setBounds(margin + 20, 265, deckW - 40, 24);
        seekA.setBounds(margin + 20, 290, deckW - 40, 24);
        cueA.setBounds(margin + 20, 330, 85, 34);
        volumeLabelA.setBounds(margin + 115, 330, 85, 34);
        volumeA.setBounds(margin + 200, 330, deckW - 225, 34);
        jogWheelA.setBounds(margin + (deckW - 110) / 2, 432, 110, 110);

        const int buttonsBX = deckBX + (deckW - buttonsWidth) / 2;
        loadB.setBounds(buttonsBX, buttonY, loadW, 42);
        playB.setBounds(buttonsBX + loadW + buttonGap, buttonY, playW, 42);
        stopB.setBounds(buttonsBX + loadW + buttonGap + playW + buttonGap, buttonY, stopW, 42);
        trackB.setBounds(deckBX + 20, 210, deckW - 40, 48);
        timeB.setBounds(deckBX + 20, 265, deckW - 40, 24);
        seekB.setBounds(deckBX + 20, 290, deckW - 40, 24);
        cueB.setBounds(deckBX + 20, 330, 85, 34);
        volumeLabelB.setBounds(deckBX + 115, 330, 85, 34);
        volumeB.setBounds(deckBX + 200, 330, deckW - 225, 34);
        jogWheelB.setBounds(deckBX + (deckW - 110) / 2, 432, 110, 110);

        mixerPanel.setBounds(mixerX, 105, mixerW, 540);

        const int colW = 90;
        const int colAX = mixerX + 12;
        const int colBX = mixerX + mixerW - 12 - colW;

        channelLabelA.setBounds(colAX, 132, colW, 18);
        channelLabelB.setBounds(colBX, 132, colW, 18);
        channelLabelGainA.setBounds(colAX, 151, colW, 18);
        channelFaderA.setBounds(colAX + 5, 168, colW - 10, 58);
        channelLabelGainB.setBounds(colBX, 151, colW, 18);
        channelFaderB.setBounds(colBX + 5, 168, colW - 10, 58);

        eqLabelLowA.setBounds(colAX, 226, colW, 18);
        eqLowA.setBounds(colAX + 5, 242, colW - 10, 58);
        eqLabelMidA.setBounds(colAX, 300, colW, 18);
        eqMidA.setBounds(colAX + 5, 316, colW - 10, 58);
        eqLabelHighA.setBounds(colAX, 374, colW, 18);
        eqHighA.setBounds(colAX + 5, 390, colW - 10, 58);

        eqLabelLowB.setBounds(colBX, 226, colW, 18);
        eqLowB.setBounds(colBX + 5, 242, colW - 10, 58);
        eqLabelMidB.setBounds(colBX, 300, colW, 18);
        eqMidB.setBounds(colBX + 5, 316, colW - 10, 58);
        eqLabelHighB.setBounds(colBX, 374, colW, 18);
        eqHighB.setBounds(colBX + 5, 390, colW - 10, 58);

        // Keep both VU meters paired and centered between A and B channels.
        const int vuX = mixerX + (mixerW - 42) / 2;
        vuLabelA.setBounds(vuX - 2, 278, 22, 20);
        vuMeterA.setBounds(vuX, 300, 18, 108);
        vuLabelB.setBounds(vuX + 22, 278, 22, 20);
        vuMeterB.setBounds(vuX + 22, 300, 18, 108);
        headphoneCueAButton.setBounds(mixerX + (mixerW - 126) / 2, 416, 60, 26);
        headphoneCueBButton.setBounds(mixerX + (mixerW - 126) / 2 + 66, 416, 60, 26);

        volumeLabelA.setBounds(mixerX + 8, 450, 70, 18);
        volumeA.setBounds(mixerX + 12, 468, 58, 62);
        volumeLabelB.setBounds(mixerX + mixerW - 78, 450, 70, 18);
        volumeB.setBounds(mixerX + mixerW - 70, 468, 58, 62);

        filterLabelA.setBounds(mixerX + 82, 450, 72, 18);
        filterA.setBounds(mixerX + 82, 468, 72, 54);
        filterLabelB.setBounds(mixerX + mixerW - 154, 450, 72, 18);
        filterB.setBounds(mixerX + mixerW - 154, 468, 72, 54);

        masterLabel.setBounds(mixerX + (mixerW - 80) / 2, 526, 80, 18);
        masterFader.setBounds(mixerX + (mixerW - 80) / 2, 542, 80, 42);
        clipLabel.setBounds(mixerX + mixerW - 66, 542, 55, 26);
        mixerLabel.setBounds(mixerX + 15, 584, mixerW - 30, 18);
        crossfader.setBounds(mixerX + 20, 600, mixerW - 40, 24);
        status.setBounds(margin, 650, w - 2 * margin, 25);
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

    juce::Label title, mixerLabel, status, channelLabelA, channelLabelB, masterLabel, vuLabelA, vuLabelB, clipLabel;
    std::atomic<int> clipHoldTicks{ 0 };
    VUMeter vuMeterA, vuMeterB;
    std::atomic<float> meterLevelA{ 0.0f }, meterLevelB{ 0.0f };
    juce::Label eqLabelLowA, eqLabelMidA, eqLabelHighA, eqLabelLowB, eqLabelMidB, eqLabelHighB;
    juce::Label filterLabelA, filterLabelB, channelLabelGainA, channelLabelGainB;
    juce::GroupComponent mixerPanel;
    juce::Slider channelFaderA, channelFaderB, masterFader, filterA, filterB;
    juce::TextButton libraryButton, audioSettingsButton, refreshMidiButton;
    juce::ComboBox midiDeviceBox;
    juce::Label midiLabel;
    std::unique_ptr<juce::MidiInput> midiInput;
    std::atomic<int> midiMessageCount{ 0 }, midiLastStatus{ 0 }, midiLastData1{ 0 }, midiLastData2{ 0 };
    juce::TextButton headphoneCueAButton{ "CUE A" }, headphoneCueBButton{ "CUE B" };
    std::atomic<bool> headphoneCueA{ false }, headphoneCueB{ false };
    MusicLibraryPanel libraryPanel;
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
    float filterStateA[2] = { 0.0f, 0.0f }, filterStateB[2] = { 0.0f, 0.0f };

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

    void applyFilter(juce::AudioBuffer<float>& buffer, int numSamples,
                     float amount, float* state)
    {
        if (std::abs(amount) < 0.001f)
            return;

        const float magnitude = std::abs(amount);
        const float cutoff = amount < 0.0f
            ? 18000.0f * std::pow(200.0f / 18000.0f, magnitude)
            : 30.0f * std::pow(5000.0f / 30.0f, magnitude);
        const float alpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi
                                            * cutoff / static_cast<float>(outputSampleRate));
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* samples = buffer.getWritePointer(channel);
            float low = state[channel];
            for (int i = 0; i < numSamples; ++i)
            {
                low += alpha * (samples[i] - low);
                samples[i] = amount < 0.0f ? low : samples[i] - low;
            }
            state[channel] = low;
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
            slider.setSliderStyle(juce::Slider::LinearVertical);
            slider.setRange(0.0, 1.0, 0.01);
            slider.setValue(1.0);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 46, 18);
            slider.setColour(juce::Slider::trackColourId, juce::Colour(35, 210, 205));
            slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
            slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
            addAndMakeVisible(slider);
        };
        setupVolume(volumeA);
        setupVolume(volumeB);

        auto setupEq = [this](juce::Slider& slider)
        {
            // Rotary EQ knobs with a compact value readout underneath.
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                                       juce::MathConstants<float>::pi * 2.8f, true);
            slider.setRange(0.0, 2.0, 0.01);
            slider.setValue(1.0);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 42, 18);
            slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(35, 210, 205));
            slider.setColour(juce::Slider::thumbColourId, juce::Colours::white);
            slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
            addAndMakeVisible(slider);
        };
        for (auto* slider : { &eqLowA, &eqMidA, &eqHighA, &eqLowB, &eqMidB, &eqHighB })
            setupEq(*slider);

        auto setupEqLabel = [this](juce::Label& label, const juce::String& text)
        {
            label.setText(text, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            label.setColour(juce::Label::textColourId, juce::Colour(190, 200, 215));
            addAndMakeVisible(label);
        };
        setupEqLabel(eqLabelLowA, "LOW");
        setupEqLabel(eqLabelMidA, "MID");
        setupEqLabel(eqLabelHighA, "HIGH");
        setupEqLabel(eqLabelLowB, "LOW");
        setupEqLabel(eqLabelMidB, "MID");
        setupEqLabel(eqLabelHighB, "HIGH");
        setupEqLabel(filterLabelA, "FILTER A");
        setupEqLabel(filterLabelB, "FILTER B");
        setupEqLabel(channelLabelGainA, "LEVEL A");
        setupEqLabel(channelLabelGainB, "LEVEL B");
        setupEqLabel(channelLabelGainB, "LEVEL B");
        eqTitleA.setVisible(false);
        eqTitleB.setVisible(false);

        cueA.onClick = [this] { cueDeck(0); };
        cueB.onClick = [this] { cueDeck(1); };

        for (auto* button : { &headphoneCueAButton, &headphoneCueBButton })
        {
            button->setClickingTogglesState(true);
            button->setColour(juce::TextButton::buttonColourId, juce::Colour(45, 55, 70));
            button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            button->setColour(juce::TextButton::textColourOnId, juce::Colour(35, 220, 190));
            addAndMakeVisible(button);
        }
        headphoneCueAButton.onClick = [this]
        {
            headphoneCueA.store(headphoneCueAButton.getToggleState());
            status.setText(headphoneCueA.load()
                ? "Preescucha de DECK A activada en audífonos."
                : "Preescucha de DECK A desactivada.", juce::dontSendNotification);
        };
        headphoneCueBButton.onClick = [this]
        {
            headphoneCueB.store(headphoneCueBButton.getToggleState());
            status.setText(headphoneCueB.load()
                ? "Preescucha de DECK B activada en audífonos."
                : "Preescucha de DECK B desactivada.", juce::dontSendNotification);
        };

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
        volumeLabelA.setText("VOLUME A", juce::dontSendNotification);
        volumeLabelB.setText("VOLUME B", juce::dontSendNotification);
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
        vuMeterA.setLevel(meterLevelA.load());
        vuMeterB.setLevel(meterLevelB.load());
        const int midiCount = midiMessageCount.load();
        static int previousMidiCount = 0;
        if (midiCount != previousMidiCount)
        {
            previousMidiCount = midiCount;
            status.setText("MIDI recibido: " + juce::String(midiLastStatus.load())
                + " / " + juce::String(midiLastData1.load()) + " / "
                + juce::String(midiLastData2.load()) + "  (" + juce::String(midiCount) + ")",
                juce::dontSendNotification);
        }
        int clipTicks = clipHoldTicks.load();
        if (clipTicks > 0)
        {
            clipLabel.setColour(juce::Label::textColourId, juce::Colour(255, 65, 60));
            clipHoldTicks.compare_exchange_strong(clipTicks, clipTicks - 1);
        }
        else
        {
            clipLabel.setColour(juce::Label::textColourId, juce::Colour(105, 112, 122));
        }

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

    void showAudioSettings()
    {
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(new juce::AudioDeviceSelectorComponent(
            deviceManager, 0, 0, 2, 4, false, false, true, false));
        options.dialogTitle = "Configuración de audio - DJ Studio RB";
        options.dialogBackgroundColour = juce::Colour(24, 29, 38);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.launchAsync();
        status.setText("Selecciona Pioneer DDJ-RB y activa 4 canales de salida: 1/2 MASTER RCA, 3/4 audífonos.",
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
    const juce::String getApplicationVersion() override { return "0.8.0"; }

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