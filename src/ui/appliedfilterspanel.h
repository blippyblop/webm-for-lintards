#pragma once
// Shows the currently-applied filters as large icons. Right-click an item to
// modify or delete it.

#include <QWidget>
#include <QColor>

#include "core/filtermodel.h"
#include "ui/filterpanel.h"

class QListWidget;

namespace wc {

class AppliedFiltersPanel : public QWidget {
    Q_OBJECT
public:
    explicit AppliedFiltersPanel(QWidget* parent = nullptr);

    void setModel(const FilterModel& filters);

signals:
    void modifyRequested(FilterKind kind);
    void deleteRequested(FilterKind kind);

private:
    void addItem(FilterKind kind, const QString& name, const QColor& color);
    QListWidget* list_ = nullptr;
};

} // namespace wc
