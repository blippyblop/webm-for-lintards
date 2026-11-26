#include "ui/dialogs/rotatedialog.h"
#include "ui/scrubpreview.h"

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

namespace wc {

RotateDialog::RotateDialog(FilterModel& model, const VideoInfo& info, QWidget* parent,
                           const QString& inputPath, const QString& ffmpegPath)
    : QDialog(parent), model_(model), info_(info) {
    setWindowTitle(tr("Rotate"));

    auto* layout = new QVBoxLayout(this);
    if (info_.width > 0 && info_.height > 0) {
        layout->addWidget(new QLabel(
            tr("Source size: %1 x %2 px (rotates to %3 x %4 px)")
                .arg(info_.width)
                .arg(info_.height)
                .arg(info_.height)
                .arg(info_.width),
            this));
    }
    layout->addWidget(new QLabel(tr("Rotation:"), this));

    group_ = new QButtonGroup(this);
    group_->setExclusive(true);

    auto* none = new QRadioButton(tr("None (0°)"), this);
    auto* r90 = new QRadioButton(tr("90° clockwise"), this);
    auto* r180 = new QRadioButton(tr("180°"), this);
    auto* r270 = new QRadioButton(tr("270° clockwise"), this);

    group_->addButton(none, 0);
    group_->addButton(r90, 90);
    group_->addButton(r180, 180);
    group_->addButton(r270, 270);

    layout->addWidget(none);
    layout->addWidget(r90);
    layout->addWidget(r180);
    layout->addWidget(r270);

    int normalized = model_.rotateDegrees % 360;
    if (normalized < 0)
        normalized += 360;
    if (normalized != 0 && normalized != 90 && normalized != 180 && normalized != 270)
        normalized = 0;

    if (auto* button = group_->button(normalized))
        button->setChecked(true);
    else
        none->setChecked(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &RotateDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &RotateDialog::reject);

    scrub_ = new ScrubPreview(this);
    if (!inputPath.isEmpty()) scrub_->setSource(inputPath, info_, ffmpegPath);
    scrub_->setEditableModel(model_);
    scrub_->setLimits(model_);
    layout->insertWidget(0, scrub_);
    connect(group_, &QButtonGroup::idClicked, this, [this](int id) {
        scrub_->previewModel().rotateDegrees = (id == 90 || id == 180 || id == 270) ? id : 0;
        scrub_->refreshPreview();
    });
    resize(900, 760);
}

void RotateDialog::accept() {
    const int id = group_->checkedId();
    model_.rotateDegrees = (id == 90 || id == 180 || id == 270) ? id : 0;
    QDialog::accept();
}

} // namespace wc
