#pragma once

#include <QDialog>
#include <QString>

#include <vector>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QListWidget;
class QPushButton;

namespace wc {

// Multi-trim segments. "Add segment…" opens the normal trim scrubber; segments
// are concatenated in list order and can be reordered.
class MultiTrimDialog : public QDialog {
    Q_OBJECT
public:
    explicit MultiTrimDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr,
                             const QString& inputPath = QString(), const QString& ffmpegPath = QString());

    void accept() override;

private:
    void addSegmentFromTrim();
    void editSelected();
    void refreshList();

    FilterModel& model_;
    const VideoInfo& info_;
    QString inputPath_;
    QString ffmpegPath_;

    QListWidget* list_ = nullptr;
    QPushButton* addBtn_ = nullptr;
    QPushButton* editBtn_ = nullptr;
    QPushButton* removeBtn_ = nullptr;
    QPushButton* upBtn_ = nullptr;
    QPushButton* downBtn_ = nullptr;
    std::vector<TrimRange> segments_;
};

} // namespace wc
