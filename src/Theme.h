#pragma once
#include <QColor>
#include <QList>
#include <QPixmap>
#include <QString>
#include <QStringList>

struct Palette {
    QString id, name;
    QColor bg, panel, panel2, border, text, muted, accent;
    QColor groupBg, groupText;                         // cabeçalho do console na lista
    QColor rowLight, rowLightText, rowDark, rowDarkText, rowSel, rowSelText, separator;
    QColor ok, warn, err, plate;
    QColor logoColor;                                  // inválida = logo original sobre uma placa clara
    QColor gradA, gradB;                               // degradê personalizado (opcional)
    int rowRadius = 10, rowGap = 4;
    int gradient = -1;                                 // -1 = opção do usuário; 0/1 = o tema decide
};

namespace Theme {
enum Kind { All, Normal };   // All = muda tudo (cores, lista, grupos, degradê...) | Normal = botões, fontes, etc.

QList<Palette> builtins();
const Palette& current();
bool gradientOn();
QStringList roots();                                   // AppData/themes e <pasta do .exe>/themes
QStringList list(Kind k);                              // arquivos .css/.qss disponíveis
QString pathOf(Kind k, const QString& file);
QString importDir(Kind k);
Kind importFile(const QString& path);                  // detecta all/normal pelo conteúdo e copia
QString notice();                                      // aviso da última aplicação
void apply();
QPixmap logo(const QPixmap& original);                 // aplica a cor da logo do tema
}
