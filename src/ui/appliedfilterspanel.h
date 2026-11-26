#pragma once
// Shows applied filters as large icons, with a JSON view + save/load.

#include <QWidget>
#include <QColor>

#include "core/filtermodel.h"
#include "ui/filterpanel.h"

class QListWidget;
class QStackedWidget;
class QPlainTextEdit;
class QToolButton;

namespace wc {

class AppliedFiltersPanel : public QWidget {
    Q_OBJECT
public:
    explicit AppliedFiltersPanel(QWidget* parent = nullptr);

    void setModel(const FilterModel& filters);

signals:
    void modifyRequested(FilterKind kind);
    void deleteRequested(FilterKind kind);
    void modelLoaded(const FilterModel& filters);

private slots:
    void saveJson();
    void loadJson();
    void applyJson();

private:
    void addItem(FilterKind kind, const QString& name, const QColor& color);
    void updateJsonView();

    QListWidget* list_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    QPlainTextEdit* jsonView_ = nullptr;
    QToolButton* jsonToggle_ = nullptr;
    FilterModel lastModel_;
};

} // namespace wc
