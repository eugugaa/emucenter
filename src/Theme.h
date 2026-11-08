#pragma once
#include <QColor>
#include <QFont>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>

struct Palette {
    QString id, name;
    QColor bg, panel, panel2, border, text, muted, accent;
    QColor groupBg, groupText;
    QColor rowLight, rowLightText, rowDark, rowDarkText, rowSel, rowSelText, separator;
    QColor ok, warn, err, plate;
    QColor logoColor;
    QColor gradA, gradB;
    int rowRadius = 10, rowGap = 4;
    int gradient = -1;
};

namespace Theme {
enum Kind { All, Normal };
enum FontKind { Body, Title, Mono };

QList<Palette> builtins();
const Palette& current();
bool gradientOn();
QStringList roots();
QStringList list(Kind k);
QString pathOf(Kind k, const QString& file);
QString importDir(Kind k);
Kind importFile(const QString& path);
QString notice();
void apply();
QPixmap logo(const QPixmap& original);
QString fontFamily(FontKind k);
QFont font(FontKind k, int pixelSize, int weight = QFont::Normal);
QStringList availableFonts();
int rowHeight();
QString backgroundImage();
int backgroundOpacity();
QString customLogo();
}
