#include "Theme.h"
#include "I18n.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

static Palette g_cur;
static bool g_set = false;
static QString g_notice;

// ------------------------------------------------------------------ built-in palettes
QList<Palette> Theme::builtins() {
    auto mk = [](const char* id, const char* name, const char* bg, const char* panel, const char* panel2, const char* border,
                 const char* text, const char* muted, const char* accent, const char* groupBg, const char* rowDark,
                 const char* logo) {
        Palette p;
        p.id = id; p.name = name;
        p.bg = bg; p.panel = panel; p.panel2 = panel2; p.border = border;
        p.text = text; p.muted = muted; p.accent = accent;
        p.groupBg = groupBg; p.groupText = text;
        p.rowLight = "#e8ecf6"; p.rowLightText = "#161a26";
        p.rowDark = rowDark; p.rowDarkText = "#e6eaf5";
        p.rowSel = accent; p.rowSelText = "#ffffff";
        p.separator = border;
        p.ok = "#5fd38d"; p.warn = "#f2c14e"; p.err = "#ff6b6b";
        p.plate = "#e8ecf6";
        if (logo[0]) p.logoColor = logo;   // empty = original logo
        return p;
    };
    QList<Palette> v;
    v << mk("grafite", "Graphite Blue", "#1b1f2b", "#232a3a", "#2d3550", "#3b4562", "#e6eaf5", "#98a2bd", "#5b8cff", "#2a3350", "#141824", "#dbe4ff");
    v << mk("meianoite", "Midnight", "#0d1017", "#141824", "#1c2233", "#2a3248", "#e4e8f4", "#8791a8", "#7a8cff", "#1a2036", "#090b10", "#8fa6ff");
    Palette c = mk("claro", "Light Gray", "#e6e6ea", "#f3f3f6", "#dcdce2", "#c4c4cc", "#14161c", "#666a75", "#3b6fe0", "#d2d4de", "#262a36", "");
    c.ok = "#1b7f3b"; c.warn = "#b8860b"; c.err = "#b3261e";
    c.plate = "#f3f3f6";
    v << c;
    return v;
}

const Palette& Theme::current() {
    if (!g_set) { g_cur = builtins().first(); g_set = true; }
    return g_cur;
}

bool Theme::gradientOn() {
    const Palette& p = current();
    return p.gradient >= 0 ? p.gradient == 1 : QSettings().value("gradient", true).toBool();
}

// ------------------------------------------------------------------ theme folders (themes/all and themes/normal)
QStringList Theme::roots() {
    return {QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/themes",
            QCoreApplication::applicationDirPath() + "/themes"};
}
static QString kindDir(Theme::Kind k) { return k == Theme::All ? "all" : "normal"; }
QString Theme::importDir(Kind k) { return roots().first() + "/" + kindDir(k); }

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

// ------------------------------------------------------------------ CSS reading: variables in :root + QSS
struct Parsed { QMap<QString, QString> vars; QString qss; bool hadVars = false; };

static const QRegularExpression& rootRe() {
    static const QRegularExpression re(R"(:root\s*\{[^}]*\})");
    return re;
}

static Parsed parseCss(QString css) {
    Parsed r;
    css.remove(QRegularExpression(R"(/\*.*?\*/)", QRegularExpression::DotMatchesEverythingOption));   // comments
    auto it = rootRe().globalMatch(css);
    while (it.hasNext()) {
        r.hadVars = true;
        QString body = it.next().captured(0);
        body = body.mid(body.indexOf('{') + 1);
        body.chop(1);
        for (const QString& line : body.split(';')) {
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
    auto col = [&](const char* k, QColor& dst) {
        if (v.contains(k)) { const QColor c(v[k]); if (c.isValid()) dst = c; }
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

static QString readFile(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

// "Normal" file containing "all" things (:root variables): this part goes to themes/all.
static void transferToAll(const QString& normalPath, const QString& raw, const QString& fileName) {
    QStringList blocks;
    auto it = rootRe().globalMatch(raw);
    while (it.hasNext()) blocks << it.next().captured(0);
    if (blocks.isEmpty()) return;

    QDir().mkpath(Theme::importDir(Theme::All));
    QString name = fileName;
    if (QFile::exists(Theme::importDir(Theme::All) + "/" + name)) name = QFileInfo(fileName).completeBaseName() + "-from-normal.css";
    QFile out(Theme::importDir(Theme::All) + "/" + name);
    if (!out.open(QIODevice::WriteOnly)) return;
    out.write(("/* Automatically moved from themes/normal/" + fileName + " */\n" + blocks.join("\n\n") + "\n").toUtf8());
    out.close();

    QString rest = raw;
    rest.remove(rootRe());
    QFile back(normalPath);
    if (back.open(QIODevice::WriteOnly | QIODevice::Truncate))
        back.write(("/* The :root variables from here were moved to themes/all/" + name + " */\n" + rest).toUtf8());

    g_notice = T("Part of '%1' was a complete theme and was moved to themes/all/%2.").arg(fileName, name);
    QSettings st;
    if (st.value("themeAll").toString().isEmpty()) st.setValue("themeAll", name);   // keeps the visual appearance
}

Theme::Kind Theme::importFile(const QString& path) {
    const Kind k = parseCss(readFile(path)).hadVars ? All : Normal;
    QDir().mkpath(importDir(k));
    const QString dest = importDir(k) + "/" + QFileInfo(path).fileName();
    QFile::remove(dest);
    QFile::copy(path, dest);
    return k;
}

// ------------------------------------------------------------------ colored logo according to the theme
QPixmap Theme::logo(const QPixmap& src) {
    const Palette& p = current();
    if (!p.logoColor.isValid() || src.isNull()) return src;
    QImage img = src.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter pt(&img);
    pt.setCompositionMode(QPainter::CompositionMode_SourceIn);   // keeps only the shape (alpha) of the logo
    pt.fillRect(img.rect(), p.logoColor);
    pt.end();
    return QPixmap::fromImage(img);
}

// ------------------------------------------------------------------ base QSS
static QString buildStyle(const Palette& p, bool gradient) {
    auto grad = [](const QColor& a, const QColor& b) {
        return QString("qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 %1, stop:1 %2)").arg(a.name(), b.name());
    };
    const bool custom = p.gradA.isValid() && p.gradB.isValid();
    QString bgFill = gradient ? (custom ? grad(p.gradA, p.gradB) : grad(p.panel, p.bg)) : p.bg.name();
    QString panelFill = gradient ? grad(p.panel2, p.panel) : p.panel.name();
    QString btnFill = gradient ? grad(p.panel2.lighter(115), p.panel2.darker(115)) : p.panel2.name();
    QString playFill = gradient ? grad(p.accent.lighter(130), p.accent.darker(115)) : p.accent.name();
    QString titleFill = gradient ? (custom ? grad(p.gradA, p.gradB) : grad(p.panel2, p.panel)) : p.panel.name();
    const QString logoBg = p.logoColor.isValid() ? "transparent" : p.plate.name();

    QString s = R"(
- { font-family: "Segoe UI","Inter","Noto Sans",sans-serif; font-size: 14px; color: @text; }
QMainWindow, QDialog, QWidget#root { background: @bgFill; }
QWidget#side { background: @panelFill; border-right: 1px solid @border; }
QWidget#titlebar { background: @titleFill; border-bottom: 1px solid @border; }
QLabel { background: transparent; }
QLabel#title { font-size: 28px; font-weight: 800; }
QLabel#muted { color: @muted; }
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
QPushButton#play { background: @playFill; color: #ffffff; border: none; font-size: 18px; font-weight: 800; padding: 14px 54px; border-radius: 12px; }
QPushButton#play:disabled { background: @border; color: @muted; }
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
QTabBar::tab { padding: 10px 20px; background: @panel; border: 1px solid @border; border-bottom: none;
  border-top-left-radius: 10px; border-top-right-radius: 10px; margin-right: 3px; color: @muted; }
QTabBar::tab:selected { background: @panel2; color: @text; border-bottom: 2px solid @accent; }
QHeaderView::section { background: @panel2; border: none; padding: 8px; }
QTableWidget { gridline-color: @border; }
QMenu { background: @panel2; border: 1px solid @border; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 9px 30px 9px 22px; border-radius: 6px; }
QMenu::item:selected { background: @accent; color: #ffffff; }
QMenu::separator { height: 1px; background: @border; margin: 6px 4px; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
QScrollBar::handle:vertical { background: @border; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: @accent; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px; }
QScrollBar::handle:horizontal { background: @border; border-radius: 5px; min-width: 30px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QCheckBox { spacing: 10px; }
QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid @border; border-radius: 5px; background: @panel; }
QCheckBox::indicator:checked { background: @accent; border-color: @accent; }
QGroupBox { border: 1px solid @border; border-radius: 10px; margin-top: 16px; padding-top: 12px; }
QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; color: @muted; }
)";
    const QList<QPair<QString, QString>> vars = {
        {"@bgFill", bgFill}, {"@panelFill", panelFill}, {"@btnFill", btnFill}, {"@playFill", playFill},
        {"@titleFill", titleFill}, {"@logoBg", logoBg}, {"@panel2", p.panel2.name()}, {"@panel", p.panel.name()},
        {"@bg", p.bg.name()}, {"@border", p.border.name()}, {"@text", p.text.name()}, {"@muted", p.muted.name()},
        {"@accent", p.accent.name()}};
    for (const auto& v : vars) s.replace(v.first, v.second);
    return s;
}

void Theme::apply() {
    QSettings st;
    g_notice.clear();
    const QString id = st.value("theme", "grafite").toString();
    Palette p = builtins().first();
    for (const auto& b : builtins()) if (b.id == id) p = b;

    // "normal" with "all" variables: transfers to themes/all (and applies the variables anyway)
    Parsed normal, all;
    const QString nFile = st.value("themeNormal").toString();
    const QString nPath = pathOf(Normal, nFile);
    if (!nPath.isEmpty()) {
        const QString raw = readFile(nPath);
        normal = parseCss(raw);
        if (normal.hadVars) transferToAll(nPath, raw, nFile);
    }
    const QString aPath = pathOf(All, st.value("themeAll").toString());
    if (!aPath.isEmpty()) all = parseCss(readFile(aPath));

    applyVars(p, normal.vars);
    applyVars(p, all.vars);
    g_cur = p;
    g_set = true;

    qApp->setStyleSheet(buildStyle(p, gradientOn()) + "\n" + all.qss + "\n" + normal.qss);   // base < all < normal
}