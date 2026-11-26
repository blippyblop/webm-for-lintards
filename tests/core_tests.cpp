// Qt-free unit tests for the WebMConverter-Qt core. Build & run with
//   scripts/build_core_tests.sh    (uses any C++17 compiler; zig works with no root)
#include "../src/core/commandbuilder.h"
#include "../src/core/easing.h"
#include "../src/core/progressparser.h"
#include "../src/core/sizecalc.h"
#include "../src/core/videoinfo.h"

#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

using namespace wc;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) <= (eps))

static bool hasToken(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& t : v) if (t == s) return true;
    return false;
}
static bool hasSubstr(const std::vector<std::string>& v, const std::string& s) {
    for (const auto& t : v) if (t.find(s) != std::string::npos) return true;
    return false;
}
static std::string argAfter(const std::vector<std::string>& v, const std::string& key) {
    for (size_t i = 0; i + 1 < v.size(); ++i)
        if (v[i] == key) return v[i + 1];
    return "";
}

static VideoInfo sampleInfo() {
    VideoInfo i;
    i.path = "/tmp/in.mp4";
    i.durationSeconds = 60.0;
    i.frameRate = Rational{30, 1};
    i.width = 1920;
    i.height = 1080;
    i.pixelFormat = "yuv420p";
    i.hasAudio = true;
    StreamInfo v; v.codecType = "video"; v.codecName = "h264"; v.width = 1920; v.height = 1080;
    StreamInfo a; a.codecType = "audio"; a.codecName = "aac"; a.channels = 2;
    i.streams = {v, a};
    return i;
}

static void testVideoInfoMath() {
    VideoInfo i = sampleInfo();
    CHECK(i.frameAt(2.0) == 60);
    CHECK_NEAR(i.timeOfFrame(60), 2.0, 1e-9);
    CHECK(i.frameCount() == 1800);
    CHECK_NEAR(i.frameRateValue(), 30.0, 1e-9);
}

static void testEasing() {
    std::string e = easingExpression("linear", 0.0, 100.0, 10.0);
    CHECK(e.find("100") != std::string::npos);
    CHECK(e.find("t") != std::string::npos);
    // quadIn should reference squared progress
    std::string q = easingExpression("quadIn", 5.0, 20.0, 4.0);
    CHECK(q.find("5") != std::string::npos);
    CHECK(q.find("20") != std::string::npos);
    // degenerate duration must not divide by zero
    std::string z = easingExpression("linear", 0.0, 1.0, 0.0);
    CHECK(!z.empty());
}

static void testSizeCalc() {
    // 8 MiB target over 60s with 128 kbps audio.
    SizePlan p = planForSize(60.0, 8LL * 1024 * 1024, 128);
    CHECK(p.feasible);
    // expected ~968 kbps video
    CHECK(p.videoKbps > 900 && p.videoKbps < 1000);
    CHECK(p.audioKbps == 128);

    SizePlan bad = planForSize(60.0, 100, 128); // 100 bytes is impossible
    CHECK(!bad.feasible);
}

static void testProgressParser() {
    ProgressParser pp;
    pp.setDuration(60.0);
    bool changed = pp.feed("frame=  300 fps=30 q=0.0 size=  1024kB time=00:00:10.00 bitrate= 838.9kbits/s speed=2.0x");
    CHECK(changed);
    CHECK(pp.last().frame == 300);
    CHECK_NEAR(pp.last().timeSeconds, 10.0, 1e-6);
    CHECK_NEAR(pp.last().percent, (10.0 / 60.0) * 100.0, 1e-3);
    CHECK_NEAR(pp.last().speed, 2.0, 1e-6);

    // Non-progress log lines should not change state.
    CHECK(!pp.feed("Input #0, mov,mp4,m4a from '/tmp/in.mp4':"));
}

static FilterModel emptyFilters() { return FilterModel{}; }

static void testBuilderDefaultVp9() {
    VideoInfo info = sampleInfo();
    FilterModel f = emptyFilters();
    EncodingOptions e;                 // defaults: WebM / VP9 / variable / crf 30
    CommandBuilder b(info, f, e);
    auto passes = b.build("/tmp/in.mp4", "/tmp/out.webm");
    CHECK(passes.size() == 1);
    const auto& a = passes[0].args;
    CHECK(argAfter(a, "-c:v") == "libvpx-vp9");
    CHECK(argAfter(a, "-crf") == "30");
    CHECK(hasToken(a, "-pix_fmt"));
    CHECK(argAfter(a, "-pix_fmt") == "yuv420p");
    CHECK(argAfter(a, "-f") == "webm");
    CHECK(hasToken(a, "-map"));
    CHECK(a.back() == "/tmp/out.webm");
}

static void testBuilderConstantSizeLimit() {
    VideoInfo info = sampleInfo();
    FilterModel f = emptyFilters();
    EncodingOptions e;
    e.rateControl = RateControl::Constant;
    e.sizeLimitBytes = 8LL * 1024 * 1024;
    CommandBuilder b(info, f, e);
    auto passes = b.build("/tmp/in.mp4", "/tmp/out.webm");
    const auto& a = passes[0].args;
    CHECK(hasToken(a, "-fs"));
    CHECK(argAfter(a, "-fs") == std::to_string(8LL * 1024 * 1024));
    // video bitrate should be the computed one (~968k)
    std::string br = argAfter(a, "-b:v");
    CHECK(!br.empty() && br.back() == 'k');
}

static void testBuilderTrimAndCrop() {
    VideoInfo info = sampleInfo();
    FilterModel f = emptyFilters();
    f.trim = TrimRange{5.0, 15.0};
    f.crop = Crop{true, 10, 20, 640, 480};
    EncodingOptions e;
    CommandBuilder b(info, f, e);
    auto passes = b.build("/tmp/in.mp4", "/tmp/out.webm");
    const auto& a = passes[0].args;
    // input seeking
    CHECK(argAfter(a, "-ss") == "5");
    CHECK(argAfter(a, "-t") == "10");
    // crop via filter_complex
    CHECK(hasToken(a, "-filter_complex"));
    CHECK(hasSubstr(a, "crop=640:480:10:20"));
    CHECK(hasToken(a, "[vout]"));
}

static void testBuilderTwoPass() {
    VideoInfo info = sampleInfo();
    FilterModel f = emptyFilters();
    EncodingOptions e;
    e.twoPass = true;
    e.rateControl = RateControl::Constant;
    CommandBuilder b(info, f, e);
    auto passes = b.build("/tmp/in.mp4", "/tmp/out.webm");
    CHECK(passes.size() == 2);
    CHECK(passes[0].isAnalysisPass);
    CHECK(argAfter(passes[0].args, "-pass") == "1");
    CHECK(argAfter(passes[0].args, "-f") == "null");
    CHECK(argAfter(passes[1].args, "-pass") == "2");
}

static void testBuilderMp4H264AudioDisabled() {
    VideoInfo info = sampleInfo();
    FilterModel f = emptyFilters();
    EncodingOptions e;
    e.codec = VideoCodec::H264;
    e.format = OutputFormat::MP4;
    e.disableAudio = true;
    CommandBuilder b(info, f, e);
    auto passes = b.build("/tmp/in.mp4", "/tmp/out.mp4");
    const auto& a = passes[0].args;
    CHECK(argAfter(a, "-c:v") == "libx264");
    CHECK(argAfter(a, "-f") == "mp4");
    CHECK(hasToken(a, "-an"));
}

static void testBuilderAlphaAndHdr() {
    VideoInfo info = sampleInfo();
    FilterModel f = emptyFilters();
    {
        EncodingOptions e; e.useAlpha = true;
        CommandBuilder b(info, f, e);
        auto p = b.build("/tmp/in.mp4", "/tmp/out.webm");
        CHECK(argAfter(p[0].args, "-pix_fmt") == "yuva420p");
    }
    {
        EncodingOptions e; e.hdr = true;
        CommandBuilder b(info, f, e);
        auto p = b.build("/tmp/in.mp4", "/tmp/out.webm");
        CHECK(argAfter(p[0].args, "-pix_fmt") == "yuv420p10le");
        CHECK(hasToken(p[0].args, "-color_primaries"));
    }
}

static void testBuilderResizeRotateRate() {
    VideoInfo info = sampleInfo();
    FilterModel f = emptyFilters();
    f.resizeEnabled = true; f.resizeW = 1280; f.resizeH = 720; f.resizeAlgo = "lanczos";
    f.rotateDegrees = 90;
    f.rateFps = 24;
    EncodingOptions e;
    CommandBuilder b(info, f, e);
    auto p = b.build("/tmp/in.mp4", "/tmp/out.webm");
    const auto& a = p[0].args;
    CHECK(hasSubstr(a, "scale=1280:720:flags=lanczos"));
    CHECK(hasSubstr(a, "transpose=1"));
    CHECK(argAfter(a, "-r") == "24");
}

static void testBuilderDynamicCrop() {
    VideoInfo info = sampleInfo();
    FilterModel f = emptyFilters();
    f.dynamicCrop.enabled = true;
    f.dynamicCrop.w = 640; f.dynamicCrop.h = 480;
    f.dynamicCrop.keyframes = {
        KeyframeCrop{0.0, 0, 0, 640, 480},
        KeyframeCrop{10.0, 800, 300, 640, 480},
    };
    EncodingOptions e;
    CommandBuilder b(info, f, e);
    auto p = b.build("/tmp/in.mp4", "/tmp/out.webm");
    CHECK(hasSubstr(p[0].args, "crop=640:480:x='"));
    CHECK(hasSubstr(p[0].args, "if(lte(t,"));
}

static void testBuilderStabilization() {
    VideoInfo info = sampleInfo();
    FilterModel f = emptyFilters();
    f.stabilization.enabled = true;
    f.stabilization.shakiness = 5;
    f.stabilization.accuracy = 15;
    EncodingOptions e;
    CommandBuilder b(info, f, e);
    auto p = b.build("/tmp/in.mp4", "/tmp/out.webm");
    CHECK(p.size() == 2);                       // detect pass + encode pass
    CHECK(p[0].isAnalysisPass);
    CHECK(hasSubstr(p[0].args, "vidstabdetect="));
    CHECK(hasSubstr(p[1].args, "vidstabtransform=input=transforms.trf"));
}

static void testBuilderGrid() {
    VideoInfo info = sampleInfo();
    FilterModel f = emptyFilters();
    f.grid.enabled = true;
    f.grid.otherFile = "/tmp/two.mp4";
    f.grid.vertical = false;
    EncodingOptions e;
    CommandBuilder b(info, f, e);
    auto p = b.build("/tmp/one.mp4", "/tmp/out.webm");
    const auto& a = p[0].args;
    CHECK(hasToken(a, "hstack"));
    CHECK(hasSubstr(a, "/tmp/two.mp4"));
}

int main() {
    testVideoInfoMath();
    testEasing();
    testSizeCalc();
    testProgressParser();
    testBuilderDefaultVp9();
    testBuilderConstantSizeLimit();
    testBuilderTrimAndCrop();
    testBuilderTwoPass();
    testBuilderMp4H264AudioDisabled();
    testBuilderAlphaAndHdr();
    testBuilderResizeRotateRate();
    testBuilderDynamicCrop();
    testBuilderStabilization();
    testBuilderGrid();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
