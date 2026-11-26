#pragma once
// Serialise/deserialise a FilterModel to/from JSON.

#include <QJsonObject>
#include <QString>

#include "core/filtermodel.h"

namespace wc {

QJsonObject filterToJson(const FilterModel& filters);
bool filterFromJson(const QJsonObject& obj, FilterModel& filters, QString* error = nullptr);

} // namespace wc
