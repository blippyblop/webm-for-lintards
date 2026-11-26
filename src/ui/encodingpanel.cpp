#include "ui/encodingpanel.h"

#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

namespace wc {

EncodingPanel::EncodingPanel(QWidget* parent) : QWidget(parent) {
    videoTab_ = new QWidget(this);
    audioTab_ = new QWidget(this);
    metadataTab_ = new QWidget(this);

    // --- Output / video ---
    auto* outGroup = new QGroupBox(tr("Encoding"), videoTab_);
    auto* form = new QFormLayout(outGroup);

    format_ = new QComboBox(outGroup);
    format_->addItem(tr("WebM (VP8/VP9)"), static_cast<int>(OutputFormat::WebM));
    format_->addItem(tr("MP4 (H.264/H.265)"), static_cast<int>(OutputFormat::MP4));
    form->addRow(tr("Container"), format_);

    codec_ = new QComboBox(outGroup);
    form->addRow(tr("Video codec"), codec_);

    rateControl_ = new QComboBox(outGroup);
    rateControl_->addItem(tr("Constant bitrate"), static_cast<int>(RateControl::Constant));
    rateControl_->addItem(tr("Variable quality (CRF)"), static_cast<int>(RateControl::Variable));
    form->addRow(tr("Rate control"), rateControl_);

    videoBitrate_ = new QSpinBox(outGroup);
    videoBitrate_->setRange(1, 200000);
    videoBitrate_->setSuffix(tr(" kbps"));
    form->addRow(tr("Video bitrate"), videoBitrate_);

    crf_ = new QSpinBox(outGroup);
    crf_->setRange(0, 63);
    form->addRow(tr("CRF (quality)"), crf_);

    sizeLimitMb_ = new QLineEdit(outGroup);
    sizeLimitMb_->setToolTip(tr("Target output size in MB (0 = no limit). "
                                "Overrides bitrate via two-pass-friendly maths."));
    form->addRow(tr("Size limit (MB)"), sizeLimitMb_);

    twoPass_ = new QCheckBox(tr("Two-pass"), outGroup);
    hq_ = new QCheckBox(tr("HQ mode (alt-ref)"), outGroup);
    alpha_ = new QCheckBox(tr("Alpha (yuva420p)"), outGroup);
    hdr_ = new QCheckBox(tr("HDR (10-bit bt.2020)"), outGroup);
    form->addRow(twoPass_);
    form->addRow(hq_);
    form->addRow(alpha_);
    form->addRow(hdr_);

    threads_ = new QSpinBox(outGroup);
    threads_->setRange(0, 64);
    threads_->setSpecialValueText(tr("auto"));
    form->addRow(tr("Threads"), threads_);

    targetFps_ = new QSpinBox(outGroup);
    targetFps_->setRange(0, 240);
    targetFps_->setSpecialValueText(tr("keep source"));
    form->addRow(tr("Target FPS"), targetFps_);

    // --- Audio ---
    auto* audioGroup = new QGroupBox(tr("Audio"), audioTab_);
    auto* aform = new QFormLayout(audioGroup);

    disableAudio_ = new QCheckBox(tr("No audio"), audioGroup);
    aform->addRow(disableAudio_);

    audioTrack_ = new QComboBox(audioGroup);
    audioTrack_->addItem(tr("Track 0"), 0);
    aform->addRow(tr("Source track"), audioTrack_);

    audioCodec_ = new QComboBox(audioGroup);
    audioCodec_->addItem(tr("Opus"), static_cast<int>(AudioCodec::Opus));
    audioCodec_->addItem(tr("Vorbis"), static_cast<int>(AudioCodec::Vorbis));
    audioCodec_->addItem(tr("AAC"), static_cast<int>(AudioCodec::AAC));
    aform->addRow(tr("Audio codec"), audioCodec_);

    audioBitrate_ = new QSpinBox(audioGroup);
    audioBitrate_->setRange(6, 512);
    audioBitrate_->setSuffix(tr(" kbps"));
    aform->addRow(tr("Audio bitrate"), audioBitrate_);

    normalize_ = new QCheckBox(tr("Normalize (loudnorm)"), audioGroup);
    amplify_ = new QCheckBox(tr("Amplify +3 dB"), audioGroup);
    mono_ = new QCheckBox(tr("Downmix to mono"), audioGroup);
    aform->addRow(normalize_);
    aform->addRow(amplify_);
    aform->addRow(mono_);

    // --- Metadata ---
    auto* metaGroup = new QGroupBox(tr("Metadata"), metadataTab_);
    auto* mform = new QFormLayout(metaGroup);
    title_ = new QLineEdit(metaGroup);
    title_->setPlaceholderText(tr("optional container title"));
    mform->addRow(tr("Title"), title_);

    auto* vl = new QVBoxLayout(videoTab_);
    vl->addWidget(outGroup);
    vl->addStretch(1);
    vl->setContentsMargins(6, 6, 6, 6);

    auto* al = new QVBoxLayout(audioTab_);
    al->addWidget(audioGroup);
    al->addStretch(1);
    al->setContentsMargins(6, 6, 6, 6);

    auto* ml = new QVBoxLayout(metadataTab_);
    ml->addWidget(metaGroup);
    ml->addStretch(1);
    ml->setContentsMargins(6, 6, 6, 6);

    // Defaults.
    EncodingOptions def;
    setOptions(def);

    // Wire change notification.
    auto notify = [this]() { if (!loading_) emit changed(); syncEnabled(); };
    connect(format_, &QComboBox::currentIndexChanged, this, &EncodingPanel::onFormatChanged);
    connect(codec_, &QComboBox::currentIndexChanged, this, notify);
    connect(rateControl_, &QComboBox::currentIndexChanged, this, notify);
    connect(videoBitrate_, &QSpinBox::valueChanged, this, notify);
    connect(crf_, &QSpinBox::valueChanged, this, notify);
    connect(audioBitrate_, &QSpinBox::valueChanged, this, notify);
    connect(audioCodec_, &QComboBox::currentIndexChanged, this, notify);
    connect(audioTrack_, &QComboBox::currentIndexChanged, this, notify);
    connect(threads_, &QSpinBox::valueChanged, this, notify);
    connect(targetFps_, &QSpinBox::valueChanged, this, notify);
    connect(twoPass_, &QCheckBox::toggled, this, notify);
    connect(hq_, &QCheckBox::toggled, this, notify);
    connect(alpha_, &QCheckBox::toggled, this, notify);
    connect(hdr_, &QCheckBox::toggled, this, notify);
    connect(disableAudio_, &QCheckBox::toggled, this, notify);
    connect(normalize_, &QCheckBox::toggled, this, notify);
    connect(amplify_, &QCheckBox::toggled, this, notify);
    connect(mono_, &QCheckBox::toggled, this, notify);
    connect(sizeLimitMb_, &QLineEdit::textChanged, this, [notify](const QString&) { notify(); });
    connect(title_, &QLineEdit::textChanged, this, [notify](const QString&) { notify(); });
}

OutputFormat EncodingPanel::currentFormat() const {
    return static_cast<OutputFormat>(format_->currentData().toInt());
}

void EncodingPanel::rebuildCodecList(OutputFormat fmt) {
    QSignalBlocker b(codec_);
    codec_->clear();
    if (fmt == OutputFormat::WebM) {
        codec_->addItem(tr("VP9"), static_cast<int>(VideoCodec::VP9));
        codec_->addItem(tr("VP8"), static_cast<int>(VideoCodec::VP8));
    } else {
        codec_->addItem(tr("H.264"), static_cast<int>(VideoCodec::H264));
        codec_->addItem(tr("H.265"), static_cast<int>(VideoCodec::H265));
    }
}

void EncodingPanel::onFormatChanged() {
    rebuildCodecList(currentFormat());
    syncEnabled();
    if (!loading_) emit changed();
}

void EncodingPanel::syncEnabled() {
    const bool constant = rateControl_->currentData().toInt() == static_cast<int>(RateControl::Constant);
    videoBitrate_->setEnabled(constant);
    crf_->setEnabled(!constant);
    const bool noAudio = disableAudio_->isChecked();
    audioTrack_->setEnabled(!noAudio && audioTrack_->count() > 0 &&
                            audioTrack_->currentData().toInt() >= 0);
    audioCodec_->setEnabled(!noAudio);
    audioBitrate_->setEnabled(!noAudio);
    normalize_->setEnabled(!noAudio);
    amplify_->setEnabled(!noAudio);
    mono_->setEnabled(!noAudio);
}

void EncodingPanel::setOptions(const EncodingOptions& o) {
    loading_ = true;

    int fi = format_->findData(static_cast<int>(o.format));
    format_->setCurrentIndex(fi < 0 ? 0 : fi);
    rebuildCodecList(currentFormat());

    int ci = codec_->findData(static_cast<int>(o.codec));
    if (ci < 0) ci = 0;
    codec_->setCurrentIndex(ci);

    int ri = rateControl_->findData(static_cast<int>(o.rateControl));
    rateControl_->setCurrentIndex(ri < 0 ? 1 : ri);

    videoBitrate_->setValue(o.videoBitrateKbps);
    crf_->setValue(o.crf);
    twoPass_->setChecked(o.twoPass);
    hq_->setChecked(o.hq);
    alpha_->setChecked(o.useAlpha);
    hdr_->setChecked(o.hdr);
    threads_->setValue(o.threads);
    targetFps_->setValue(o.targetFps);

    disableAudio_->setChecked(o.disableAudio);
    int ai = audioCodec_->findData(static_cast<int>(o.audioCodec));
    audioCodec_->setCurrentIndex(ai < 0 ? 0 : ai);
    int ti = audioTrack_->findData(o.audioStreamIndex);
    if (ti >= 0) audioTrack_->setCurrentIndex(ti);
    audioBitrate_->setValue(o.audioBitrateKbps);
    normalize_->setChecked(o.normalizeAudio);
    amplify_->setChecked(o.amplifyAudio);
    mono_->setChecked(o.monoAudio);

    if (o.sizeLimitBytes > 0)
        sizeLimitMb_->setText(QString::number(o.sizeLimitBytes / 1024.0 / 1024.0, 'f', 1));
    else
        sizeLimitMb_->setText(QStringLiteral("0"));
    title_->setText(QString::fromStdString(o.title));

    loading_ = false;
    syncEnabled();
}

EncodingOptions EncodingPanel::options() const {
    EncodingOptions o;
    o.format = currentFormat();
    o.codec = static_cast<VideoCodec>(codec_->currentData().toInt());
    o.rateControl = static_cast<RateControl>(rateControl_->currentData().toInt());
    o.videoBitrateKbps = videoBitrate_->value();
    o.crf = crf_->value();
    o.twoPass = twoPass_->isChecked();
    o.hq = hq_->isChecked();
    o.useAlpha = alpha_->isChecked();
    o.hdr = hdr_->isChecked();
    o.threads = threads_->value();
    o.targetFps = targetFps_->value();

    o.disableAudio = disableAudio_->isChecked();
    o.audioStreamIndex = audioTrack_->currentData().toInt();
    o.audioCodec = static_cast<AudioCodec>(audioCodec_->currentData().toInt());
    o.audioBitrateKbps = audioBitrate_->value();
    o.normalizeAudio = normalize_->isChecked();
    o.amplifyAudio = amplify_->isChecked();
    o.monoAudio = mono_->isChecked();

    bool ok = false;
    const double mb = sizeLimitMb_->text().toDouble(&ok);
    o.sizeLimitBytes = (ok && mb > 0.0) ? static_cast<long long>(mb * 1024.0 * 1024.0) : 0;
    o.title = title_->text().toStdString();
    return o;
}

void EncodingPanel::setVideoInfo(const VideoInfo& info) {
    loading_ = true;
    const int prev = audioTrack_->count() > 0 ? audioTrack_->currentData().toInt() : 0;
    audioTrack_->clear();
    int ord = 0;
    for (const auto& s : info.streams) {
        if (s.codecType != "audio") continue;
        QString label = tr("Track %1").arg(ord);
        if (!s.codecName.empty()) label += QStringLiteral(" — ") + QString::fromStdString(s.codecName);
        if (!s.language.empty())
            label += QStringLiteral(" [") + QString::fromStdString(s.language) + QLatin1Char(']');
        audioTrack_->addItem(label, ord);
        ++ord;
    }
    if (audioTrack_->count() == 0)
        audioTrack_->addItem(tr("No audio stream"), -1);
    int idx = audioTrack_->findData(prev >= 0 ? prev : 0);
    if (idx < 0) idx = 0;
    audioTrack_->setCurrentIndex(idx);
    loading_ = false;
    syncEnabled();
}

QWidget* EncodingPanel::videoTab() { return videoTab_; }
QWidget* EncodingPanel::audioTab() { return audioTab_; }
QWidget* EncodingPanel::metadataTab() { return metadataTab_; }

} // namespace wc
