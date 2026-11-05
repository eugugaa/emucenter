#pragma once
#include <QString>

// Tradução simples: o texto-fonte está em português; "en" usa o dicionário em I18n.cpp.
namespace I18n {
void setLang(const QString& code);   // "pt" | "en"
QString lang();
QString tr(const QString& pt);
}
inline QString T(const char* pt) { return I18n::tr(QString::fromUtf8(pt)); }
