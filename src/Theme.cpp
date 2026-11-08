#include "Theme.h"
#include "FileUtil.h"
#include "I18n.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMap>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <utility>

static Palette g_cur;
static bool g_set = false;
static QString g_notice;

QList<Palette> Theme::builtins() {
    auto mk = [](const char* id, const char* name, QRgb bg, QRgb panel, QRgb panel2, QRgb border, QRgb text, QRgb muted, QRgb accent,
                 QRgb groupBg, QRgb rowLight, QRgb rowDark, QRgb logo) {
        Palette p;
        p.id = QString::fromLatin1(id);
        p.name = QString::fromUtf8(name);
        p.bg = QColor(bg); p.panel = QColor(panel); p.panel2 = QColor(panel2); p.border = QColor(border);
        p.text = QColor(text); p.muted = QColor(muted); p.accent = QColor(accent);
        p.groupBg = QColor(groupBg); p.groupText = QColor(text);
        p.rowLight = QColor(rowLight); p.rowDark = QColor(rowDark);
        p.rowLightText = QColor(text); p.rowDarkText = QColor(text);
        p.rowSel = QColor(accent); p.rowSelText = QColor(0xffffff);
        p.separator = QColor(border);
        p.ok = QColor(0x5fd38d); p.warn = QColor(0xf2c14e); p.err = QColor(0xff7b7b);
        p.plate = QColor(0xe8ecf6);
        if (logo) p.logoColor = QColor(logo);
        return p;
    };
    QList<Palette> v;
    v << mk("grafite", "Azul Grafite", 0x1b1f2b, 0x232a3a, 0x2d3550, 0x3b4562, 0xe6eaf5, 0x98a2bd, 0x5b8cff, 0x2a3350, 0x212840, 0x181d30, 0xdbe4ff);
    v << mk("meianoite", "Meia-noite", 0x0d1017, 0x141824, 0x1c2233, 0x2a3248, 0xe4e8f4, 0x8791a8, 0x7a8cff, 0x1a2036, 0x171c2c, 0x0f1320, 0x8fa6ff);
    Palette c = mk("claro", "Cinza Claro", 0xe6e6ea, 0xf3f3f6, 0xdcdce2, 0xc4c4cc, 0x14161c, 0x666a75, 0x3b6fe0, 0xd2d4de, 0xeceef4, 0xe0e3ec, 0);
    c.ok = QColor(0x1b7f3b); c.warn = QColor(0xb8860b); c.err = QColor(0xb3261e);
    c.plate = QColor(0xf3f3f6);
    v << c;
    return v;
}

const Palette& Theme::current() {
    if (!g_set) { g_cur = builtins().constFirst(); g_set = true; }
    return g_cur;
}

bool Theme::gradientOn() {
    const Palette& p = current();
    return p.gradient >= 0 ? p.gradient == 1 : QSettings().value("gradient", true).toBool();
}

QStringList Theme::roots() {
    return {QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/themes",
            QCoreApplication::applicationDirPath() + "/themes"};
}
static QString kindDir(Theme::Kind k) { return k == Theme::All ? QStringLiteral("all") : QStringLiteral("normal"); }
QString Theme::importDir(Kind k) { return roots().constFirst() + "/" + kindDir(k); }

QStringList Theme::list(Kind k) {
    QStringList out;
    QSet<QString> seen;
    for (const QString& r : roots())
        for (const QString& f : QDir(r + "/" + kindDir(k)).entryList({"*.css", "*.qss"}, QDir::Files, QDir::Name))
            if (!seen.contains(f)) { seen.insert(f); out << f; }
    return out;
}

QString Theme::pathOf(Kind k, const QString& f) {
    if (f.isEmpty()) return {};
    for (const QString& r : roots()) {
        const QString p = r + "/" + kindDir(k) + "/" + f;
        if (QFile::exists(p)) return p;
    }
    return {};
}
QString Theme::notice() { return g_notice; }

struct Parsed {
    QMap<QString, QString> vars;
    QString qss;
    bool hadVars = false;
};

static const QRegularExpression& rootRe() {
    static const QRegularExpression re(R"(:root\s*\{[^}]*\})");
    return re;
}

static Parsed parseCss(QString css) {
    Parsed r;
    css.remove(QRegularExpression(R"(/\*.*?\*/)", QRegularExpression::DotMatchesEverythingOption));
    auto it = rootRe().globalMatch(css);
    while (it.hasNext()) {
        r.hadVars = true;
        QString body = it.next().captured(0);
        body = body.mid(body.indexOf('{') + 1);
        body.chop(1);
        const QStringList lines = body.split(';');
        for (const QString& line : lines) {
            const int i = line.indexOf(':');
            if (i < 0) continue;
            QString name = line.left(i).trimmed();
            if (name.startsWith("--")) name = name.mid(2);
            r.vars[name.toLower()] = line.mid(i + 1).trimmed();
        }
    }
    r.qss = css;
    r.qss.remove(rootRe());
    return r;
}

static void applyVars(Palette& p, const QMap<QString, QString>& v) {
    auto col = [&v](const char* k, QColor& dst) {
        if (v.contains(QString::fromLatin1(k))) {
            const QColor c(v[QString::fromLatin1(k)]);
            if (c.isValid()) dst = c;
        }
    };
    col("bg", p.bg); col("panel", p.panel); col("panel2", p.panel2); col("border", p.border);
    col("text", p.text); col("muted", p.muted); col("accent", p.accent);
    col("group-bg", p.groupBg); col("group-text", p.groupText);
    col("row-light", p.rowLight); col("row-light-text", p.rowLightText);
    col("row-dark", p.rowDark); col("row-dark-text", p.rowDarkText);
    col("row-selected", p.rowSel); col("row-selected-text", p.rowSelText); col("row-separator", p.separator);
    col("ok", p.ok); col("warn", p.warn); col("err", p.err); col("plate", p.plate);
    col("logo-color", p.logoColor); col("gradient-a", p.gradA); col("gradient-b", p.gradB);
    if (v.contains("row-radius")) p.rowRadius = qBound(0, v["row-radius"].toInt(), 30);
    if (v.contains("row-gap")) p.rowGap = qBound(0, v["row-gap"].toInt(), 20);
    if (v.contains("gradient")) {
        const QString g = v["gradient"].toLower();
        p.gradient = (g == "on" || g == "true" || g == "1" || g == "yes") ? 1 : 0;
    }
    if (v.value("logo-color").toLower() == "original") p.logoColor = QColor();
}

static QString readText(const QString& path) { return QString::fromUtf8(FileUtil::readAll(path)); }

static void transferToAll(const QString& normalPath, const QString& raw, const QString& fileName) {
    QStringList blocks;
    auto it = rootRe().globalMatch(raw);
    while (it.hasNext()) blocks << it.next().captured(0);
    if (blocks.isEmpty()) return;

    QDir().mkpath(Theme::importDir(Theme::All));
    QString name = fileName;
    if (QFile::exists(Theme::importDir(Theme::All) + "/" + name)) name = QFileInfo(fileName).completeBaseName() + "-do-normal.css";
    const QString header = "/* Movido automaticamente de themes/normal/" + fileName + " */\n";
    if (!FileUtil::writeAll(Theme::importDir(Theme::All) + "/" + name, (header + blocks.join("\n\n") + "\n").toUtf8())) return;

    QString rest = raw;
    rest.remove(rootRe());
    FileUtil::writeAll(normalPath, ("/* As variaveis :root daqui foram movidas para themes/all/" + name + " */\n" + rest).toUtf8());

    g_notice = T("Parte de '%1' era de tema completo e foi movida para themes/all/%2.").arg(fileName, name);
    QSettings st;
    if (st.value("themeAll").toString().isEmpty()) st.setValue("themeAll", name);
}

Theme::Kind Theme::importFile(const QString& path) {
    const Kind k = parseCss(readText(path)).hadVars ? All : Normal;
    QDir().mkpath(importDir(k));
    const QString dest = importDir(k) + "/" + QFileInfo(path).fileName();
    QFile::remove(dest);
    QFile::copy(path, dest);
    return k;
}

QStringList Theme::availableFonts() {
    static const QStringList fams = QFontDatabase::families();
    return fams;
}

static QString pickFont(const QString& chosen, const QStringList& stack) {
    const QStringList fams = Theme::availableFonts();
    if (!chosen.isEmpty() && fams.contains(chosen)) return chosen;
    for (const QString& f : stack) if (fams.contains(f)) return f;
    return QStringLiteral("sans-serif");
}

QString Theme::fontFamily(FontKind k) {
    QSettings s;
    switch (k) {
    case Title:
        return pickFont(s.value("ui/fontTitle").toString(), {"Bahnschrift", "Segoe UI Variable Display", "Segoe UI Semibold", "Montserrat", "Poppins", "Ubuntu", "Noto Sans"});
    case Mono:
        return pickFont(s.value("ui/fontMono").toString(), {"Cascadia Mono", "Consolas", "JetBrains Mono", "DejaVu Sans Mono", "Liberation Mono", "Courier New"});
    default:
        return pickFont(s.value("ui/fontBody").toString(), {"Segoe UI Variable Text", "Segoe UI", "Inter", "Noto Sans", "Ubuntu", "Helvetica Neue", "Arial"});
    }
}

QFont Theme::font(FontKind k, int pixelSize, int weight) {
    QFont f(fontFamily(k));
    f.setPixelSize(qMax(8, pixelSize * QSettings().value("ui/fontScale", 100).toInt() / 100));
    f.setWeight(QFont::Weight(weight));
    return f;
}

int Theme::rowHeight() {
    const QString d = QSettings().value("ui/density", "normal").toString();
    return d == "compact" ? 44 : d == "large" ? 68 : 54;
}

QString Theme::backgroundImage() {
    const QString p = QSettings().value("ui/bgImage").toString();
    return (!p.isEmpty() && QFile::exists(p)) ? p : QString();
}
int Theme::backgroundOpacity() { return qBound(0, QSettings().value("ui/bgOpacity", 25).toInt(), 100); }
QString Theme::customLogo() {
    const QString p = QSettings().value("ui/logoImage").toString();
    return (!p.isEmpty() && QFile::exists(p)) ? p : QString();
}

QPixmap Theme::logo(const QPixmap& src) {
    const Palette& p = current();
    if (!p.logoColor.isValid() || src.isNull()) return src;
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter pt(&img);
    pt.setCompositionMode(QPainter::CompositionMode_SourceIn);
    pt.fillRect(img.rect(), p.logoColor);
    pt.end();
    return QPixmap::fromImage(img);
}

static QString rgba(const QColor& c, int a) {
    return QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(a);
}

static QString buildStyle(const Palette& p, bool gradient) {
    auto grad = [](const QColor& a, const QColor& b) {
        return QString("qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %1, stop:1 %2)").arg(a.name(), b.name());
    };
    const bool custom = p.gradA.isValid() && p.gradB.isValid();
    const QString bgFill = gradient ? (custom ? grad(p.gradA, p.gradB) : grad(p.panel, p.bg)) : p.bg.name();
    const QString panelFill = gradient ? grad(p.panel2, p.panel) : p.panel.name();
    const QString btnFill = gradient ? grad(p.panel2.lighter(115), p.panel2.darker(115)) : p.panel2.name();
    const QString playFill = gradient ? grad(p.accent.lighter(130), p.accent.darker(115)) : p.accent.name();
    const QString titleFill = gradient ? (custom ? grad(p.gradA, p.gradB) : grad(p.panel2, p.panel)) : p.panel.name();
    const QString logoBg = p.logoColor.isValid() ? QStringLiteral("transparent") : p.plate.name();
    const int scale = QSettings().value("ui/fontScale", 100).toInt();
    const int px = qMax(9, 14 * scale / 100);

    QString s = R"(
* { font-family: "@fontBody"; font-size: @pxpx; color: @text; }
QMainWindow, QDialog, QWidget#root { background: @bgFill; }
QWidget#side { background: @panelFill; border-right: 1px solid @border; }
QWidget#titlebar { background: @titleFill; border-bottom: 1px solid @border; }
QWidget#card { background: @panel; border: 1px solid @border; border-radius: 14px; }
QLabel { background: transparent; }
QLabel#title { font-family: "@fontTitle"; font-size: @pxbigpx; font-weight: 800; }
QLabel#heading { font-family: "@fontTitle"; font-size: @pxmidpx; font-weight: 700; }
QLabel#muted { color: @muted; }
QLabel#mono { font-family: "@fontMono"; color: @muted; }
QToolTip { background: @panel2; color: @text; border: 1px solid @accent; padding: 6px; border-radius: 6px; }
QTreeWidget { background: transparent; border: none; outline: 0; }
QTextBrowser, QTextEdit, QListWidget, QLineEdit, QComboBox, QTableWidget, QSpinBox {
  background: @panel; border: 1px solid @border; border-radius: 10px; padding: 6px;
  selection-background-color: @accent; selection-color: #ffffff; }
QLineEdit:focus, QTextEdit:focus, QComboBox:focus { border: 1px solid @accent; }
QComboBox { min-height: 24px; padding: 6px 12px; }
QComboBox QAbstractItemView { background: @panel2; selection-background-color: @accent; border: 1px solid @border; }
QPushButton { background: @btnFill; border: 1px solid @border; border-radius: 10px; padding: 10px 22px; min-height: 22px; }
QPushButton:hover { border-color: @accent; }
QPushButton:pressed { background: @accent; color: #ffffff; }
QPushButton:checked { background: @accent; color: #ffffff; border-color: @accent; }
QPushButton:disabled { color: @muted; }
QPushButton#play { font-family: "@fontTitle"; background: @playFill; color: #ffffff; border: none; font-size: @pxbigpx; font-weight: 800; padding: 14px 54px; border-radius: 12px; }
QPushButton#play:disabled { background: @border; color: @muted; }
QPushButton#chip { font-family: "@fontTitle"; padding: 8px 18px; border-radius: 18px; min-height: 18px; }
QToolButton { background: transparent; border: none; border-radius: 10px; padding: 8px 12px; }
QToolButton:hover { background: @panel2; }
QToolButton#winbtn { border-radius: 0; padding: 10px 18px; font-size: 15px; }
QToolButton#closebtn { border-radius: 0; padding: 10px 18px; font-size: 15px; }
QToolButton#closebtn:hover { background: #c42b1c; color: #ffffff; }
QToolButton#logoBtn { background: @logoBg; border: 1px solid @border; border-radius: 12px; padding: 10px 14px; }
QToolButton#logoBtn:hover { border-color: @accent; }
QToolButton::menu-indicator { image: none; }
QProgressBar { border: 1px solid @border; border-radius: 7px; background: @panel; text-align: center; min-height: 18px; max-height: 18px; }
QProgressBar::chunk { background: @accent; border-radius: 6px; }
QStatusBar { background: @panel; color: @muted; border-top: 1px solid @border; }
QStatusBar::item { border: none; }
QTabWidget::pane { border: 1px solid @border; border-radius: 10px; top: -1px; }
QTabBar::tab { font-family: "@fontTitle"; padding: 10px 20px; background: @panel; border: 1px solid @border; border-bottom: none;
  border-top-left-radius: 10px; border-top-right-radius: 10px; margin-right: 3px; color: @muted; }
QTabBar::tab:selected { background: @panel2; color: @text; border-bottom: 2px solid @accent; }
QHeaderView::section { background: @panel2; border: none; padding: 8px; }
QTableWidget { gridline-color: @border; }
QMenu { background: @panel2; border: 1px solid @border; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 9px 30px 9px 22px; border-radius: 6px; }
QMenu::item:selected { background: @accent; color: #ffffff; }
QMenu::separator { height: 1px; background: @border; margin: 6px 4px; }
QScrollArea { border: none; background: transparent; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
QScrollBar::handle:vertical { background: @border; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: @accent; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px; }
QScrollBar::handle:horizontal { background: @border; border-radius: 5px; min-width: 30px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QSlider::groove:horizontal { height: 6px; background: @border; border-radius: 3px; }
QSlider::sub-page:horizontal { background: @accent; border-radius: 3px; }
QSlider::handle:horizontal { background: @text; width: 16px; margin: -6px 0; border-radius: 8px; }
QCheckBox { spacing: 10px; }
QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid @border; border-radius: 5px; background: @panel; }
QCheckBox::indicator:checked { background: @accent; border-color: @accent; }
QGroupBox { font-family: "@fontTitle"; border: 1px solid @border; border-radius: 12px; margin-top: 18px; padding-top: 14px; }
QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 8px; color: @muted; }
)";
    const QList<QPair<QString, QString>> vars = {
        {"@pxbig", QString::number(qRound(px * 1.9))}, {"@pxmid", QString::number(qRound(px * 1.45))}, {"@px", QString::number(px)},
        {"@fontBody", Theme::fontFamily(Theme::Body)}, {"@fontTitle", Theme::fontFamily(Theme::Title)}, {"@fontMono", Theme::fontFamily(Theme::Mono)},
        {"@bgFill", bgFill}, {"@panelFill", panelFill}, {"@btnFill", btnFill}, {"@playFill", playFill},
        {"@titleFill", titleFill}, {"@logoBg", logoBg}, {"@panel2", p.panel2.name()}, {"@panel", p.panel.name()},
        {"@border", p.border.name()}, {"@text", p.text.name()}, {"@muted", p.muted.name()},
        {"@accent", p.accent.name()}};
    for (const auto& v : vars) s.replace(v.first, v.second);
    return s;
}

static QString userStyle(const Palette& p) {
    QSettings s;
    QString css;
    const QString style = s.value("ui/btnStyle", "theme").toString();
    const int radius = s.value("ui/btnRadius", -1).toInt();
    if (style == "outline")
        css += QString("QPushButton { background: transparent; border: 2px solid %1; } QPushButton:hover { background: %1; color: #ffffff; }\n").arg(p.accent.name());
    else if (style == "flat")
        css += QString("QPushButton { background: %1; border: none; } QPushButton:hover { background: %2; }\n").arg(p.panel.name(), p.panel2.name());
    else if (style == "filled")
        css += QString("QPushButton { background: %1; border: 1px solid %2; }\n").arg(p.panel2.name(), p.border.name());
    if (radius >= 0) css += QString("QPushButton { border-radius: %1px; }\nQPushButton#chip { border-radius: %1px; }\n").arg(radius);
    if (!Theme::backgroundImage().isEmpty())
        css += QString("QWidget#root { background: transparent; }\nQWidget#side { background: %1; }\n").arg(rgba(p.panel, 215));
    return css;
}

void Theme::apply() {
    QSettings st;
    g_notice.clear();
    const QString id = st.value("theme", "grafite").toString();
    const QList<Palette> all0 = builtins();
    Palette p = all0.constFirst();
    for (const auto& b : all0) if (b.id == id) p = b;

    Parsed normal, all;
    const QString nFile = st.value("themeNormal").toString();
    const QString nPath = pathOf(Normal, nFile);
    if (!nPath.isEmpty()) {
        const QString raw = readText(nPath);
        normal = parseCss(raw);
        if (normal.hadVars) transferToAll(nPath, raw, nFile);
    }
    const QString aPath = pathOf(All, st.value("themeAll").toString());
    if (!aPath.isEmpty()) all = parseCss(readText(aPath));

    applyVars(p, normal.vars);
    applyVars(p, all.vars);

    const QColor userAccent(st.value("ui/accent").toString());
    if (userAccent.isValid()) { p.accent = userAccent; p.rowSel = userAccent; }
    const int rr = st.value("ui/rowRadius", -1).toInt();
    const int rg = st.value("ui/rowGap", -1).toInt();
    if (rr >= 0) p.rowRadius = rr;
    if (rg >= 0) p.rowGap = rg;

    g_cur = p;
    g_set = true;
    qApp->setStyleSheet(buildStyle(p, gradientOn()) + "\n" + all.qss + "\n" + normal.qss + "\n" + userStyle(p));
}
