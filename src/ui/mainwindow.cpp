#include "ui/mainwindow.h"

#include "ui/previewcanvas.h"
#include "ui/timelinewidget.h"
#include "ui/consolepanel.h"
#include "ui/encodingpanel.h"
#include "ui/filterpanel.h"

#include "ui/dialogs/trimdialog.h"
#include "ui/dialogs/multitrimdialog.h"
#include "ui/dialogs/cropdialog.h"
#include "ui/dialogs/resizedialog.h"
#include "ui/dialogs/rotatedialog.h"
#include "ui/dialogs/ratedialog.h"
#include "ui/dialogs/dynamicratedialog.h"
#include "ui/dialogs/captiondialog.h"
#include "ui/dialogs/subtitledialog.h"
#include "ui/dialogs/overlaydialog.h"
#include "ui/dialogs/fadedialog.h"
#include "ui/dialogs/eqdialog.h"
#include "ui/dialogs/denoisedialog.h"
#include "ui/dialogs/deinterlacedialog.h"
#include "ui/dialogs/stabilizationdialog.h"
#include "ui/dialogs/delayaudiodialog.h"

#include "core/commandbuilder.h"
#include "core/mediaprobe.h"
#include "core/encoder.h"
#include "core/ytdlprunner.h"
#include "preview/previewengine.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QListWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QSettings>
#include <QStandardPaths>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QDialog>

#include <algorithm>

namespace wc {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    probe_ = new MediaProbe(ffprobePath_);
    preview_ = new PreviewEngine(this);
    encoder_ = new Encoder(this);
    ytdlp_ = new YtdlpRunner(this);

    buildUi();
    buildMenus();

    connect(encoder_, &Encoder::logLine, this, [this](const QString& line) {
        const QString t = line.trimmed();
        if (!t.isEmpty() && !t.startsWith("frame=")) appendLog(line);
    });
    connect(encoder_, &Encoder::progress, this, &MainWindow::onEncodeProgress);
    connect(encoder_, &Encoder::finished, this, &MainWindow::onEncodeFinished);

    connect(preview_, &PreviewEngine::frameReady, this, &MainWindow::onPreviewFrameReady);
    connect(preview_, &PreviewEngine::error, this, &MainWindow::onPreviewError);

    connect(ytdlp_, &YtdlpRunner::logLine, this, [this](const QString& line) {
        if (dlLog_ && !line.trimmed().isEmpty()) dlLog_->appendPlainText(line);
    });
    connect(ytdlp_, &YtdlpRunner::progress, this, &MainWindow::onDownloadProgress);
    connect(ytdlp_, &YtdlpRunner::finished, this, &MainWindow::onDownloadFinished);

    loadSettings();

    setAcceptDrops(true);
    resize(1200, 780);
    setWindowTitle(tr("WebM for Lazys (Qt)"));
}

MainWindow::~MainWindow() {
    if (preview_) preview_->cancel();
    if (encoder_) encoder_->cancel();
    delete probe_;
}

void MainWindow::buildUi() {
    tabs_ = new QTabWidget(this);
    setCentralWidget(tabs_);
    statusLabel_ = new QLabel(tr("Ready"), this);
    statusBar()->addWidget(statusLabel_, 1);

    // ---------------- Convert tab ----------------
    auto* convert = new QWidget(tabs_);
    auto* cv = new QVBoxLayout(convert);

    auto* hsplit = new QSplitter(Qt::Horizontal, convert);

    auto* left = new QWidget(hsplit);
    auto* lv = new QVBoxLayout(left);
    lv->setContentsMargins(0, 0, 0, 0);
    canvas_ = new PreviewCanvas(left);
    timeline_ = new TimelineWidget(left);
    lv->addWidget(canvas_, 1);
    lv->addWidget(timeline_);

    auto* outRow = new QHBoxLayout;
    outRow->addWidget(new QLabel(tr("Output:"), left));
    outputEdit_ = new QLineEdit(left);
    outRow->addWidget(outputEdit_, 1);
    auto* browseOut = new QPushButton(tr("Browse…"), left);
    outRow->addWidget(browseOut);
    convertBtn_ = new QPushButton(tr("Convert"), left);
    convertBtn_->setDefault(true);
    outRow->addWidget(convertBtn_);
    lv->addLayout(outRow);

    auto* rightScroll = new QScrollArea(hsplit);
    rightScroll->setWidgetResizable(true);
    auto* right = new QWidget(rightScroll);
    auto* rv = new QVBoxLayout(right);
    filterPanel_ = new FilterPanel(right);
    encodingPanel_ = new EncodingPanel(right);
    rv->addWidget(filterPanel_);
    rv->addWidget(encodingPanel_);
    rv->addStretch(1);
    rightScroll->setWidget(right);
    rightScroll->setMinimumWidth(340);
    rightScroll->setMaximumWidth(480);

    hsplit->addWidget(left);
    hsplit->addWidget(rightScroll);
    hsplit->setStretchFactor(0, 1);
    hsplit->setStretchFactor(1, 0);

    auto* vsplit = new QSplitter(Qt::Vertical, convert);
    vsplit->addWidget(hsplit);
    console_ = new ConsolePanel(vsplit);
    vsplit->addWidget(console_);
    vsplit->setStretchFactor(0, 3);
    vsplit->setStretchFactor(1, 1);
    vsplit->setSizes({540, 200});

    cv->addWidget(vsplit);
    tabs_->addTab(convert, tr("Convert"));

    // ---------------- Download tab ----------------
    auto* dl = new QWidget(tabs_);
    auto* dv = new QVBoxLayout(dl);
    auto* dlForm = new QFormLayout;

    urlEdit_ = new QLineEdit(dl);
    urlEdit_->setPlaceholderText(tr("https://…   (append @*start-end to clip, e.g. @*5:35-5:45)"));
    dlDirEdit_ = new QLineEdit(dl);
    auto* dlBrowse = new QPushButton(tr("Browse…"), dl);
    auto* dirRow = new QHBoxLayout;
    dirRow->addWidget(dlDirEdit_, 1);
    dirRow->addWidget(dlBrowse);

    dlFormat_ = new QComboBox(dl);
    dlFormat_->addItem(tr("Best quality"), QStringLiteral("best"));
    dlFormat_->addItem(tr("≤1080p"), QStringLiteral("bestvideo[height<=1080]+bestaudio/best"));
    dlFormat_->addItem(tr("≤720p"), QStringLiteral("bestvideo[height<=720]+bestaudio/best"));
    dlFormat_->addItem(tr("≤480p"), QStringLiteral("bestvideo[height<=480]+bestaudio/best"));
    dlFormat_->addItem(tr("Audio only"), QStringLiteral("bestaudio/best"));

    dlForm->addRow(tr("URL"), urlEdit_);
    dlForm->addRow(tr("Save to"), dirRow);
    dlForm->addRow(tr("Format"), dlFormat_);
    dv->addLayout(dlForm);

    auto* dlBtn = new QPushButton(tr("Download"), dl);
    dv->addWidget(dlBtn);
    dlProgress_ = new QProgressBar(dl);
    dlProgress_->setRange(0, 100);
    dv->addWidget(dlProgress_);
    dlLog_ = new QPlainTextEdit(dl);
    dlLog_->setReadOnly(true);
    dlLog_->setMaximumBlockCount(2000);
    dv->addWidget(dlLog_, 1);
    tabs_->addTab(dl, tr("Download"));

    // ---------------- Merge tab ----------------
    auto* mg = new QWidget(tabs_);
    auto* mv = new QVBoxLayout(mg);
    mergeList_ = new QListWidget(mg);
    mv->addWidget(mergeList_, 1);
    auto* mgRow = new QHBoxLayout;
    auto* addFiles = new QPushButton(tr("Add files…"), mg);
    auto* removeFile = new QPushButton(tr("Remove"), mg);
    mgRow->addWidget(addFiles);
    mgRow->addWidget(removeFile);
    mgRow->addStretch(1);
    mv->addLayout(mgRow);

    auto* modeGroup = new QGroupBox(tr("Action"), mg);
    auto* mgl = new QVBoxLayout(modeGroup);
    mergeMode_ = new QRadioButton(tr("Merge two files with grid (hstack)"), modeGroup);
    convertMode_ = new QRadioButton(tr("Convert each file with current settings"), modeGroup);
    mergeMode_->setChecked(true);
    mgl->addWidget(mergeMode_);
    mgl->addWidget(convertMode_);
    mv->addWidget(modeGroup);

    auto* startMerge = new QPushButton(tr("Start"), mg);
    mv->addWidget(startMerge);
    tabs_->addTab(mg, tr("Merge / Convert"));

    // ---------------- signal wiring ----------------
    connect(convertBtn_, &QPushButton::clicked, this, &MainWindow::onEncodeClicked);
    connect(browseOut, &QPushButton::clicked, this, [this]() {
        const QString f = QFileDialog::getSaveFileName(
            this, tr("Save output"), outputEdit_->text(), tr("Video (*.webm *.mp4);;All files (*)"));
        if (!f.isEmpty()) outputEdit_->setText(f);
    });
    connect(filterPanel_, &FilterPanel::filterClicked, this, &MainWindow::onFilterClicked);
    connect(filterPanel_, &FilterPanel::loopToggled, this, &MainWindow::onLoopToggled);
    connect(filterPanel_, &FilterPanel::reverseToggled, this, &MainWindow::onReverseToggled);
    connect(timeline_, &TimelineWidget::seekRequested, this, &MainWindow::onSeek);
    connect(timeline_, &TimelineWidget::frameStepRequested, this, &MainWindow::onFrameStep);
    connect(canvas_, &PreviewCanvas::cropChanged, this, &MainWindow::onCropChanged);
    connect(console_, &ConsolePanel::cancelRequested, this, &MainWindow::onCancelClicked);
    connect(encodingPanel_, &EncodingPanel::changed, this, [this]() {
        encoding_ = encodingPanel_->options();
    });

    connect(dlBrowse, &QPushButton::clicked, this, [this]() {
        const QString d = QFileDialog::getExistingDirectory(this, tr("Download folder"), dlDirEdit_->text());
        if (!d.isEmpty()) dlDirEdit_->setText(d);
    });
    connect(dlBtn, &QPushButton::clicked, this, &MainWindow::onDownloadClicked);

    connect(addFiles, &QPushButton::clicked, this, &MainWindow::onAddMergeFiles);
    connect(removeFile, &QPushButton::clicked, this, [this]() {
        const int row = mergeList_->currentRow();
        if (row >= 0) delete mergeList_->takeItem(row);
    });
    connect(startMerge, &QPushButton::clicked, this, &MainWindow::onStartMerge);

    encoding_ = encodingPanel_->options();
}

void MainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* openAct = new QAction(tr("&Open…"), this);
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::openFile);
    fileMenu->addAction(openAct);

    auto* quitAct = new QAction(tr("&Quit"), this);
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAct);

    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
    auto* aboutAct = new QAction(tr("&About"), this);
    connect(aboutAct, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About WebM for Lazys"),
                           tr("WebM for Lazys (Qt)\n\nA Linux-first Qt port of the "
                              "WebMConverter GUI. Uses ffmpeg and yt-dlp.\nMIT licensed."));
    });
    helpMenu->addAction(aboutAct);
}

void MainWindow::openFile() {
    const QString f = QFileDialog::getOpenFileName(
        this, tr("Open video"), QString(),
        tr("Video (*.mp4 *.mkv *.webm *.avi *.mov *.m4v *.ts *.flv *.wmv);;All files (*)"));
    if (!f.isEmpty()) loadFile(f);
}

void MainWindow::loadFile(const QString& path) {
    QFileInfo fi(path);
    if (!fi.exists()) {
        showError(tr("File not found"), path);
        return;
    }
    VideoInfo vi;
    QString err;
    if (!probe_->probe(path, vi, &err)) {
        showError(tr("Could not probe file"), err);
        return;
    }

    info_ = vi;
    inputPath_ = path;
    resetFiltersForNewFile();
    encoding_ = encodingPanel_->options();

    outputPath_ = defaultOutputPath();
    outputEdit_->setText(outputPath_);

    timeline_->setDuration(info_.durationSeconds);
    timeline_->setFrameRate(info_.frameRateValue());
    timeline_->setCurrentTime(0.0);
    syncTimelineTrim();

    preview_->setFfmpegPath(ffmpegPath_);
    preview_->setSource(path, info_);

    currentFrame_ = 0;
    requestPreview(0);

    const int total = static_cast<int>(info_.durationSeconds);
    const QString dur = QString("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QChar('0'));
    statusLabel_->setText(tr("%1 — %2x%3 @ %4 fps — %5")
                              .arg(fi.fileName())
                              .arg(info_.width)
                              .arg(info_.height)
                              .arg(info_.frameRateValue(), 0, 'f', 2)
                              .arg(dur));
    appendLog(tr("Loaded %1 (%2x%3, %4 fps, %5 s)")
                  .arg(path)
                  .arg(info_.width)
                  .arg(info_.height)
                  .arg(info_.frameRateValue(), 0, 'f', 2)
                  .arg(info_.durationSeconds, 0, 'f', 2));
    setWindowTitle(tr("%1 — WebM for Lazys (Qt)").arg(fi.fileName()));
}

void MainWindow::resetFiltersForNewFile() {
    filters_ = FilterModel{};
    if (filterPanel_) {
        filterPanel_->setLoop(false);
        filterPanel_->setReverse(false);
    }
    if (canvas_) {
        canvas_->setCropEditable(true);
        canvas_->setCropRect(QRect());
    }
}

void MainWindow::requestPreview(int frame) {
    if (!preview_->hasSource()) return;
    const long long maxFrame = std::max<long long>(0, info_.frameCount() - 1);
    if (frame < 0) frame = 0;
    if (frame > maxFrame) frame = static_cast<int>(maxFrame);
    currentFrame_ = frame;
    preview_->requestFrame(frame);
    timeline_->setCurrentTime(info_.timeOfFrame(frame));
}

void MainWindow::updateCropOverlay() {
    if (!canvas_) return;
    if (filters_.crop.enabled && filters_.crop.w > 0 && filters_.crop.h > 0)
        canvas_->setCropRect(QRect(filters_.crop.x, filters_.crop.y, filters_.crop.w, filters_.crop.h));
    else
        canvas_->setCropRect(QRect());
}

void MainWindow::syncTimelineTrim() {
    if (!timeline_) return;
    if (!filters_.multiTrim.empty()) {
        double s = filters_.multiTrim.front().start;
        double e = filters_.multiTrim.back().end;
        timeline_->setTrimRange(s, e, true);
    } else if (filters_.trim) {
        double e = filters_.trim->end > 0 ? filters_.trim->end : info_.durationSeconds;
        timeline_->setTrimRange(filters_.trim->start, e, true);
    } else {
        timeline_->setTrimRange(0, 0, false);
    }
}

void MainWindow::onSeek(double seconds) {
    requestPreview(info_.frameAt(seconds));
}

void MainWindow::onFrameStep(int delta) {
    if (!preview_->hasSource()) return;
    const long long maxFrame = std::max<long long>(0, info_.frameCount() - 1);
    int f = currentFrame_ + delta;
    f = std::clamp<int>(f, 0, static_cast<int>(maxFrame));
    requestPreview(f);
}

void MainWindow::onCropChanged(const QRect& rect) {
    if (rect.width() <= 0 || rect.height() <= 0) return;
    filters_.crop.enabled = true;
    filters_.crop.x = rect.x();
    filters_.crop.y = rect.y();
    filters_.crop.w = rect.width() & ~1;   // even
    filters_.crop.h = rect.height() & ~1;
    canvas_->setCropRect(QRect(filters_.crop.x, filters_.crop.y, filters_.crop.w, filters_.crop.h));
}

void MainWindow::onLoopToggled(bool on) {
    filters_.loop = on;
    appendLog(on ? tr("Loop enabled") : tr("Loop disabled"));
}
void MainWindow::onReverseToggled(bool on) {
    filters_.reverse = on;
    appendLog(on ? tr("Reverse enabled") : tr("Reverse disabled"));
}

void MainWindow::onFilterClicked(FilterKind kind) {
    auto run = [this](QDialog& d) {
        if (d.exec() == QDialog::Accepted) {
            updateCropOverlay();
            syncTimelineTrim();
            requestPreview(currentFrame_);
            appendLog(tr("Filter updated"));
        }
    };

    switch (kind) {
        case FilterKind::Trim: { TrimDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::MultiTrim: { MultiTrimDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Crop: { CropDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Resize: { ResizeDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Rotate: { RotateDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Rate: { RateDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::DynamicRate: { DynamicRateDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Caption: { CaptionDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Subtitle: { SubtitleDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Overlay: { OverlayDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Fade: { FadeDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Eq: { EqDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Denoise: { DenoiseDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Deinterlace: { DeinterlaceDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::Stabilization: { StabilizationDialog d(filters_, info_, this); run(d); } break;
        case FilterKind::DelayAudio: { DelayAudioDialog d(filters_, info_, this); run(d); } break;
    }
}

QString MainWindow::defaultOutputPath() const {
    if (inputPath_.isEmpty()) return QString();
    const QFileInfo fi(inputPath_);
    const bool webm = (encoding_.codec == VideoCodec::VP8 || encoding_.codec == VideoCodec::VP9);
    const QString ext = webm ? QStringLiteral("webm") : QStringLiteral("mp4");
    return fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QLatin1Char('.') + ext;
}

void MainWindow::onEncodeClicked() {
    if (inputPath_.isEmpty()) {
        showError(tr("No input"), tr("Load a video first (File > Open or drag & drop)."));
        return;
    }
    if (convertRunning_) return;

    outputPath_ = outputEdit_->text().trimmed();
    if (outputPath_.isEmpty()) outputPath_ = defaultOutputPath();
    encoding_ = encodingPanel_->options();

    CommandBuilder builder(info_, filters_, encoding_);
    auto passes = builder.build(inputPath_.toStdString(), outputPath_.toStdString());
    if (passes.empty()) {
        showError(tr("Nothing to do"), tr("The encoder produced no passes."));
        return;
    }

    console_->clearLog();
    for (size_t i = 0; i < passes.size(); ++i) {
        QString cmd = QStringLiteral("ffmpeg");
        for (const auto& t : passes[i].args) cmd += QLatin1Char(' ') + QString::fromStdString(t);
        appendLog(tr("> %1").arg(cmd));
    }

    convertRunning_ = true;
    console_->setBusy(true);
    convertBtn_->setEnabled(false);
    statusLabel_->setText(tr("Encoding…"));

    encoder_->setFfmpegPath(ffmpegPath_);
    encoder_->start(passes, info_.durationSeconds);
}

void MainWindow::onCancelClicked() {
    if (encoder_) {
        encoder_->cancel();
        appendLog(tr("Cancel requested"));
    }
}

void MainWindow::onPreviewFrameReady(int frame, const QImage& image) {
    Q_UNUSED(frame);
    canvas_->setImage(image);
    updateCropOverlay();
}

void MainWindow::onPreviewError(const QString& message) {
    appendLog(tr("Preview: %1").arg(message));
}

void MainWindow::onEncodeProgress(qint64 frame, double timeSeconds, double percent,
                                  double fps, double speed) {
    console_->setProgress(percent);
    statusLabel_->setText(tr("Encoding… frame %1  %2s  %3x  (%4%)")
                              .arg(frame)
                              .arg(timeSeconds, 0, 'f', 1)
                              .arg(speed, 0, 'f', 2)
                              .arg(percent, 0, 'f', 1));
    Q_UNUSED(fps);
}

void MainWindow::onEncodeFinished(bool success, const QString& error) {
    convertRunning_ = false;
    console_->setBusy(false);
    convertBtn_->setEnabled(true);

    if (success) {
        console_->setProgress(100);
        appendLog(tr("Done -> %1").arg(outputPath_));
        statusLabel_->setText(tr("Done: %1").arg(outputPath_));
        if (!convertQueue_.isEmpty()) startNextConvert();
    } else {
        appendLog(tr("Error: %1").arg(error));
        statusLabel_->setText(tr("Failed: %1").arg(error));
    }
}

void MainWindow::startNextConvert() {
    if (convertQueue_.isEmpty()) return;
    const QString next = convertQueue_.takeFirst();
    appendLog(tr("=== Converting %1").arg(next));
    loadFile(next);
    if (inputPath_ == next) onEncodeClicked();
}

QString MainWindow::formatSelectorForCombo() const {
    const QString data = dlFormat_ ? dlFormat_->currentData().toString() : QString();
    return data == QStringLiteral("best") ? QString() : data;
}

void MainWindow::onDownloadClicked() {
    const QString url = urlEdit_->text().trimmed();
    if (url.isEmpty()) {
        showError(tr("No URL"), tr("Enter a video URL to download."));
        return;
    }
    QString dir = dlDirEdit_->text().trimmed();
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        if (dir.isEmpty()) dir = QDir::homePath();
        dlDirEdit_->setText(dir);
    }
    if (!YtdlpRunner::isAvailable())
        dlLog_->appendPlainText(tr("Warning: yt-dlp not found on PATH; download will likely fail."));

    ytdlp_->setBinary(ytdlpPath_);
    dlLog_->clear();
    dlProgress_->setValue(0);
    ytdlp_->download(url, dir, formatSelectorForCombo());
}

void MainWindow::onDownloadProgress(double percent, const QString& speed, const QString& eta) {
    dlProgress_->setValue(static_cast<int>(percent));
    if (dlLog_) {
        // Keep it light: only append occasional status via the progress bar.
        Q_UNUSED(speed);
        Q_UNUSED(eta);
    }
}

void MainWindow::onDownloadFinished(bool success, const QString& outputPath, const QString& error) {
    if (success) {
        dlLog_->appendPlainText(tr("Saved: %1").arg(outputPath));
        if (!outputPath.isEmpty() && QFileInfo::exists(outputPath)) {
            loadFile(outputPath);
            tabs_->setCurrentIndex(0);
        }
    } else {
        dlLog_->appendPlainText(tr("Download failed: %1").arg(error));
    }
}

void MainWindow::onAddMergeFiles() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Add videos"), QString(),
        tr("Video (*.mp4 *.mkv *.webm *.avi *.mov *.m4v *.ts);;All files (*)"));
    for (const QString& f : files) mergeList_->addItem(f);
}

void MainWindow::onStartMerge() {
    QStringList files;
    for (int i = 0; i < mergeList_->count(); ++i) files << mergeList_->item(i)->text();
    if (files.isEmpty()) {
        showError(tr("No files"), tr("Add at least one file."));
        return;
    }

    if (mergeMode_->isChecked()) {
        if (files.size() < 2) {
            showError(tr("Need two files"), tr("Grid merge needs two files."));
            return;
        }
        loadFile(files[0]);
        if (inputPath_ != files[0]) return;
        filters_.grid.enabled = true;
        filters_.grid.vertical = false;
        filters_.grid.otherFile = files[1].toStdString();
        outputPath_ = defaultOutputPath();
        outputEdit_->setText(outputPath_);
        onEncodeClicked();
    } else {
        convertQueue_ = files;
        appendLog(tr("Batch convert: %1 file(s)").arg(files.size()));
        startNextConvert();
    }
}

void MainWindow::appendLog(const QString& line) {
    if (console_) console_->appendLog(line);
}

void MainWindow::showError(const QString& title, const QString& message) {
    QMessageBox::critical(this, title, message);
}

void MainWindow::loadSettings() {
    QSettings s(QStringLiteral("webmforlazies"), QStringLiteral("webmconverter-qt"));
    QString dir = s.value(QStringLiteral("download/dir")).toString();
    if (dir.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        if (dir.isEmpty()) dir = QDir::homePath();
    }
    dlDirEdit_->setText(dir);
}

void MainWindow::saveSettings() {
    QSettings s(QStringLiteral("webmforlazies"), QStringLiteral("webmconverter-qt"));
    s.setValue(QStringLiteral("download/dir"), dlDirEdit_->text());
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData() && e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    if (!e->mimeData() || !e->mimeData()->hasUrls()) return;
    QStringList paths;
    for (const QUrl& u : e->mimeData()->urls()) {
        if (u.isLocalFile()) paths << u.toLocalFile();
    }
    if (paths.isEmpty()) return;
    if (paths.size() == 1) {
        loadFile(paths.first());
    } else {
        for (const QString& p : paths) mergeList_->addItem(p);
        tabs_->setCurrentIndex(2);
    }
    e->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    saveSettings();
    QMainWindow::closeEvent(e);
}

} // namespace wc
