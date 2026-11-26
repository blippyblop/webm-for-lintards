#pragma once
// Main window: Convert / Download / Merge / Log tabs, preview + timeline,
// filter & encoding panels, applied-filters panel and conversion console.

#include <QMainWindow>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QRect>
#include <QElapsedTimer>

#include "core/videoinfo.h"
#include "core/filtermodel.h"
#include "ui/filterpanel.h"

class QTabWidget;
class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QListWidget;
class QRadioButton;
class QProgressBar;
class QPlainTextEdit;

namespace wc {

class PreviewCanvas;
class TimelineWidget;
class ConsolePanel;
class EncodingPanel;
class AppliedFiltersPanel;
class PreviewEngine;
class Encoder;
class YtdlpRunner;
class MediaProbe;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void closeEvent(QCloseEvent* e) override;

private slots:
    void openFile();
    void onFilterClicked(FilterKind kind);
    void onDeleteFilter(FilterKind kind);
    void onLoopToggled(bool on);
    void onReverseToggled(bool on);
    void onSeek(double seconds);
    void onFrameStep(int delta);
    void onCropChanged(const QRect& rect);
    void onEncodeClicked();
    void onCancelClicked();
    void onPreviewFrameReady(int frame, const QImage& image);
    void onPreviewError(const QString& message);
    void onEncodeProgress(qint64 frame, double timeSeconds, double percent, double fps, double speed);
    void onEncodeFinished(bool success, const QString& error);
    void onDownloadClicked();
    void onDownloadProgress(double percent, const QString& speed, const QString& eta);
    void onDownloadFinished(bool success, const QString& outputPath, const QString& error);
    void onAddMergeFiles();
    void onStartMerge();

private:
    void buildUi();
    void buildMenus();
    void loadFile(const QString& path);
    void resetFiltersForNewFile();
    void requestPreview(int frame);
    void updateCropOverlay();
    void syncTimelineTrim();
    void refreshAppliedFilters();
    void appendLog(const QString& line);
    QString defaultOutputPath() const;
    QString formatSelectorForCombo() const;
    QString formatHms(double seconds) const;
    void loadSettings();
    void saveSettings();
    void persistFolders();
    void showError(const QString& title, const QString& message);
    void startNextConvert();

    // State
    VideoInfo info_;
    FilterModel filters_;
    EncodingOptions encoding_;
    QString inputPath_;
    QString outputPath_;
    int currentFrame_ = 0;
    bool convertRunning_ = false;
    QStringList convertQueue_;

    double encodeDuration_ = 0.0;
    QElapsedTimer progressClock_;
    qint64 lastProgressMs_ = 0;

    QString inputDir_;
    QString outputDir_;

    QString ffmpegPath_ = QStringLiteral("ffmpeg");
    QString ffprobePath_ = QStringLiteral("ffprobe");
    QString ytdlpPath_ = QStringLiteral("yt-dlp");

    // Widgets
    QTabWidget* tabs_ = nullptr;
    PreviewCanvas* canvas_ = nullptr;
    TimelineWidget* timeline_ = nullptr;
    FilterPanel* filterPanel_ = nullptr;
    EncodingPanel* encodingPanel_ = nullptr;
    AppliedFiltersPanel* applied_ = nullptr;
    ConsolePanel* console_ = nullptr;
    QProgressBar* convertProgress_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QLineEdit* inputEdit_ = nullptr;
    QLineEdit* outputEdit_ = nullptr;
    QPushButton* convertBtn_ = nullptr;

    // Download tab
    QLineEdit* urlEdit_ = nullptr;
    QLineEdit* dlDirEdit_ = nullptr;
    QComboBox* dlFormat_ = nullptr;
    QProgressBar* dlProgress_ = nullptr;
    QPlainTextEdit* dlLog_ = nullptr;

    // Merge tab
    QListWidget* mergeList_ = nullptr;
    QRadioButton* mergeMode_ = nullptr;
    QRadioButton* convertMode_ = nullptr;

    // Engines
    PreviewEngine* preview_ = nullptr;
    Encoder* encoder_ = nullptr;
    YtdlpRunner* ytdlp_ = nullptr;
    MediaProbe* probe_ = nullptr;
};

} // namespace wc
