#pragma once
#include <QList>
#include <QPair>
#include <QString>

namespace I18n {
void setLang(const QString& code);
QString lang();
QString tr(const QString& pt);
QList<QPair<QString, QString>> languages();
}
inline QString T(const char* pt) { return I18n::tr(QString::fromUtf8(pt)); }
