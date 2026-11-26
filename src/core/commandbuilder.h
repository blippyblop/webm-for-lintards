#pragma once
// Compiles a FilterModel + EncodingOptions into ffmpeg command lines. This is a
// direct port of the argument templates in the original MainForm.cs, minus the
// AviSynth layer (everything becomes a plain ffmpeg filter graph).
//
// The builder produces argv tokens (std::vector<std::string>) rather than a
// single string so callers can pass them to QProcess argv without re-quoting.

#include "videoinfo.h"
#include "filtermodel.h"

#include <string>
#include <vector>

namespace wc {

struct PassPlan {
    std::vector<std::string> args;   // argv for ffmpeg (program name excluded)
    bool isAnalysisPass = false;     // true for pass 1 of a two-pass encode
};

class CommandBuilder {
public:
    CommandBuilder(const VideoInfo& info, const FilterModel& filters, const EncodingOptions& enc);

    // Build all passes (one normally, two when two-pass is enabled).
    std::vector<PassPlan> build(const std::string& inputPath,
                                const std::string& outputPath) const;

    bool isTwoPass() const;

    // Expected duration of the *output* after trims and (approximate) speed
    // changes. Used for progress percentage and ETA, which ffmpeg reports in
    // output time — NOT source time.
    double outputDuration() const;

    // Exposed pieces (used by tests and the settings/diagnostics UI).
    std::string videoFilterGraph() const;   // contents of -filter_complex, or "" if none
    std::string videoCodecArgs() const;     // space-joined video encoding flags
    std::string audioFilterArgs() const;    // -af chain (or "")
    int effectiveVideoKbps() const;
    int effectiveAudioKbps() const;

private:
    bool hasVideoFilters() const;
    bool needsFilterComplex() const;

    void appendInputArgs(std::vector<std::string>& a, const std::string& inputPath) const;
    void appendVideoArgs(std::vector<std::string>& a) const;
    void appendAudioArgs(std::vector<std::string>& a) const;
    void appendOutputArgs(std::vector<std::string>& a, const std::string& outputPath) const;
    std::vector<std::string> buildGridArgs(const std::string& inputPath,
                                           const std::string& outputPath) const;

    std::string codecName() const;
    std::string containerFormat() const;
    std::string pixelFormat() const;
    std::string audioEncoder() const;
    std::string rateControlArgs() const;
    double clipDuration() const;

    const VideoInfo& info_;
    const FilterModel& f_;
    const EncodingOptions& e_;
};

} // namespace wc
