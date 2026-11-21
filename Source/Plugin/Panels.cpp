#include "Panels.h"
#include "Parameters.h"
#include "Presets.h"
#include "Theme.h"
#include <cmath>
#include <numeric>
#include <ranges>

namespace openedm::ui {

namespace {

namespace core = openedm::core;
namespace params = openedm::params;

constexpr double kMinHz = 20.0;
constexpr double kMaxHz = 20000.0;
constexpr float kMaxDb = 24.0f;
constexpr float kMinDb = -48.0f;
constexpr int kCurvePoints = 240;
constexpr int kSweepCurvePoints = 160;
constexpr int kRefreshHz = 30;
constexpr float kResonanceDragPixels = 150.0f;
constexpr float kSlopeDragPixels = 90.0f;
constexpr double kFallbackSampleRate = 44100.0;
constexpr int kCaptionHeight = 14;
constexpr int kSectionGap = 10;
constexpr const char* kSweepTitle = "Auto Sweep & Tempo Sync";
constexpr const char* kBandBypassHint = "Turn off BYPASS on this filter to use this control.";
constexpr const char* kSweepDrivenHint
    = "Auto Sweep sets this value. To use this control, turn off AUTO SWEEP or set the sweep target to the other filter.";

void drawLevelMeter(juce::Graphics& g, juce::Rectangle<int> section, float peak, juce::Colour accent) {
    const auto bounds = section.reduced(10, 0);
    const auto bar = juce::Rectangle<float>(static_cast<float>(bounds.getX()),
        static_cast<float>(bounds.getBottom() - 26), static_cast<float>(bounds.getWidth()), 7.0f);
    g.setColour(theme::kTrack);
    g.fillRoundedRectangle(bar, 3.5f);
    const float db = peak > 0.0f ? juce::Decibels::gainToDecibels(peak) : -60.0f;
    const float fill = juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 48.0f);
    g.setColour(peak >= 1.0f ? juce::Colour(0xffff665e) : accent);
    g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * fill), 3.5f);
    g.setFont(juce::FontOptions(9.0f));
    g.setColour(theme::kTextDim);
    const juce::String value = db <= -59.9f ? "-INF dBFS" : juce::String(db, 1) + " dBFS";
    g.drawText(value, bounds.withY(bounds.getBottom() - 17).withHeight(12), juce::Justification::centredLeft);
}

} // namespace

ResponseView::ResponseView(OpenEdmFilterProcessor& processor)
    : processor_(processor) {
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    startTimerHz(kRefreshHz);
}

juce::Rectangle<float> ResponseView::plotArea() const {
    return getLocalBounds().toFloat().reduced(14.0f, 12.0f);
}

float ResponseView::xForHz(double hz) const {
    const auto plot = plotArea();
    return plot.getX() + plot.getWidth() * static_cast<float>(std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz));
}

// Before the first processed block there is no modulated cutoff yet, so show the sweep or knob value.
double ResponseView::displayCutoff(int band) const {
    const double modulated = processor_.displayCutoffs[static_cast<size_t>(band)].load();
    if (modulated > 0.0) {
        return modulated;
    }
    const auto sweep = processor_.currentSweepSettings();
    if (core::AutoSweep::appliesToBand(sweep, band)) {
        return core::AutoSweep::valueAt(sweep, band, processor_.displaySweepPhase.load()).cutoffHz;
    }
    return processor_.parameters.getRawParameterValue(params::bandId(band, params::kCutoff))->load();
}

float ResponseView::displayResonance(int band) const {
    const auto sweep = processor_.currentSweepSettings();
    if (core::AutoSweep::appliesToBand(sweep, band)) {
        const float modulated = processor_.displayResonances[static_cast<size_t>(band)].load();
        if (modulated >= 0.0f) {
            return modulated;
        }
        return core::AutoSweep::valueAt(sweep, band, processor_.displaySweepPhase.load()).resonance;
    }
    return processor_.parameters.getRawParameterValue(params::bandId(band, params::kResonance))->load();
}

void ResponseView::mouseDown(const juce::MouseEvent& event) {
    // Auto Sweep overrides a swept band's cutoff and resonance, so a click moves only a band it leaves free.
    const auto sweep = processor_.currentSweepSettings();
    const auto isFree = [&](int band) { return !core::AutoSweep::appliesToBand(sweep, band); };
    if (!isFree(0) && !isFree(1))
        return;
    const auto distanceTo = [&](int band) { return std::abs(event.position.x - xForHz(displayCutoff(band))); };
    const int band = !isFree(1) || (isFree(0) && distanceTo(0) <= distanceTo(1)) ? 0 : 1;
    dragCutoff_ = processor_.parameters.getParameter(params::bandId(band, params::kCutoff));
    dragResonance_ = processor_.parameters.getParameter(params::bandId(band, params::kResonance));
    resonanceAtDragStart_ = dragResonance_->getValue();
    dragCutoff_->beginChangeGesture();
    dragResonance_->beginChangeGesture();
    mouseDrag(event);
}

void ResponseView::mouseDrag(const juce::MouseEvent& event) {
    if (dragCutoff_ == nullptr) {
        return;
    }
    const auto plot = plotArea();
    const float proportion = juce::jlimit(0.0f, 1.0f, (event.position.x - plot.getX()) / plot.getWidth());
    const auto hz = static_cast<float>(kMinHz * std::pow(kMaxHz / kMinHz, static_cast<double>(proportion)));
    dragCutoff_->setValueNotifyingHost(dragCutoff_->convertTo0to1(hz));
    const float resonance = resonanceAtDragStart_ - static_cast<float>(event.getDistanceFromDragStartY()) / kResonanceDragPixels;
    dragResonance_->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, resonance));
}

void ResponseView::mouseUp(const juce::MouseEvent&) {
    if (dragCutoff_ == nullptr) {
        return;
    }
    dragCutoff_->endChangeGesture();
    dragResonance_->endChangeGesture();
    dragCutoff_ = nullptr;
    dragResonance_ = nullptr;
}

void ResponseView::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kPanel);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    const auto plot = plotArea();
    const auto yForMagnitude = [&](double magnitude) {
        const auto db = static_cast<float>(juce::Decibels::gainToDecibels(magnitude, static_cast<double>(kMinDb)));
        return juce::jmap(juce::jlimit(kMinDb, kMaxDb, db), kMaxDb, kMinDb, plot.getY(), plot.getBottom());
    };

    g.setFont(juce::FontOptions(10.0f));
    const std::array<std::pair<double, const char*>, 8> gridLines{ {
        { 50.0, "50" }, { 100.0, "100" }, { 200.0, "200" }, { 500.0, "500" },
        { 1000.0, "1k" }, { 2000.0, "2k" }, { 5000.0, "5k" }, { 10000.0, "10k" } } };
    std::ranges::for_each(gridLines, [&](const auto& line) {
        const float x = xForHz(line.first);
        g.setColour(theme::kPanelEdge);
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        g.setColour(theme::kTextDim.withAlpha(0.6f));
        g.drawText(line.second, juce::Rectangle<float>(x + 3.0f, plot.getBottom() - 13.0f, 40.0f, 12.0f),
                   juce::Justification::centredLeft);
    });
    std::ranges::for_each(std::array{ 12.0f, 0.0f, -12.0f, -24.0f, -36.0f }, [&](float db) {
        const float y = yForMagnitude(juce::Decibels::decibelsToGain(static_cast<double>(db)));
        g.setColour(db == 0.0f ? theme::kTrack : theme::kPanelEdge);
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
        g.setColour(theme::kTextDim.withAlpha(0.6f));
        g.drawText((db > 0.0f ? "+" : "") + juce::String(juce::roundToInt(db)) + " dB",
                   juce::Rectangle<float>(plot.getX() + 3.0f, y + 1.0f, 50.0f, 12.0f), juce::Justification::centredLeft);
    });

    if (processor_.spectrumVisible.load(std::memory_order_relaxed)) {
        juce::Path spectrum;
        std::ranges::for_each(std::views::iota(size_t{0}, processor_.displaySpectrum.size()), [&](size_t bin) {
            const float magnitude = processor_.displaySpectrum[bin].load(std::memory_order_relaxed);
            const float db = juce::jlimit(-72.0f, 3.0f, juce::Decibels::gainToDecibels(magnitude, -72.0f));
            const float x = plot.getX() + static_cast<float>(bin) / static_cast<float>(processor_.displaySpectrum.size() - 1)
                * plot.getWidth();
            const float y = juce::jmap(db, -72.0f, 3.0f, plot.getBottom(), plot.getY());
            if (bin == 0) spectrum.startNewSubPath(x, y);
            else spectrum.lineTo(x, y);
        });
        g.setColour(theme::kAccentSpectrum.withAlpha(0.82f));
        g.strokePath(spectrum, juce::PathStrokeType(1.4f));
    }

    const auto& state = processor_.parameters;
    const auto value = [&](size_t band, const char* suffix) {
        return state.getRawParameterValue(params::bandId(static_cast<int>(band), suffix))->load();
    };
    const auto shapeFor = [&](size_t band) {
        return core::FilterShape{ value(band, params::kBypass) < 0.5f,
                                  static_cast<core::FilterType>(params::effectiveType(static_cast<int>(value(band, params::kType)),
                                      static_cast<int>(value(band, params::kExtendedType)))),
                                  static_cast<core::FilterSlope>(static_cast<int>(value(band, params::kSlope))),
                                  displayResonance(static_cast<int>(band)), value(band, params::kWidth) };
    };
    const std::array<core::FilterShape, 2> shapes{ shapeFor(0), shapeFor(1) };
    const std::array<double, 2> displayCutoffs{ displayCutoff(0), displayCutoff(1) };
    const std::array<juce::Colour, 2> accents{ theme::kAccentA, theme::kAccentB };
    const double hostRate = processor_.getSampleRate() > 0.0 ? processor_.getSampleRate() : kFallbackSampleRate;
    const double filterRate = hostRate * core::Oversampler::kFactor;

    const auto bandMagnitude = [&](size_t band, double hz) {
        return core::MultiModeFilter::magnitudeAt(shapes[band], displayCutoffs[band], hz, filterRate);
    };
    const bool parallel = state.getRawParameterValue("routing")->load() > 0.5f;
    const double blend = state.getRawParameterValue("parallel_blend")->load();
    const auto combinedMagnitude = [&](double hz) {
        const auto a = core::MultiModeFilter::responseAt(shapes[0], displayCutoffs[0], hz, filterRate);
        const auto b = core::MultiModeFilter::responseAt(shapes[1], displayCutoffs[1], hz, filterRate);
        return std::abs(parallel ? a + blend * (b - a) : a * b);
    };
    const auto curve = [&](const auto& magnitudeAt) {
        juce::Path path;
        std::ranges::for_each(std::views::iota(0, kCurvePoints), [&](int point) {
            const double hz = kMinHz * std::pow(kMaxHz / kMinHz, point / static_cast<double>(kCurvePoints - 1));
            const juce::Point<float> position{ xForHz(hz), yForMagnitude(magnitudeAt(hz)) };
            if (point == 0) {
                path.startNewSubPath(position);
                return;
            }
            path.lineTo(position);
        });
        return path;
    };

    std::ranges::for_each(std::views::iota(size_t{0}, size_t{2}), [&](size_t band) {
        g.setColour(accents[band].withAlpha(shapes[band].enabled ? 0.5f : 0.15f));
        g.strokePath(curve([&](double hz) { return bandMagnitude(band, hz); }), juce::PathStrokeType(1.5f));
    });

    const juce::Path combined = curve(combinedMagnitude);
    juce::Path fill(combined);
    fill.lineTo(plot.getRight(), plot.getBottom());
    fill.lineTo(plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(theme::kText.withAlpha(0.14f), 0.0f, plot.getY(),
                                           theme::kText.withAlpha(0.0f), 0.0f, plot.getBottom(), false));
    g.fillPath(fill);
    g.setColour(theme::kText);
    g.strokePath(combined, juce::PathStrokeType(2.0f));

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    const auto sweep = processor_.currentSweepSettings();
    std::ranges::for_each(std::views::iota(size_t{0}, size_t{2}), [&](size_t band) {
        const double cutoff = juce::jlimit(kMinHz, kMaxHz, displayCutoffs[band]);
        const juce::Point<float> marker{ xForHz(cutoff), yForMagnitude(combinedMagnitude(cutoff)) };
        const int resPercent = juce::roundToInt(shapes[band].resonance * 100.0f);
        const juce::String label = juce::String(band == 0 ? "A  " : "B  ")
            + params::formatHz(static_cast<float>(cutoff)) + "  (" + juce::String(resPercent) + "%)";
        const auto justification = band == 0 ? juce::Justification::centredLeft : juce::Justification::centredRight;
        g.setColour(accents[band].withAlpha(shapes[band].enabled ? 1.0f : 0.3f));
        // A hollow marker shows a band that Auto Sweep drives. A drag does not move it.
        const auto dot = juce::Rectangle<float>(9.0f, 9.0f).withCentre(marker);
        g.drawEllipse(dot.reduced(0.75f), 1.5f);
        if (!core::AutoSweep::appliesToBand(sweep, static_cast<int>(band)))
            g.fillEllipse(dot);
        g.drawText(label, plot.withHeight(16.0f).reduced(4.0f, 0.0f), justification);
    });
}

SweepSlopeEditor::SweepSlopeEditor(OpenEdmFilterProcessor& processor)
    : processor_(processor) {
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    startTimerHz(kRefreshHz);
}

const char* SweepSlopeEditor::parameterIdForHandle(Handle handle) noexcept {
    switch (handle) {
        case Handle::CutoffStart: return params::kSweepCutoffStart;
        case Handle::CutoffSlope: return params::kSweepCutoffSlope;
        case Handle::CutoffEnd: return params::kSweepCutoffEnd;
        case Handle::ResStart: return params::kSweepResStart;
        case Handle::ResSlope: return params::kSweepResSlope;
        case Handle::ResEnd: return params::kSweepResEnd;
        case Handle::None: break;
    }
    return nullptr;
}

juce::Rectangle<float> SweepSlopeEditor::plotArea() const {
    return getLocalBounds().toFloat().reduced(10.0f, 18.0f).withTrimmedTop(4.0f).withTrimmedBottom(14.0f);
}

float SweepSlopeEditor::yForHz(float hz) const {
    const double clamped = std::clamp(static_cast<double>(hz), kMinHz, kMaxHz);
    const auto norm = static_cast<float>(std::log(clamped / kMinHz) / std::log(kMaxHz / kMinHz));
    return yForNormalized(norm);
}

float SweepSlopeEditor::hzForY(float y) const {
    const float norm = normalizedForY(y);
    return static_cast<float>(kMinHz * std::pow(kMaxHz / kMinHz, static_cast<double>(norm)));
}

float SweepSlopeEditor::yForNormalized(float value) const {
    const auto plot = plotArea();
    return plot.getBottom() - std::clamp(value, 0.0f, 1.0f) * plot.getHeight();
}

float SweepSlopeEditor::normalizedForY(float y) const {
    const auto plot = plotArea();
    return std::clamp((plot.getBottom() - y) / std::max(1.0f, plot.getHeight()), 0.0f, 1.0f);
}

std::pair<float, float> SweepSlopeEditor::handlePhases(core::SweepMode mode, int bars) const {
    switch (mode) {
        case core::SweepMode::BarWindow:
        case core::SweepMode::BarMacro:
            return { 0.5f, 1.0f };
        case core::SweepMode::UpDown:
            return { 0.25f, 0.5f };
        case core::SweepMode::EveryBar: {
            const float oneBar = 1.0f / static_cast<float>(std::max(1, bars));
            return { 0.5f * oneBar, 1.0f };
        }
    }
    return { 0.5f, 1.0f };
}

juce::Point<float> SweepSlopeEditor::handlePosition(Handle handle, const core::AutoSweepSettings& settings) const {
    const auto plot = plotArea();
    const auto [midPhase, endPhase] = handlePhases(settings.mode, settings.bars);
    const auto xAt = [&](float phase) { return plot.getX() + phase * plot.getWidth(); };
    const auto samplePhase = [&](float phase) {
        return core::AutoSweep::valueAt(settings, 0, static_cast<double>( std::min(0.9999f, phase) ));
    };

    switch (handle) {
        case Handle::CutoffStart: return { xAt(0.0f), yForHz(settings.cutoffStartHz) };
        case Handle::CutoffSlope: return { xAt(midPhase), yForHz(samplePhase(midPhase).cutoffHz) };
        case Handle::CutoffEnd: return { xAt(endPhase), yForHz(settings.cutoffEndHz) };
        case Handle::ResStart: return { xAt(0.0f), yForNormalized(settings.resonanceStart) };
        case Handle::ResSlope: return { xAt(midPhase), yForNormalized(samplePhase(midPhase).resonance) };
        case Handle::ResEnd: return { xAt(endPhase), yForNormalized(settings.resonanceEnd) };
        case Handle::None: break;
    }
    return plot.getCentre();
}

void SweepSlopeEditor::mouseDown(const juce::MouseEvent& event) {
    const auto settings = processor_.currentSweepSettings();
    const std::array<Handle, 6> handles{
        Handle::CutoffStart, Handle::CutoffSlope, Handle::CutoffEnd,
        Handle::ResStart, Handle::ResSlope, Handle::ResEnd
    };
    activeHandle_ = *std::ranges::min_element(handles, [&](Handle a, Handle b) {
        return event.position.getDistanceFrom(handlePosition(a, settings))
             < event.position.getDistanceFrom(handlePosition(b, settings));
    });
    if (const char* id = parameterIdForHandle(activeHandle_)) {
        dragSlot_ = processor_.currentSweepSlot();
        activeParameter_ = processor_.parameters.getParameter(params::sweepId(dragSlot_, id));
        if (activeParameter_ != nullptr) {
            slopeAtDragStart_ = activeParameter_->convertFrom0to1(activeParameter_->getValue());
            activeParameter_->beginChangeGesture();
            mouseDrag(event);
        }
    }
}

void SweepSlopeEditor::mouseDrag(const juce::MouseEvent& event) {
    if (activeParameter_ == nullptr) {
        return;
    }
    if (dragSlot_ != processor_.currentSweepSlot()) {
        endDrag();
        return;
    }
    const auto settings = processor_.currentSweepSettings();
    switch (activeHandle_) {
        case Handle::CutoffStart:
        case Handle::CutoffEnd: {
            const float hz = hzForY(event.position.y);
            activeParameter_->setValueNotifyingHost(activeParameter_->convertTo0to1(hz));
            break;
        }
        case Handle::ResStart:
        case Handle::ResEnd: {
            const float res = normalizedForY(event.position.y);
            activeParameter_->setValueNotifyingHost(activeParameter_->convertTo0to1(res));
            break;
        }
        case Handle::CutoffSlope: {
            const float normStart = normalizedForY(yForHz(settings.cutoffStartHz));
            const float normEnd = normalizedForY(yForHz(settings.cutoffEndHz));
            const float span = normEnd - normStart;
            float slope = slopeAtDragStart_ + static_cast<float>(event.getDistanceFromDragStartY()) / kSlopeDragPixels;
            if (std::abs(span) > 0.04f) {
                const float fraction = (normalizedForY(event.position.y) - normStart) / span;
                slope = core::AutoSweep::slopeFromMidpoint(fraction);
            }
            activeParameter_->setValueNotifyingHost(activeParameter_->convertTo0to1(std::clamp(slope, -1.0f, 1.0f)));
            break;
        }
        case Handle::ResSlope: {
            const float span = settings.resonanceEnd - settings.resonanceStart;
            float slope = slopeAtDragStart_ + static_cast<float>(event.getDistanceFromDragStartY()) / kSlopeDragPixels;
            if (std::abs(span) > 0.04f) {
                const float fraction = (normalizedForY(event.position.y) - settings.resonanceStart) / span;
                slope = core::AutoSweep::slopeFromMidpoint(fraction);
            }
            activeParameter_->setValueNotifyingHost(activeParameter_->convertTo0to1(std::clamp(slope, -1.0f, 1.0f)));
            break;
        }
        case Handle::None:
            break;
    }
}

void SweepSlopeEditor::mouseUp(const juce::MouseEvent&) {
    endDrag();
}

void SweepSlopeEditor::endDrag() {
    if (activeParameter_ != nullptr) {
        activeParameter_->endChangeGesture();
        activeParameter_ = nullptr;
    }
    activeHandle_ = Handle::None;
}

void SweepSlopeEditor::mouseDoubleClick(const juce::MouseEvent& event) {
    const auto settings = processor_.currentSweepSettings();
    const float distCut = event.position.getDistanceFrom(handlePosition(Handle::CutoffSlope, settings));
    const float distRes = event.position.getDistanceFrom(handlePosition(Handle::ResSlope, settings));
    const char* id = distCut <= distRes ? params::kSweepCutoffSlope : params::kSweepResSlope;
    if (auto* param = processor_.parameters.getParameter(params::sweepId(processor_.currentSweepSlot(), id))) {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->convertTo0to1(0.0f));
        param->endChangeGesture();
    }
}

void SweepSlopeEditor::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(theme::kBackground.brighter(0.04f));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(theme::kPanelEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    const auto plot = plotArea();
    const auto settings = processor_.currentSweepSettings();
    const float activeAlpha = settings.enabled ? 1.0f : 0.45f;

    // Horizontal reference grid (25%, 50%, 75%)
    std::ranges::for_each(std::array{ 0.25f, 0.5f, 0.75f }, [&](float norm) {
        const float y = yForNormalized(norm);
        g.setColour(norm == 0.5f ? theme::kTrack.withAlpha(0.7f) : theme::kPanelEdge.withAlpha(0.5f));
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
    });

    // Vertical bar grid across the 1, 2, 4, 8, 16, 32 or 64 bar window
    const int bars = std::max(1, settings.bars);
    const int divisions = std::min(bars, 8);
    const int barsPerDivision = std::max(1, bars / divisions);
    g.setFont(juce::FontOptions(9.5f));
    std::ranges::for_each(std::views::iota(0, divisions + 1), [&](int div) {
        const float fraction = static_cast<float>(div) / static_cast<float>(divisions);
        const float x = plot.getX() + fraction * plot.getWidth();
        g.setColour(theme::kPanelEdge.withAlpha(0.65f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        if (div < divisions) {
            const int barNumber = div * barsPerDivision + 1;
            g.setColour(theme::kTextDim.withAlpha(0.65f));
            g.drawText("B" + juce::String(barNumber),
                       juce::Rectangle<float>(x + 2.0f, plot.getBottom() + 1.0f, 32.0f, 12.0f),
                       juce::Justification::centredLeft);
        }
    });

    const auto buildCurve = [&](const auto& yAtPhase) {
        juce::Path path;
        std::ranges::for_each(std::views::iota(0, kSweepCurvePoints), [&](int i) {
            const float phase = static_cast<float>(i) / static_cast<float>(kSweepCurvePoints - 1);
            const double samplePhase = std::min(0.9999, static_cast<double>(phase));
            const juce::Point<float> pt{ plot.getX() + phase * plot.getWidth(), yAtPhase(samplePhase) };
            if (i == 0) {
                path.startNewSubPath(pt);
                return;
            }
            path.lineTo(pt);
        });
        return path;
    };

    const juce::Path cutoffPath = buildCurve([&](double phase) {
        return yForHz(core::AutoSweep::valueAt(settings, 0, phase).cutoffHz);
    });
    juce::Path cutoffFill(cutoffPath);
    cutoffFill.lineTo(plot.getRight(), plot.getBottom());
    cutoffFill.lineTo(plot.getX(), plot.getBottom());
    cutoffFill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(theme::kAccentA.withAlpha(0.18f * activeAlpha), 0.0f, plot.getY(),
                                           theme::kAccentA.withAlpha(0.0f), 0.0f, plot.getBottom(), false));
    g.fillPath(cutoffFill);
    g.setColour(theme::kAccentA.withAlpha(activeAlpha));
    g.strokePath(cutoffPath, juce::PathStrokeType(2.0f));

    const juce::Path resPath = buildCurve([&](double phase) {
        return yForNormalized(core::AutoSweep::valueAt(settings, 0, phase).resonance);
    });
    g.setColour(theme::kAccentB.withAlpha(0.9f * activeAlpha));
    g.strokePath(resPath, juce::PathStrokeType(1.75f));

    const std::array envelopes{ settings.crush, settings.glitch };
    const std::array colours{ theme::kAccentFx, theme::kAccentB };
    for (size_t i = 0; i < envelopes.size(); ++i) {
        const auto path = buildCurve([&](double phase) {
            const float progress = core::AutoSweep::progressAt(settings.mode, settings.bars, phase);
            return yForNormalized(core::AutoSweep::envelopeAt(envelopes[i], progress));
        });
        g.setColour(colours[i].withAlpha(activeAlpha));
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

    // Live playhead across the multi-bar loop
    const float phase = std::clamp(processor_.displaySweepPhase.load(), 0.0f, 0.9999f);
    const float playheadX = plot.getX() + phase * plot.getWidth();
    const auto liveValue = core::AutoSweep::valueAt(settings, 0, static_cast<double>(phase));
    g.setColour(theme::kText.withAlpha(settings.enabled ? 0.65f : 0.2f));
    g.drawVerticalLine(juce::roundToInt(playheadX), plot.getY(), plot.getBottom());
    if (settings.enabled) {
        g.setColour(theme::kAccentA);
        g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre({ playheadX, yForHz(liveValue.cutoffHz) }));
        g.setColour(theme::kAccentB);
        g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre({ playheadX, yForNormalized(liveValue.resonance) }));
    }

    // Draw the 6 draggable handles (Start, Slope, End for Cutoff and Resonance)
    const auto drawHandle = [&](Handle handle, juce::Colour colour, bool isMidpoint) {
        const auto pos = handlePosition(handle, settings);
        const float radius = activeHandle_ == handle ? 5.5f : 4.5f;
        g.setColour(theme::kBackground);
        g.fillEllipse(juce::Rectangle<float>(radius * 2.0f + 2.0f, radius * 2.0f + 2.0f).withCentre(pos));
        g.setColour(colour.withAlpha(activeAlpha));
        if (isMidpoint) {
            g.drawEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(pos), 1.8f);
            g.fillEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre(pos));
        } else {
            g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(pos));
        }
    };
    drawHandle(Handle::CutoffStart, theme::kAccentA, false);
    drawHandle(Handle::CutoffSlope, theme::kAccentA, true);
    drawHandle(Handle::CutoffEnd, theme::kAccentA, false);
    drawHandle(Handle::ResStart, theme::kAccentB, false);
    drawHandle(Handle::ResSlope, theme::kAccentB, true);
    drawHandle(Handle::ResEnd, theme::kAccentB, false);

    // Header overlay showing live bar, time signature, BPM and slope endpoints
    const auto headerBar = getLocalBounds().toFloat().reduced(8.0f, 2.0f).removeFromTop(14.0f);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(theme::kAccentA);
    const juce::String cutSummary = "CUT " + params::formatHz(settings.cutoffStartHz) + " -> " + params::formatHz(settings.cutoffEndHz);
    g.drawText(cutSummary, headerBar.withWidth(155.0f), juce::Justification::centredLeft);

    g.setColour(theme::kAccentB);
    const juce::String resSummary = "RES " + juce::String(juce::roundToInt(settings.resonanceStart * 100.0f)) + "% -> "
        + juce::String(juce::roundToInt(settings.resonanceEnd * 100.0f)) + "%";
    g.drawText(resSummary, headerBar.withTrimmedLeft(155.0f).withWidth(105.0f), juce::Justification::centredLeft);

    const auto signature = processor_.displayTimeSignature.load();
    const float currentBar = phase * static_cast<float>(bars) + 1.0f;
    const float bpm = processor_.displayBpm.load();
    g.setColour(theme::kTextDim);
    const juce::String clockSummary = "BAR " + juce::String(currentBar, 1) + "/" + juce::String(bars)
        + "  " + juce::String(signature.numerator) + "/" + juce::String(signature.denominator)
        + "  " + juce::String(juce::roundToInt(bpm)) + " BPM";
    g.drawText(clockSummary, getLocalBounds().toFloat().reduced(8.0f, 1.0f).removeFromBottom(13.0f), juce::Justification::centredRight);
}

SweepPanel::SweepPanel(OpenEdmFilterProcessor& processor)
    : processor_(processor),
      clockSync_(processor.parameters, params::kClockSync, "HOST SYNC", theme::kAccentOut),
      timeSig_(processor.parameters, params::kTimeSignature, {}, theme::kAccentOut),
      bars_(processor.parameters, params::kSweepBars, {}, theme::kAccentA),
      barsLabel_({}, "BARS"),
      mode_(processor.parameters, params::kSweepMode, {}, theme::kAccentA),
      target_(processor.parameters, params::kSweepTarget, {}, theme::kAccentA),
      tempo_(processor.parameters, params::kTempoBpm, "Tempo", theme::kAccentOut),
      slopeEditor_(processor),
      cutoffStart_(processor.parameters, params::kSweepCutoffStart, "Start", theme::kAccentA),
      cutoffEnd_(processor.parameters, params::kSweepCutoffEnd, "End", theme::kAccentA),
      cutoffSlope_(processor.parameters, params::kSweepCutoffSlope, "Slope", theme::kAccentA),
      resStart_(processor.parameters, params::kSweepResStart, "Start", theme::kAccentB),
      resEnd_(processor.parameters, params::kSweepResEnd, "End", theme::kAccentB),
      resSlope_(processor.parameters, params::kSweepResSlope, "Slope", theme::kAccentB),
      sweepOn_(*processor.parameters.getParameter(params::kSweepEnabled), [this](float) { refresh(); }),
      sweepBypass_(*processor.parameters.getParameter(params::kSweepBypass), [this](float) { refresh(); }),
      slotWatcher_(*processor.parameters.getParameter(params::kSweepSlot), [this](float slot) {
          selectSlot(static_cast<int>(slot));
      }) {
    const std::array<const char*, 4> effectTitles{ "Start", "End", "Slope", "Onset" };
    for (size_t i = 0; i < effectKnobs_.size(); ++i) {
        const auto colour = i < 4 ? theme::kAccentFx : theme::kAccentB;
        effectKnobs_[i] = std::make_unique<Knob>(processor.parameters, params::kSweepEffectIds[i], effectTitles[i % 4], colour);
        addAndMakeVisible(*effectKnobs_[i]);
    }
    effectKnobs_[3]->setTooltip("Sweep position where the bitcrush starts to move from Start to End.");
    effectKnobs_[7]->setTooltip("Sweep position where the glitch starts to move from Start to End.");
    clockSync_.setTooltip("Follows the host tempo and time signature. Turn off to use TEMPO and the signature menu.");
    timeSig_.setTooltip("Host uses the host time signature while HOST SYNC is on. Without HOST SYNC, Host is 4/4.");
    // The old sweep BYPASS has no button now but still stops the sweep, so turning the sweep on clears it.
    enabled_.setComponentID(params::kSweepEnabled);
    enabled_.setColour(juce::TextButton::buttonOnColourId, theme::kAccentA);
    enabled_.onClick = [this] {
        const bool isRunning = enabled_.getToggleState();
        sweepOn_.setValueAsCompleteGesture(isRunning ? 0.0f : 1.0f);
        if (!isRunning)
            sweepBypass_.setValueAsCompleteGesture(0.0f);
    };
    for (size_t i = 0; i < slotButtons_.size(); ++i) {
        auto& button = slotButtons_[i];
        button.setButtonText("SLOT " + juce::String::charToString(static_cast<juce::juce_wchar>('A' + i)));
        button.setColour(juce::TextButton::buttonOnColourId, theme::kAccentA);
        button.setTooltip("Auto Mode: select and edit this sweep slot. Automate Auto Mode in your DAW.");
        button.onClick = [this, i] { slotWatcher_.setValueAsCompleteGesture(static_cast<float>(i)); };
        addAndMakeVisible(button);
    }
    barsLabel_.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    barsLabel_.setColour(juce::Label::textColourId, theme::kTextDim);
    barsLabel_.setJustificationType(juce::Justification::centredRight);
    slopePresetBox_.setComponentID("sweepPreset");
    slopePresetBox_.setTextWhenNothingSelected("Sweep Preset...");
    openedm::presets::populateComboMenu(slopePresetBox_, openedm::presets::sweepSlopePresets(), false);
    slopePresetBox_.setColour(juce::ComboBox::outlineColourId, theme::kAccentA);
    slopePresetBox_.onChange = [this] {
        const int idx = slopePresetBox_.getSelectedId() - 1;
        const auto& presets = openedm::presets::sweepSlopePresets();
        if (idx >= 0 && idx < static_cast<int>(presets.size())) {
            processor_.editParameters([&] {
                openedm::presets::applyPartial(presets[static_cast<size_t>(idx)], processor_.parameters);
            }, "Sweep preset");
        }
    };

    std::ranges::for_each(std::initializer_list<juce::Component*>{
                              &enabled_, &clockSync_, &timeSig_, &barsLabel_, &bars_, &mode_, &target_, &slopePresetBox_,
                              &tempo_, &slopeEditor_, &cutoffStart_, &cutoffEnd_, &cutoffSlope_,
                              &resStart_, &resEnd_, &resSlope_ },
                          [this](juce::Component* control) { addAndMakeVisible(control); });
    slotWatcher_.sendInitialUpdate();
    refresh();
}

void SweepPanel::refresh() {
    // A parameter attachment runs before the raw values update, so read the parameters themselves.
    const auto isOn = [this](const char* id) { return processor_.parameters.getParameter(id)->getValue() > 0.5f; };
    const bool isRunning = isOn(params::kSweepEnabled) && !isOn(params::kSweepBypass);
    enabled_.setToggleState(isRunning, juce::dontSendNotification);
    // The clock also sets the LFO and step rates. The slope editor dims its own curves.
    const std::array<juce::Component*, 5> alwaysActive{ &enabled_, &clockSync_, &timeSig_, &tempo_, &slopeEditor_ };
    std::ranges::for_each(getChildren(), [&](juce::Component* control) {
        if (std::ranges::find(alwaysActive, control) == alwaysActive.end())
            setInactive(*control, !isRunning, "Turn on AUTO SWEEP to use this control.");
    });

    const bool isHostTempo = isOn(params::kClockSync) && processor_.displayHostTempo.load();
    setInactive(tempo_, isHostTempo, "HOST SYNC uses the host tempo. Turn off HOST SYNC to use TEMPO.");
    const juce::String hostBpm(processor_.displayBpm.load(), 1);
    const auto caption = isHostTempo ? "Host " + hostBpm + " BPM" : juce::String("Clock");
    if (caption != clockCaption_) {
        clockCaption_ = caption;
        repaint(clockArea_);
    }
}

void SweepPanel::selectSlot(int slot) {
    slot = std::clamp(slot, 0, 3);
    slopeEditor_.endDrag();
    auto& state = processor_.parameters;
    bars_.bind(state, params::sweepId(slot, params::kSweepBars));
    mode_.bind(state, params::sweepId(slot, params::kSweepMode));
    target_.bind(state, params::sweepId(slot, params::kSweepTarget));
    const std::array<Knob*, 6> knobs{ &cutoffStart_, &cutoffEnd_, &cutoffSlope_, &resStart_, &resEnd_, &resSlope_ };
    for (size_t i = 0; i < knobs.size(); ++i) knobs[i]->bind(state, params::sweepId(slot, params::kSweepFilterIds[i + 3]));
    for (size_t i = 0; i < effectKnobs_.size(); ++i) effectKnobs_[i]->bind(state, params::sweepId(slot, params::kSweepEffectIds[i]));
    for (size_t i = 0; i < slotButtons_.size(); ++i) slotButtons_[i].setToggleState(static_cast<int>(i) == slot, juce::dontSendNotification);
    slopePresetBox_.setSelectedId(0, juce::dontSendNotification);
    repaint();
}

void SweepPanel::paint(juce::Graphics& g) {
    drawPanel(g, getLocalBounds(), kSweepTitle, theme::kAccentA);
    drawCaption(g, clockArea_, clockCaption_);
    drawCaption(g, editorArea_, "Slope editor (drag start / curve / end)");
    drawCaption(g, cutoffArea_, "Cutoff sweep");
    drawCaption(g, resArea_, "Resonance sweep");
    drawCaption(g, crushArea_, "Bitcrush mix / onset = sweep position");
    drawCaption(g, glitchArea_, "Glitch mix / tempo-synced beat repeat");
}

void SweepPanel::resized() {
    auto bounds = getLocalBounds().reduced(12, 6);
    // The slot buttons share the title row, so each knob row below keeps 62 px.
    auto header = bounds.removeFromTop(40);
    slopePresetBox_.setBounds(header.removeFromRight(138).withSizeKeepingCentre(138, 36));
    header.removeFromRight(5);
    target_.setBounds(header.removeFromRight(88));
    header.removeFromRight(5);
    mode_.setBounds(header.removeFromRight(98));
    header.removeFromRight(5);
    bars_.setBounds(header.removeFromRight(92));
    barsLabel_.setBounds(header.removeFromRight(40));
    const auto titleFont = juce::Font(juce::FontOptions(13.0f, juce::Font::bold));
    header.removeFromLeft(juce::GlyphArrangement::getStringWidthInt(titleFont, juce::String(kSweepTitle).toUpperCase()) + 22);
    enabled_.setBounds(header.removeFromLeft(95).withSizeKeepingCentre(95, 36));
    header.removeFromLeft(5);
    std::ranges::for_each(slotButtons_, [&](juce::TextButton& button) {
        button.setBounds(header.removeFromLeft(64).withSizeKeepingCentre(64, 36));
        header.removeFromLeft(4);
    });

    bounds.removeFromTop(6);
    clockArea_ = bounds.removeFromLeft(90);
    bounds.removeFromLeft(kSectionGap);
    editorArea_ = bounds.removeFromLeft(400);
    bounds.removeFromLeft(kSectionGap);
    auto filters = bounds.removeFromTop((bounds.getHeight() - 8) / 2);
    bounds.removeFromTop(8);
    cutoffArea_ = filters.removeFromLeft((filters.getWidth() - kSectionGap) / 2);
    filters.removeFromLeft(kSectionGap);
    resArea_ = filters;
    crushArea_ = bounds.removeFromLeft((bounds.getWidth() - kSectionGap) / 2);
    bounds.removeFromLeft(kSectionGap);
    glitchArea_ = bounds;

    auto clock = clockArea_.withTrimmedTop(kCaptionHeight);
    clockSync_.setBounds(clock.removeFromTop(36));
    clock.removeFromTop(4);
    timeSig_.setBounds(clock.removeFromTop(36));
    clock.removeFromTop(4);
    tempo_.setBounds(clock);
    slopeEditor_.setBounds(editorArea_.withTrimmedTop(kCaptionHeight));
    layoutRow(cutoffArea_.withTrimmedTop(kCaptionHeight), { &cutoffStart_, &cutoffEnd_, &cutoffSlope_ });
    layoutRow(resArea_.withTrimmedTop(kCaptionHeight), { &resStart_, &resEnd_, &resSlope_ });
    layoutRow(crushArea_.withTrimmedTop(kCaptionHeight), { effectKnobs_[0].get(), effectKnobs_[1].get(), effectKnobs_[2].get(), effectKnobs_[3].get() });
    layoutRow(glitchArea_.withTrimmedTop(kCaptionHeight), { effectKnobs_[4].get(), effectKnobs_[5].get(), effectKnobs_[6].get(), effectKnobs_[7].get() });
}

BandPanel::BandPanel(Apvts& state, int band)
    : title_(band == 0 ? "Filter A" : "Filter B"),
      accent_(band == 0 ? theme::kAccentA : theme::kAccentB),
      bypass_(state, params::bandId(band, params::kBypass), "BYPASS", theme::kBypassEngaged),
      type_(state, band, accent_),
      slope_(state, params::bandId(band, params::kSlope), {}, accent_),
      cutoff_(state, params::bandId(band, params::kCutoff), "Cutoff", accent_),
      resonance_(state, params::bandId(band, params::kResonance), "Resonance", accent_),
      width_(state, params::bandId(band, params::kWidth), "Width", accent_),
      lfoShape_(state, params::bandId(band, params::kLfoShape), "Shape", accent_),
      lfoRate_(state, params::bandId(band, params::kLfoRate), "Rate", accent_),
      lfoDepth_(state, params::bandId(band, params::kLfoDepth), "Depth", accent_),
      envSource_(state, params::bandId(band, params::kEnvSource), "Source", accent_),
      envAttack_(state, params::bandId(band, params::kEnvAttack), "Attack", accent_),
      envRelease_(state, params::bandId(band, params::kEnvRelease), "Release", accent_),
      envDepth_(state, params::bandId(band, params::kEnvDepth), "Depth", accent_),
      envListen_(state, params::bandId(band, params::kEnvInput), "Listen", accent_),
      bypassValue_(*state.getRawParameterValue(params::bandId(band, params::kBypass))) {
    std::ranges::for_each(std::initializer_list<juce::Component*>{
                              &bypass_, &type_, &slope_, &cutoff_, &resonance_, &width_, &lfoShape_, &lfoRate_,
                              &lfoDepth_, &envSource_, &envAttack_, &envRelease_, &envDepth_, &envListen_ },
                          [this](juce::Component* control) { addAndMakeVisible(control); });
    envListen_.setTooltip("Envelope input. Main follows this track. Sidechain follows the other track, or this track when no sidechain is connected.");
    envSource_.setTooltip("The envelope follows the input level around this frequency.");
    refresh(false);
}

void BandPanel::refresh(bool isSweepDriven) {
    const bool isBypassed = bypassValue_.load() > 0.5f;
    std::ranges::for_each(getChildren(), [&](juce::Component* control) {
        if (control == &bypass_)
            return;
        const bool isDriven = isSweepDriven && (control == &cutoff_ || control == &resonance_);
        setInactive(*control, isBypassed || isDriven, isBypassed ? kBandBypassHint : kSweepDrivenHint);
    });
}

void BandPanel::paint(juce::Graphics& g) {
    drawPanel(g, getLocalBounds(), title_, accent_);
    drawCaption(g, filterArea_, "Filter");
    drawCaption(g, lfoArea_, "LFO / tempo sync");
    drawCaption(g, envelopeArea_, "Envelope follower");
}

void BandPanel::resized() {
    const bool compact = getHeight() < 220;
    auto bounds = getLocalBounds().reduced(compact ? 8 : 12, compact ? 4 : 6);
    auto header = bounds.removeFromTop(compact ? 36 : 42);
    slope_.setBounds(header.removeFromRight(76));
    header.removeFromRight(6);
    type_.setBounds(header.removeFromRight(132).withSizeKeepingCentre(132, 36));
    header.removeFromRight(6);
    bypass_.setBounds(header.removeFromRight(68));

    bounds.removeFromTop(compact ? 2 : 6);
    auto topRow = bounds.removeFromTop(bounds.getHeight() / 2);
    filterArea_ = topRow.removeFromLeft(topRow.getWidth() * 3 / 5);
    topRow.removeFromLeft(kSectionGap);
    lfoArea_ = topRow;
    envelopeArea_ = compact ? bounds : bounds.withTrimmedTop(4);

    layoutRow(filterArea_.withTrimmedTop(kCaptionHeight), { &cutoff_, &resonance_, &width_ });
    auto lfoControls = lfoArea_.withTrimmedTop(kCaptionHeight);
    lfoShape_.setBounds(lfoControls.removeFromLeft(86));
    lfoControls.removeFromLeft(3);
    lfoRate_.setBounds(lfoControls.removeFromLeft(66));
    lfoControls.removeFromLeft(3);
    lfoDepth_.setBounds(lfoControls);
    layoutRow(envelopeArea_.withTrimmedTop(kCaptionHeight), { &envListen_, &envSource_, &envAttack_, &envRelease_, &envDepth_ });
}

EffectsPanel::EffectsPanel(OpenEdmFilterProcessor& processor)
    : processor_(processor),
      inputGain_(processor.parameters, params::kInputGain, "Gain", theme::kAccentOut),
      subPreserve_(processor.parameters, params::kSubPreserve, "Sub Keep", theme::kAccentOut),
      drive_(processor.parameters, params::kDrive, "Amount", theme::kAccentFx),
      drivePosition_(processor.parameters, params::kDrivePosition, "Position", theme::kAccentFx),
      analogDrift_(processor.parameters, params::kAnalogDrift, "Drift", theme::kAccentFx),
      crushRate_(processor.parameters, params::kCrushRate, "Rate", theme::kAccentFx),
      crushBits_(processor.parameters, params::kCrushBits, "Bits", theme::kAccentFx),
      crushMix_(processor.parameters, params::kCrushMix, "Mix", theme::kAccentFx),
      crushPosition_(processor.parameters, params::kCrushPosition, "Position", theme::kAccentFx),
      crushLink_(processor.parameters, params::kCrushLink, "LINK A", theme::kAccentFx),
      ringFrequency_(processor.parameters, params::kRingFrequency, "Freq", theme::kAccentFx),
      ringMix_(processor.parameters, params::kRingMix, "Mix", theme::kAccentFx),
      fmRate_(processor.parameters, params::kFmRate, "Rate", theme::kAccentFx),
      fmDepth_(processor.parameters, params::kFmDepth, "Depth", theme::kAccentFx),
      stereoSpread_(processor.parameters, params::kStereoSpread, "Spread", theme::kAccentFx),
      mix_(processor.parameters, params::kMix, "Dry/Wet", theme::kAccentOut),
      output_(processor.parameters, params::kOutput, "Gain", theme::kAccentOut),
      ceiling_(processor.parameters, params::kCeiling, "Ceiling", theme::kAccentOut),
      limiter_(processor.parameters, params::kLimiter, "LIMIT", theme::kAccentOut),
      sections_{ { { "Input", theme::kAccentOut, 2, {} },
                   { "Drive", theme::kAccentFx, 3, {} },
                   { "Crush", theme::kAccentFx, 5, {} },
                   { "Ring Mod", theme::kAccentFx, 2, {} },
                   { "FM & Spread", theme::kAccentFx, 3, {} },
                   { "Output", theme::kAccentOut, 4, {} } } } {
    crushLink_.setTooltip("Make the crush rate follow Filter A's cutoff modulation.");
    limiter_.setTooltip("Limit output peaks to the selected ceiling.");
    analogDrift_.setTooltip("Slowly moves both cutoffs up to 0.14 octave, like analog parts.");
    subPreserve_.setTooltip("Keeps the low end below this frequency dry. 20 Hz turns it off.");
    stereoSpread_.setTooltip("Moves the cutoffs of the two channels up to 1.25 octaves apart.");
    startTimerHz(kRefreshHz);
    std::ranges::for_each(std::initializer_list<juce::Component*>{
                              &inputGain_, &subPreserve_, &drive_, &drivePosition_, &analogDrift_, &crushRate_, &crushBits_, &crushPosition_,
                              &crushMix_,
                              &crushLink_, &ringFrequency_, &ringMix_, &fmRate_, &fmDepth_, &stereoSpread_, &mix_, &output_,
                              &ceiling_, &limiter_ },
                          [this](juce::Component* control) { addAndMakeVisible(control); });
}

void EffectsPanel::timerCallback() {
    const float inPeak = processor_.displayInputPeak.load(std::memory_order_relaxed);
    const float outPeak = processor_.displayOutputPeak.load(std::memory_order_relaxed);
    if (std::abs(inPeak - lastInputPeak_) > 0.002f) {
        lastInputPeak_ = inPeak;
        repaint(sections_[0].area);
    }
    if (std::abs(outPeak - lastOutputPeak_) > 0.002f) {
        lastOutputPeak_ = outPeak;
        repaint(sections_[5].area);
    }
}

void EffectsPanel::paint(juce::Graphics& g) {
    std::ranges::for_each(sections_, [&](const Section& section) {
        drawPanel(g, section.area, section.title, section.accent);
    });
    if (getHeight() >= 120) {
        drawLevelMeter(g, sections_[0].area, processor_.displayInputPeak.load(), theme::kAccentOut);
        drawLevelMeter(g, sections_[5].area, processor_.displayOutputPeak.load(), theme::kAccentOut);
    }
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(theme::kTextDim);
    g.drawText("v" JucePlugin_VersionString, getLocalBounds().reduced(8, 2).removeFromTop(14), juce::Justification::topRight);
}

void EffectsPanel::resized() {
    auto bounds = getLocalBounds();
    const int totalUnits = std::accumulate(sections_.begin(), sections_.end(), 0,
        [](int units, const Section& section) { return units + section.units; });
    const int gaps = kSectionGap * static_cast<int>(sections_.size() - 1);
    const int unitWidth = (bounds.getWidth() - gaps) / totalUnits;
    std::ranges::for_each(sections_, [&](Section& section) {
        section.area = bounds.removeFromLeft(section.units * unitWidth);
        bounds.removeFromLeft(kSectionGap);
    });
    sections_.back().area = sections_.back().area.withRight(getWidth());

    const bool roomForMeter = getHeight() >= 120;
    const auto content = [&](size_t section) {
        auto area = sections_[section].area.reduced(8, 0).withTrimmedTop(juce::jmin(34, juce::jmax(18, sections_[section].area.getHeight() / 4)));
        return area.withTrimmedBottom(4);
    };
    layoutRow(roomForMeter ? content(0).withTrimmedBottom(20) : content(0), { &inputGain_, &subPreserve_ });
    layoutRow(content(1), { &drive_, &drivePosition_, &analogDrift_ });
    layoutRow(content(2), { &crushRate_, &crushBits_, &crushMix_, &crushPosition_, &crushLink_ });
    layoutRow(content(3), { &ringFrequency_, &ringMix_ });
    layoutRow(content(4), { &fmRate_, &fmDepth_, &stereoSpread_ });
    layoutRow(roomForMeter ? content(5).withTrimmedBottom(20) : content(5), { &mix_, &output_, &ceiling_, &limiter_ });
}

} // namespace openedm::ui
