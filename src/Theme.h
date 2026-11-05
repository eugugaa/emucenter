#pragma once
#include <QColor>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>

struct Palette {
    QString id, name;
    QColor bg, panel, panel2, border, text, muted, accent;
    QColor groupBg, groupText;                           // console header in the list
    QColor rowLight, rowLightText, rowDark, rowDarkText, rowSel, rowSelText, separator;
    QColor ok, warn, err, plate;
    QColor logoColor;                                    // invalid = original logo on a light plate
    QColor gradA, gradB;                                 // custom gradient (optional)
    int rowRadius = 10, rowGap = 4;
    int gradient = -1;                                   // -1 = user option; 0/1 = theme decides
};

namespace Theme {
enum Kind { All, Normal };   // All = changes everything (colors, list, groups, gradient...) | Normal = buttons, fonts, etc.

QList<Palette> builtins();
const Palette& current();
bool gradientOn();
QStringList roots();                                     // AppData/themes and <exe folder>/themes
QStringList list(Kind k);                                // available .css/.qss files
QString pathOf(Kind k, const QString& file);
QString importDir(Kind k);
Kind importFile(const QString& path);                    // detects all/normal by content and copies
QString notice();                                        // notice of the last application
void apply();
QPixmap logo(const QPixmap& original);                   // applies the theme's logo color
}