#include "core/mediaprobe.h"

#include "core/videoinfo.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <cmath>
#include <cstdint>
#include <string>

namespace wc {

namespace {

// ---------------------------------------------------------------------------
// Small JSON helpers. ffprobe is not fully consistent: numbers are sometimes
// emitted as JSON numbers, sometimes as strings ("duration": "12.345000"), and
// most keys are simply absent for streams that do not have them. Every lookup
// below therefore has to be defensive.
// ---------------------------------------------------------------------------

// QString rendering of a scalar JSON value; empty string for null/undefined.
QString qstr(const QJsonValue& v) {
    if (v.isString()) return v.toString();
    if (v.isBool()) return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (std::isfinite(d) && d == std::floor(d) && std::fabs(d) < 1e15)
            return QString::number(static_cast<qint64>(d));
        return QString::number(d, 'g', 12);
    }
    return QString();
}

// Accepts both JSON numbers and numeric strings ("12.345000").
double toDouble(const QJsonValue& v, double def = 0.0) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        return std::isfinite(d) ? d : def;
    }
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().trimmed().toDouble(&ok);
        return ok ? d : def;
    }
    return def;
}

// Accepts JSON numbers and numeric strings.
int toInt(const QJsonValue& v, int def = 0) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (!std::isfinite(d)) return def;
        return static_cast<int>(d); // truncation matches ffprobe's integral fields
    }
    if (v.isString()) {
        bool ok = false;
        const int i = v.toString().trimmed().toInt(&ok);
        return ok ? i : def;
    }
    return def;
}

// Case tolerant tag lookup: Matroska tooling writes "LANGUAGE"/"TITLE" while
// other muxers write "language"/"title".
QString tagValue(const QJsonObject& tags, const QString& key) {
    const QJsonValue direct = tags.value(key);
    if (!direct.isUndefined() && !direct.isNull()) return qstr(direct);
    const QStringList keys = tags.keys();
    for (const QString& candidate : keys) {
        if (candidate.compare(key, Qt::CaseInsensitive) == 0) return qstr(tags.value(candidate));
    }
    return QString();
}

// Parses a rational rate ("30000/1001", "25", "25/1"). Returns {0,1} when the
// value is missing or nonsensical so callers can fall back to another field.
Rational parseFrameRate(const QJsonValue& v) {
    Rational r; // default {0, 1}
    QString s;
    if (v.isString())
        s = v.toString().trimmed();
    else if (v.isDouble())
        s = QString::number(v.toDouble(), 'g', 12);
    else
        return r;
    if (s.isEmpty()) return r;

    const auto slash = s.indexOf(QLatin1Char('/'));
    bool ok = false;
    if (slash >= 0) {
        const int num = s.left(slash).trimmed().toInt(&ok);
        if (!ok) return r;
        const int den = s.mid(slash + 1).trimmed().toInt(&ok);
        if (!ok || num == 0 || den == 0) return r;
        r.num = num;
        r.den = den;
        return r;
    }

    const int num = s.toInt(&ok);
    if (!ok || num == 0) return r;
    r.num = num;
    r.den = 1;
    return r;
}

bool isValidRate(const Rational& r) { return r.num != 0 && r.den != 0; }

// Frame rate for a video stream: r_frame_rate first, avg_frame_rate as fallback.
Rational streamFrameRate(const QJsonObject& stream) {
    Rational r = parseFrameRate(stream.value(QStringLiteral("r_frame_rate")));
    if (!isValidRate(r)) r = parseFrameRate(stream.value(QStringLiteral("avg_frame_rate")));
    return r;
}

// One-line, length-limited excerpt of ffprobe stderr for error messages.
QString stderrSnippet(const QByteArray& raw) {
    QString s = QString::fromLocal8Bit(raw).trimmed();
    if (s.isEmpty()) return QStringLiteral("<no stderr; -v quiet suppresses diagnostics>");
    s.replace(QLatin1Char('\n'), QLatin1Char(' '));
    s.replace(QLatin1Char('\r'), QLatin1Char(' '));
    s = s.simplified();
    const qsizetype kMaxLen = 400;
    if (s.size() > kMaxLen) s = s.left(kMaxLen) + QStringLiteral("...");
    return s;
}

} // namespace

MediaProbe::MediaProbe(QString ffprobePath)
    : ffprobePath_(ffprobePath.isEmpty() ? QStringLiteral("ffprobe") : ffprobePath) {}

bool MediaProbe::probe(const QString& filePath, VideoInfo& info, QString* error) const {
    if (error) error->clear();

    auto fail = [error](const QString& message) {
        if (error) *error = message;
        return false;
    };

    if (filePath.isEmpty()) return fail(QStringLiteral("Cannot probe media: empty file path."));

    const QString program = ffprobePath_.isEmpty() ? QStringLiteral("ffprobe") : ffprobePath_;

    QStringList args;
    args << QStringLiteral("-v") << QStringLiteral("quiet")
         << QStringLiteral("-print_format") << QStringLiteral("json")
         << QStringLiteral("-show_format") << QStringLiteral("-show_streams")
         << filePath;

    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);

    process.start(program, args);
    if (!process.waitForStarted(5000)) {
        return fail(QStringLiteral("Failed to start ffprobe \"%1\": %2")
                        .arg(program, process.errorString()));
    }

    if (!process.waitForFinished(20000)) {
        // Never leave the child running behind our back.
        process.kill();
        process.waitForFinished(2000);
        return fail(QStringLiteral("ffprobe timed out after 20 s while probing \"%1\": %2")
                        .arg(filePath, process.errorString()));
    }

    const QByteArray stdoutData = process.readAllStandardOutput();
    const QByteArray stderrData = process.readAllStandardError();

    if (process.exitStatus() != QProcess::NormalExit) {
        return fail(QStringLiteral("ffprobe crashed while probing \"%1\" (exit status: %2): %3")
                        .arg(filePath, QString::number(static_cast<int>(process.exitStatus())),
                             stderrSnippet(stderrData)));
    }
    if (process.exitCode() != 0) {
        return fail(QStringLiteral("ffprobe failed on \"%1\" with exit code %2: %3")
                        .arg(filePath, QString::number(process.exitCode()),
                             stderrSnippet(stderrData)));
    }
    if (stdoutData.trimmed().isEmpty()) {
        return fail(QStringLiteral("ffprobe produced no output for \"%1\": %2")
                        .arg(filePath, stderrSnippet(stderrData)));
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(stdoutData, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        const QString detail = (parseError.error != QJsonParseError::NoError)
                                   ? parseError.errorString()
                                   : QStringLiteral("top-level JSON value is not an object");
        return fail(QStringLiteral("Invalid ffprobe output for \"%1\": %2. stderr: %3")
                        .arg(filePath, detail, stderrSnippet(stderrData)));
    }

    const QJsonObject root = doc.object();
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();
    const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();

    if (format.isEmpty() && streams.isEmpty()) {
        return fail(QStringLiteral("ffprobe returned no format or stream information for \"%1\": %2")
                        .arg(filePath, stderrSnippet(stderrData)));
    }

    VideoInfo parsed;
    parsed.path = filePath.toStdString();

    // First pass: build StreamInfo for every stream and remember the first
    // video stream (the primary one that describes the picture).
    QJsonObject primaryVideo;
    bool havePrimaryVideo = false;

    const int streamCount = static_cast<int>(streams.size());
    for (int i = 0; i < streamCount; ++i) {
        const QJsonValue streamValue = streams.at(i);
        if (!streamValue.isObject()) continue;
        const QJsonObject stream = streamValue.toObject();

        const QString codecType = qstr(stream.value(QStringLiteral("codec_type")));
        const QJsonObject tags = stream.value(QStringLiteral("tags")).toObject();

        StreamInfo si;
        si.index = toInt(stream.value(QStringLiteral("index")), -1);
        si.codecName = qstr(stream.value(QStringLiteral("codec_name"))).toStdString();
        si.codecType = codecType.toStdString();
        si.language = tagValue(tags, QStringLiteral("language")).toStdString();
        si.title = tagValue(tags, QStringLiteral("title")).toStdString();
        si.durationSeconds = toDouble(stream.value(QStringLiteral("duration")), 0.0);

        if (codecType == QLatin1String("video")) {
            si.width = toInt(stream.value(QStringLiteral("width")), 0);
            si.height = toInt(stream.value(QStringLiteral("height")), 0);
            si.frameRate = streamFrameRate(stream);
            if (!havePrimaryVideo) {
                primaryVideo = stream;
                havePrimaryVideo = true;
            }
        } else if (codecType == QLatin1String("audio")) {
            si.sampleRate = toInt(stream.value(QStringLiteral("sample_rate")), 0);
            si.channels = toInt(stream.value(QStringLiteral("channels")), 0);
            parsed.hasAudio = true;
        }

        parsed.streams.push_back(si);
    }

    // Container duration wins; some containers (and raw streams) only expose it
    // per stream, so fall back to the primary video stream before giving up.
    double duration = toDouble(format.value(QStringLiteral("duration")), 0.0);
    if (duration <= 0.0 && havePrimaryVideo)
        duration = toDouble(primaryVideo.value(QStringLiteral("duration")), 0.0);
    parsed.durationSeconds = (duration > 0.0 && std::isfinite(duration)) ? duration : 0.0;

    if (havePrimaryVideo) {
        parsed.width = toInt(primaryVideo.value(QStringLiteral("width")), 0);
        parsed.height = toInt(primaryVideo.value(QStringLiteral("height")), 0);
        parsed.pixelFormat = qstr(primaryVideo.value(QStringLiteral("pix_fmt"))).toStdString();
        parsed.colorRange = qstr(primaryVideo.value(QStringLiteral("color_range"))).toStdString();
        parsed.colorPrimaries =
            qstr(primaryVideo.value(QStringLiteral("color_primaries"))).toStdString();
        parsed.colorTransfer =
            qstr(primaryVideo.value(QStringLiteral("color_transfer"))).toStdString();
        parsed.frameRate = streamFrameRate(primaryVideo);

        const QString fieldOrder =
            qstr(primaryVideo.value(QStringLiteral("field_order"))).trimmed().toLower();
        parsed.interlaced = !fieldOrder.isEmpty() &&
                            fieldOrder != QLatin1String("progressive") &&
                            fieldOrder != QLatin1String("unknown");
    }

    info = parsed;
    return true;
}

} // namespace wc
