#pragma once
// Edits EncodingOptions: container, codec, rate control, quality/size, audio.

#include <QWidget>
#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QComboBox;
class QSpinBox;
class QCheckBox;
class QLineEdit;

namespace wc {

class EncodingPanel : public QWidget {
    Q_OBJECT
public:
    explicit EncodingPanel(QWidget* parent = nullptr);

    void setOptions(const EncodingOptions& options);
    EncodingOptions options() const;

    // Populates the "source track" selector from the loaded media.
    void setVideoInfo(const VideoInfo& info);

    // The three sub-panels are exposed so the host window can place them as
    // flat tabs (Encoding / Audio / Metadata) alongside the Filters tab.
    QWidget* videoTab();
    QWidget* audioTab();
    QWidget* metadataTab();

signals:
    void changed();

private slots:
    void onFormatChanged();
    void syncEnabled();

private:
    void rebuildCodecList(OutputFormat fmt);
    OutputFormat currentFormat() const;

    QComboBox* format_ = nullptr;
    QComboBox* codec_ = nullptr;
    QComboBox* rateControl_ = nullptr;
    QSpinBox* videoBitrate_ = nullptr;
    QSpinBox* crf_ = nullptr;
    QSpinBox* audioBitrate_ = nullptr;
    QComboBox* audioCodec_ = nullptr;
    QComboBox* audioTrack_ = nullptr;
    QCheckBox* twoPass_ = nullptr;
    QCheckBox* hq_ = nullptr;
    QCheckBox* alpha_ = nullptr;
    QCheckBox* hdr_ = nullptr;
    QSpinBox* threads_ = nullptr;
    QSpinBox* targetFps_ = nullptr;
    QCheckBox* disableAudio_ = nullptr;
    QCheckBox* normalize_ = nullptr;
    QCheckBox* amplify_ = nullptr;
    QCheckBox* mono_ = nullptr;
    QLineEdit* sizeLimitMb_ = nullptr;
    QLineEdit* title_ = nullptr;

    QWidget* videoTab_ = nullptr;
    QWidget* audioTab_ = nullptr;
    QWidget* metadataTab_ = nullptr;

    bool loading_ = false;
};

} // namespace wc
