
#pragma once
#include <array>
#include <vector>
#include <map>
#include "PadVoice.h"
#include "SamplePool.h"
#include "Slicer.h"
#include <atomic>
#include <thread>
class AudioEngine {
public:
    void prepare (double sampleRate, int blockSize) {
        sr = sampleRate; for (auto& v : voices) v.prepare (sampleRate, blockSize); slicer.prepare (sampleRate);
        // update min-gap in samples when sample rate changes
        setMinGapMs (minGapMs);
    }
    void setParams (float attack, float release, float cutoff, float reso, float gainDb) {
        for (auto& v : voices) v.setParams (attack, release, cutoff, reso, gainDb);
    }
    bool loadFile (const juce::File& f) {
        const juce::ScopedLock sl (dataLock);
        if (! pool.loadFromFile (f)) return false; buildSlices(); return true;
    }
    bool loadFileAsync (const juce::File& f) {
        bool expected = false;
        if (! loading.compare_exchange_strong (expected, true))
            return false; // already loading
        stopPreview();
        if (loader && loader->joinable()) loader->join();
        loader = std::make_unique<std::thread>([this, f]{
            this->loadFile (f);
            loading.store (false);
        });
        return true;
    }
    void setSliceControls (int newBaseNote, int newMaxSlices, float newSensitivity) {
        newBaseNote   = juce::jlimit (0, 127, newBaseNote);
        newMaxSlices  = juce::jlimit (1, 128, newMaxSlices);
        newSensitivity = juce::jlimit (0.6f, 2.0f, newSensitivity);
        if (baseNote != newBaseNote || maxSlices != newMaxSlices || std::abs (sensitivity - newSensitivity) > 1.0e-4f) {
            pushSnapshot();
            baseNote = newBaseNote; maxSlices = newMaxSlices; sensitivity = newSensitivity; buildSlices();
        }
    }
    void setMinGapMs (float ms) {
        minGapMs = juce::jlimit (1.0f, 500.0f, ms);
        minGapSamples = (int) std::round ((minGapMs / 1000.0f) * (float) sr);
        if (minGapSamples < 1) minGapSamples = 1;
    }
    void render (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
        buffer.clear();
        if (loading.load()) return;
        juce::ScopedTryLock tryLock (dataLock);
        if (! tryLock.isLocked()) return;
        // Preview playback of the long file
        if (previewPlaying && pool.getBuffer().getNumSamples() > 0) {
            const auto& src = pool.getBuffer();
            const int total = src.getNumSamples();
            const int loopStart = juce::jlimit (0, total, loopStartSample);
            const int loopEnd   = juce::jlimit (loopStart, total, loopEndSample > 0 ? loopEndSample : total);
            int remaining = total - previewPos;
            int toCopy = juce::jmin (buffer.getNumSamples(), remaining);
            if (toCopy > 0) {
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                    buffer.addFrom (ch, 0, src, juce::jmin (ch, src.getNumChannels()-1), previewPos, toCopy, 0.5f);
                previewPos += toCopy;
            }
            const int boundary = loopPreview ? loopEnd : total;
            if (previewPos >= boundary) {
                if (loopPreview) {
                    previewPos = loopStart;
                } else {
                    previewPlaying = false; previewPos = total;
                }
            }
        }
        for (const auto meta : midi) {
            const auto m = meta.getMessage();
            if (m.isNoteOn()) {
                int idx = m.getNoteNumber() - baseNote;
                if (idx >= 0 && idx < (int) slices.size()) {
                    const PadSlice& slice = slices[(size_t) idx];
                    if (slice.endSample <= slice.startSample) continue;
                    if (chokeEnabled) { for (auto& v : voices) if (v.isActive()) v.kill(); }
                    for (auto& v : voices) { if (! v.isActive()) { v.startNote (pool.getBuffer(), slice); break; } }
                }
            } else if (m.isNoteOff()) {
                if (gateEnabled) {
                    int note = m.getNoteNumber();
                    for (auto& v : voices) if (v.isPlayingMidi (note)) v.stopNote();
                }
            }
        }
        for (auto& v : voices) v.render (buffer, 0, buffer.getNumSamples());
    }
    const SamplePool& getPool() const { return pool; }
    const std::vector<PadSlice>& getSlices() const { return slices; }
    const WaveformCache& getWaveform() const { return pool.getWaveform(); }
    bool isLoading() const { return loading.load(); }
    // Undo/Redo
    bool canUndo() const { return historyIndex > 0; }
    bool canRedo() const { return historyIndex + 1 < (int) history.size(); }
    bool undo() {
        const juce::ScopedLock sl (dataLock);
        if (! canUndo()) return false;
        --historyIndex; restoreFromSnapshot (history[(size_t) historyIndex]);
        return true;
    }
    bool redo() {
        const juce::ScopedLock sl (dataLock);
        if (! canRedo()) return false;
        ++historyIndex; restoreFromSnapshot (history[(size_t) historyIndex]);
        return true;
    }
    // Editing: move a boundary at slice index 'i' (i >= 1) to 'newSample'.
    bool moveBoundary (int i, int newSample) {
        const juce::ScopedLock sl (dataLock);
        if (i <= 0 || i >= (int) slices.size()) return false;
        pushSnapshot();
        const int total = pool.getBuffer().getNumSamples();
        const int mg = juce::jmax (1, minGapSamples);
        // Boundaries cannot cross neighbours and must respect min gap
        int leftLimit  = slices[(size_t) (i-1)].startSample + mg;
        int rightLimit = (i + 1 < (int) slices.size()) ? slices[(size_t) (i+1)].startSample - mg
                                                       : juce::jmax (mg, total - mg);
        int clamped = juce::jlimit (leftLimit, rightLimit, juce::jlimit (0, juce::jmax (0, total), newSample));
        int oldStart = slices[(size_t) i].startSample;
        if (clamped == oldStart) return false;
        // adjust neighbour endpoints
        slices[(size_t) (i-1)].endSample = clamped;
        slices[(size_t) i].startSample = clamped;
        // migrate per-slice gain mapping keyed by start
        auto it = gainByStart.find (oldStart);
        if (it != gainByStart.end()) {
            float g = it->second; gainByStart.erase (it); gainByStart[clamped] = g;
            slices[(size_t) i].gainLin = g; // keep current slice gain consistent
        }
        return true;
    }
    // Delete slice i: merges into previous if possible, else into next
    bool deleteSlice (int i) {
        const juce::ScopedLock sl (dataLock);
        if (i < 0 || i >= (int) slices.size()) return false;
        if (slices.size() <= 1) return false;
        pushSnapshot();
        const int mg = juce::jmax (1, minGapSamples);
        if (i > 0) {
            // merge into previous
            slices[(size_t) (i-1)].endSample = slices[(size_t) i].endSample;
            gainByStart.erase (slices[(size_t) i].startSample);
            slices.erase (slices.begin() + i);
        } else {
            // i == 0, merge into next by moving its start to 0
            int newStart = 0;
            newStart = juce::jlimit (0, slices[(size_t) 1].endSample - mg, newStart);
            int oldStart = slices[(size_t) 1].startSample;
            slices[(size_t) 1].startSample = newStart;
            slices.erase (slices.begin());
            // migrate gain mapping
            auto it = gainByStart.find (oldStart);
            if (it != gainByStart.end()) { float g = it->second; gainByStart.erase (it); gainByStart[newStart] = g; }
        }
        // Reassign midi notes to keep consecutive mapping from baseNote
        for (size_t k = 0; k < slices.size(); ++k)
            slices[k].midiNote = baseNote + (int) k;
        return true;
    }
    // Preview controls
    void togglePreview() { if (pool.getBuffer().getNumSamples() == 0) return; previewPlaying = ! previewPlaying; if (previewPlaying && previewPos >= pool.getBuffer().getNumSamples()) previewPos = 0; }
    void startPreview() { if (pool.getBuffer().getNumSamples() == 0) return; previewPlaying = true; if (previewPos >= pool.getBuffer().getNumSamples()) previewPos = 0; }
    void stopPreview()  { previewPlaying = false; }
    void setLoopPreview (bool shouldLoop) { loopPreview = shouldLoop; }
    bool isLoopPreview () const { return loopPreview; }
    // Choke mode (mono)
    void setChoke (bool shouldChoke) { chokeEnabled = shouldChoke; }
    bool isChokeEnabled () const { return chokeEnabled; }
    // Gate mode (stop on note-off)
    void setGate (bool shouldGate) { gateEnabled = shouldGate; }
    bool isGateEnabled () const { return gateEnabled; }
    void setPreviewPositionNorm (float n) {
        n = juce::jlimit (0.0f, 1.0f, n);
        int total = pool.getBuffer().getNumSamples();
        previewPos = juce::jlimit (0, juce::jmax (0, total-1), (int) std::round (n * (float) total));
    }
    float getPreviewPositionNorm () const {
        int total = pool.getBuffer().getNumSamples();
        if (total <= 0) return 0.0f;
        return (float) juce::jlimit (0, total, previewPos) / (float) total;
    }
    void setLoopRegionNorm (float a, float b) {
        a = juce::jlimit (0.0f, 1.0f, a); b = juce::jlimit (0.0f, 1.0f, b);
        if (pool.getBuffer().getNumSamples() <= 0) { loopStartSample = 0; loopEndSample = 0; return; }
        if (b < a) std::swap (a, b);
        int total = pool.getBuffer().getNumSamples();
        loopStartSample = juce::jlimit (0, total, (int) std::round (a * total));
        loopEndSample   = juce::jlimit (0, total, (int) std::round (b * total));
    }
    std::pair<float,float> getLoopRegionNorm() const {
        int total = pool.getBuffer().getNumSamples();
        if (total <= 0 || loopEndSample <= loopStartSample) return { 0.f, 1.f };
        return { loopStartSample / (float) total, loopEndSample / (float) total };
    }
    void tapSliceAtCurrent() {
        pushSnapshot();
        const juce::ScopedLock sl (dataLock);
        if (pool.getBuffer().getNumSamples() == 0) return;
        int s = juce::jlimit (0, pool.getBuffer().getNumSamples()-1, previewPos);
        manualTaps.push_back (s);
        // Deduplicate nearby taps
        std::sort (manualTaps.begin(), manualTaps.end());
        const int mg = juce::jmax (1, minGapSamples);
        manualTaps.erase (std::unique (manualTaps.begin(), manualTaps.end(), [mg](int a, int b){ return std::abs (a-b) < mg; }), manualTaps.end());
        buildSlices();
    }
    // Per-slice gain control
    void setSliceGainDb (int index, float gainDb) {
        const juce::ScopedLock sl (dataLock);
        if (index < 0 || index >= (int) slices.size()) return;
        float g = juce::Decibels::decibelsToGain (gainDb);
        slices[(size_t) index].gainLin = g;
        gainByStart[slices[(size_t) index].startSample] = g;
    }
    float getSliceGainDb (int index) const {
        const juce::ScopedLock sl (dataLock);
        if (index < 0 || index >= (int) slices.size()) return 0.0f;
        return juce::Decibels::gainToDecibels (slices[(size_t) index].gainLin);
    }
    // Per-slice pitch/time/reverse
    void setSlicePitchSemitones (int index, float semitones) {
        const juce::ScopedLock sl (dataLock);
        if (index < 0 || index >= (int) slices.size()) return;
        slices[(size_t) index].pitchSemitones = juce::jlimit (-24.0f, 24.0f, semitones);
    }
    float getSlicePitchSemitones (int index) const {
        const juce::ScopedLock sl (dataLock);
        if (index < 0 || index >= (int) slices.size()) return 0.0f;
        return slices[(size_t) index].pitchSemitones;
    }
    void setSliceTimeRatio (int index, float ratio) {
        const juce::ScopedLock sl (dataLock);
        if (index < 0 || index >= (int) slices.size()) return;
        slices[(size_t) index].timeRatio = juce::jlimit (0.25f, 4.0f, ratio);
    }
    float getSliceTimeRatio (int index) const {
        const juce::ScopedLock sl (dataLock);
        if (index < 0 || index >= (int) slices.size()) return 1.0f;
        return slices[(size_t) index].timeRatio;
    }
    void setSliceReverse (int index, bool rev) {
        const juce::ScopedLock sl (dataLock);
        if (index < 0 || index >= (int) slices.size()) return;
        slices[(size_t) index].reverse = rev;
    }
    bool getSliceReverse (int index) const {
        const juce::ScopedLock sl (dataLock);
        if (index < 0 || index >= (int) slices.size()) return false;
        return slices[(size_t) index].reverse;
    }
    int getTotalLengthSamples() const { return pool.getBuffer().getNumSamples(); }
    private:
    void buildSlices() {
        slices.clear();
        if (pool.getBuffer().getNumSamples() == 0) return;
        slicer.setThresholdScale (sensitivity);
        auto slicePoints = slicer.slice (pool.getBuffer(), 0, maxSlices);
        std::vector<int> starts; starts.reserve (slicePoints.size() + manualTaps.size() + 1);
        starts.push_back (0);
        for (auto& sp : slicePoints) starts.push_back (sp.sampleIndex);
        for (auto s : manualTaps) starts.push_back (juce::jlimit (0, pool.getBuffer().getNumSamples()-1, s));
        std::sort (starts.begin(), starts.end());
        // Dedup close points
        const int mg = juce::jmax (1, minGapSamples);
        starts.erase (std::unique (starts.begin(), starts.end(), [mg](int a, int b){ return std::abs (a-b) < mg; }), starts.end());
        if ((int) starts.size() > maxSlices) starts.resize ((size_t) maxSlices);
        for (size_t i = 0; i < starts.size(); ++i) {
            int start = starts[i];
            int end = (i + 1 < starts.size()) ? starts[i+1] : pool.getBuffer().getNumSamples();
            float g = 1.0f; auto it = gainByStart.find (start); if (it != gainByStart.end()) g = it->second;
            PadSlice ps; ps.startSample = start; ps.endSample = end; ps.midiNote = (int) (baseNote + (int) i); ps.gainLin = g;
            slices.push_back (ps);
        }
    }
    juce::CriticalSection dataLock;
    std::atomic<bool> loading { false };
    std::unique_ptr<std::thread> loader;
    double sr { 44100.0 }; SamplePool pool; SpectralFluxSlicer slicer;
    std::array<PadVoice, 32> voices; std::vector<PadSlice> slices; int baseNote { 36 }; int maxSlices { 64 }; float sensitivity { 1.2f };
    std::vector<int> manualTaps; int previewPos { 0 }; bool previewPlaying { false }; bool loopPreview { false };
    int loopStartSample { 0 }; int loopEndSample { 0 };
    int minGapSamples { 128 }; float minGapMs { 30.0f };
    std::map<int, float> gainByStart;
    bool chokeEnabled { false };
    bool gateEnabled { false };
    struct Snapshot { std::vector<PadSlice> slices; std::map<int,float> gainByStart; };
    std::vector<Snapshot> history; int historyIndex { -1 }; const int historyMax { 64 };
    void pushSnapshot() {
        // truncate redo tail
        if (historyIndex + 1 < (int) history.size())
            history.erase (history.begin() + historyIndex + 1, history.end());
        history.push_back ({ slices, gainByStart });
        historyIndex = (int) history.size() - 1;
        if ((int) history.size() > historyMax) { history.erase (history.begin()); --historyIndex; }
    }
    void restoreFromSnapshot (const Snapshot& s) {
        slices = s.slices; gainByStart = s.gainByStart;
    }
};

