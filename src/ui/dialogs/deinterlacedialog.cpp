#include "ui/dialogs/deinterlacedialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QVBoxLayout>

namespace wc {

DeinterlaceDialog::DeinterlaceDialog(FilterModel& model, const VideoInfo& info, QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    Q_UNUSED(info_);
    setWindowTitle(tr("Deinterlace"));
    setModal(true);

    enableCheck_ = new QCheckBox(tr("Deinterlace (yadif)"), this);
    enableCheck_->setChecked(model_.deinterlace);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &DeinterlaceDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &DeinterlaceDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(enableCheck_);
    layout->addWidget(buttons);
}

void DeinterlaceDialog::accept() {
    model_.deinterlace = enableCheck_->isChecked();
    QDialog::accept();
}

} // namespace wc
