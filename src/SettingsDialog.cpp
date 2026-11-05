#include "SettingsDialog.h"
#include "I18n.h"
#include "Theme.h"
#include <QCheckBox>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGroupBox>
#include <QLineEdit>
#include <QStandardPaths>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSysInfo>
#include <QTabWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

static QPushButton* btn(const QString& t, QWidget* parent = nullptr) { return new QPushButton(t, parent); }

SettingsDialog::SettingsDialog(EmulatorManager* emu, Metadata* meta, Igdb* igdb, QWidget* parent, int tab)
    : QDialog(parent), emu_(emu), meta_(meta), igdb_(igdb) {
    setWindowTitle(T("Settings"));
    resize(940, 680);
    auto* l = new QVBoxLayout(this);
    tabs_ = new QTabWidget;
    tabs_->addTab(buildGeneral(), T("General"));
    tabs_->addTab(buildFolders(), T("Folders"));
    tabs_->addTab(buildEmus(), T("Emulators"));
    tabs_->addTab(buildAppearance(), T("Appearance"));
    tabs_->addTab(buildMeta(), T("Metadata"));
    tabs_->addTab(buildAdvanced(), T("Advanced"));
    tabs_->setCurrentIndex(tab);
    l->addWidget(tabs_, 1);

    auto* row = new QHBoxLayout;
    barLbl_ = new QLabel; barLbl_->setObjectName("muted");
    bar_ = new QProgressBar; bar_->setMaximumWidth(260); bar_->hide();
    auto* close = btn(T("Close"));
    row->addWidget(barLbl_, 1); row->addWidget(bar_); row->addWidget(close);
    l->addLayout(row);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    connect(emu_, &EmulatorManager::status, this, [this](const QString& m) { barLbl_->setText(m); });
    connect(emu_, &EmulatorManager::progress, this, [this](int p) {
        bar_->setVisible(p < 100);
        bar_->setRange(0, p < 0 ? 0 : 100);
        if (p >= 0) bar_->setValue(p);
    });
    connect(emu_, &EmulatorManager::finished, this, [this](bool ok, const QString& m) {
        barLbl_->setText(m);
        if (ok) latest_.clear();
        refreshEmuTable();
        emit emulatorsChanged();
    });
    connect(emu_, &EmulatorManager::updateInfo, this, [this](const QString& id, const QString& ver, int st) {
        latest_[id] = st == 1 ? T("↑ new version: %1").arg(ver) : st == 0 ? T("✔ up to date") : T("unknown version (latest: %1)").arg(ver);
        refreshEmuTable();
    });
    connect(emu_, &EmulatorManager::updatesChecked, this, [this](int n) {
        barLbl_->setText(n > 0 ? T("%1 update(s) available.").arg(n) : T("Everything is up to date."));
    });
}

// ---------------- General ----------------
QWidget* SettingsDialog::buildGeneral() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    QSettings s;

    auto* lang = new QComboBox;
    lang->addItem("Português (Brasil)", "pt");
    lang->addItem("English", "en");
    lang->setCurrentIndex(lang->findData(I18n::lang()));
    auto* rowL = new QHBoxLayout; rowL->addWidget(new QLabel(T("Language:"))); rowL->addWidget(lang, 1);
    l->addLayout(rowL);
    connect(lang, &QComboBox::currentIndexChanged, this, [this, lang] {
        I18n::setLang(lang->currentData().toString());
        QMessageBox::information(this, "EmuCenter", T("Restart EmuCenter to apply the language."));
    });

    auto check = [&](const QString& text, const char* key, bool def, const QString& tip = {}) {
        auto* c = new QCheckBox(text);
        c->setChecked(s.value(key, def).toBool());
        if (!tip.isEmpty()) c->setToolTip(tip);
        connect(c, &QCheckBox::toggled, this, [this, key](bool v) { QSettings().setValue(key, v); emit generalChanged(); });
        l->addWidget(c);
        return c;
    };
    check(T("Custom title bar (buttons embedded in the top) — restart to apply"), "frameless", true);
    check(T("Continue in background (tray) when closing the window"), "background", true);
    check(T("Show notifications (metadata ready, emulator updates)"), "notify", true);
    check(T("Check for emulator updates on startup"), "autoEmuUpdate", false);

    l->addSpacing(12);
    auto* arch = new QComboBox;
    arch->addItem(T("Automatic"), "auto"); arch->addItem("x64 (Intel/AMD)", "x64"); arch->addItem("ARM64", "arm64");
    arch->setCurrentIndex(arch->findData(s.value("arch", "auto")));
    connect(arch, &QComboBox::currentIndexChanged, this, [this, arch] { QSettings().setValue("arch", arch->currentData()); refreshEmuTable(); });
    auto* rowA = new QHBoxLayout;
    rowA->addWidget(new QLabel(T("Emulator packages for:"))); rowA->addWidget(arch, 1);
    l->addLayout(rowA);
    auto* det = new QLabel(T("Detected processor: %1  •  using: %2").arg(QSysInfo::currentCpuArchitecture(), emu_->arch()));
    det->setObjectName("muted");
    l->addWidget(det);
    l->addStretch();
    return w;
}

// ---------------- Folders ----------------
void SettingsDialog::refreshRomList() {
    romList_->clear();
    romList_->addItems(QSettings().value("romDirs").toStringList());
}

QWidget* SettingsDialog::buildFolders() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    l->addWidget(new QLabel(T("Folders containing your ROMs (the app searches subfolders too):")));
    romList_ = new QListWidget;
    l->addWidget(romList_, 1);
    refreshRomList();
    auto* row = new QHBoxLayout;
    auto* add = btn(T("+ Add folder")); auto* rem = btn(T("Remove selected"));
    row->addWidget(add); row->addWidget(rem); row->addStretch();
    l->addLayout(row);
    connect(add, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, T("Choose the folder with your ROMs"));
        if (d.isEmpty()) return;
        QSettings s; QStringList v = s.value("romDirs").toStringList();
        if (!v.contains(d)) v << d;
        s.setValue("romDirs", v);
        refreshRomList(); emit romFoldersChanged();
    });
    connect(rem, &QPushButton::clicked, this, [this] {
        auto* it = romList_->currentItem();
        if (!it) return;
        QSettings s; QStringList v = s.value("romDirs").toStringList();
        v.removeAll(it->text());
        s.setValue("romDirs", v);
        refreshRomList(); emit romFoldersChanged();
    });

    l->addSpacing(14);
    l->addWidget(new QLabel(T("Emulators base folder (each emulator goes into a separate subfolder):")));
    rootLbl_ = new QLabel(emu_->root());
    rootLbl_->setObjectName("muted");
    auto* r2 = new QHBoxLayout;
    auto* ch = btn(T("Change…")); auto* op = btn(T("Open folder"));
    r2->addWidget(rootLbl_, 1); r2->addWidget(ch); r2->addWidget(op);
    l->addLayout(r2);
    l->addWidget(new QLabel(T("Tip: each emulator's folder can also be chosen individually in the Emulators tab.")));
    connect(ch, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, T("Choose the emulators base folder"), emu_->root());
        if (d.isEmpty()) return;
        emu_->setRoot(d); rootLbl_->setText(d); refreshEmuTable(); emit emulatorsChanged();
    });
    connect(op, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(emu_->root())); });
    return w;
}

// ---------------- Emulators ----------------
QWidget* SettingsDialog::buildEmus() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    auto* top = new QHBoxLayout;
    auto* chk = btn(T("Check for updates"));
    top->addWidget(new QLabel(T("Click on the emulator name to open its official website.")), 1);
    top->addWidget(chk);
    l->addLayout(top);
    emuTable_ = new QTableWidget(Catalog::emulators().size(), 4);
    emuTable_->setHorizontalHeaderLabels({T("Emulator"), T("Version / status"), T("Folder"), T("Actions")});
    emuTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    emuTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    emuTable_->verticalHeader()->hide();
    emuTable_->verticalHeader()->setDefaultSectionSize(54);
    emuTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    emuTable_->setSelectionMode(QAbstractItemView::NoSelection);
    emuTable_->setWordWrap(false);
    emuTable_->setTextElideMode(Qt::ElideMiddle);
    l->addWidget(emuTable_, 1);
    connect(emuTable_, &QTableWidget::cellClicked, this, [this](int row, int col) {
        if (col == 0) QDesktopServices::openUrl(QUrl(Catalog::emulators().at(row).homepage));
    });
    connect(chk, &QPushButton::clicked, this, [this] { barLbl_->setText(T("Checking…")); emu_->checkAllUpdates(); });
    refreshEmuTable();
    return w;
}

void SettingsDialog::refreshEmuTable() {
    if (!emuTable_) return;
    int r = 0;
    for (const auto& e : Catalog::emulators()) {
        const QString exe = emu_->findExe(e.id);
        auto* name = new QTableWidgetItem(e.name);
        QFont f = name->font(); f.setUnderline(true); name->setFont(f);
        name->setForeground(Theme::current().accent);
        name->setToolTip(e.homepage);
        emuTable_->setItem(r, 0, name);

        QString st = exe.isEmpty() ? T("not installed") : (emu_->installedVersion(e.id).isEmpty() ? T("installed") : emu_->installedVersion(e.id));
        if (latest_.contains(e.id)) st += "    " + latest_[e.id];
        auto* sti = new QTableWidgetItem(st);
        sti->setForeground(exe.isEmpty() ? Theme::current().err : Theme::current().ok);
        emuTable_->setItem(r, 1, sti);

        auto* dir = new QTableWidgetItem(emu_->emuDir(e.id));
        dir->setToolTip(exe);
        emuTable_->setItem(r, 2, dir);

        auto* cell = new QWidget; auto* h = new QHBoxLayout(cell); h->setContentsMargins(4, 4, 4, 4);
        const bool upd = latest_.value(e.id).startsWith(QString::fromUtf8("↑"));
        auto* dl = btn(exe.isEmpty() ? T("Download") : (upd ? T("Update") : T("Reinstall")));
        auto* im = btn(T("Import…")); auto* fo = btn(T("Folder…"));
        h->addWidget(dl); h->addWidget(im); h->addWidget(fo);
        const QString id = e.id;
        connect(dl, &QPushButton::clicked, this, [this, id] { emu_->install(id, {}, true); });
        connect(im, &QPushButton::clicked, this, [this, id] {
            const QString p = QFileDialog::getOpenFileName(this, T("Choose the executable"));
            if (p.isEmpty()) return;
            emu_->importExe(id, p); refreshEmuTable(); emit emulatorsChanged();
        });
        connect(fo, &QPushButton::clicked, this, [this, id] {
            const QString d = QFileDialog::getExistingDirectory(this, T("Folder for this emulator"), emu_->emuDir(id));
            if (d.isEmpty()) return;
            emu_->setEmuDir(id, d); refreshEmuTable(); emit emulatorsChanged();
        });
        emuTable_->setCellWidget(r, 3, cell);
        ++r;
    }
    emuTable_->resizeColumnToContents(0);
    emuTable_->resizeColumnToContents(3);
}

// ---------------- Appearance ----------------
void SettingsDialog::refreshThemeButtons() {
    auto clear = [](QGridLayout* g) {
        while (QLayoutItem* it = g->takeAt(0)) { delete it->widget(); delete it; }
    };
    clear(paletteGrid_); clear(allGrid_); clear(normalGrid_);
    QSettings s;
    auto applyNow = [this] {
        Theme::apply();
        refreshThemeButtons();
        if (!Theme::notice().isEmpty()) barLbl_->setText(Theme::notice());
        emit appearanceChanged();
    };
    int i = 0;
    for (const auto& p : Theme::builtins()) {
        auto* b = btn(p.name); b->setCheckable(true); b->setChecked(s.value("theme", "grafite") == p.id);
        const QString id = p.id;
        connect(b, &QPushButton::clicked, this, [applyNow, id] { QSettings().setValue("theme", id); applyNow(); });
        paletteGrid_->addWidget(b, i / 3, i % 3); ++i;
    }
    auto fill = [&](QGridLayout* g, Theme::Kind kind, const char* key) {
        int n = 0;
        auto add = [&](const QString& label, const QString& file) {
            auto* b = btn(label); b->setCheckable(true); b->setChecked(s.value(key).toString() == file);
            connect(b, &QPushButton::clicked, this, [applyNow, key, file] { QSettings().setValue(key, file); applyNow(); });
            g->addWidget(b, n / 3, n % 3); ++n;
        };
        add(T("None"), QString());
        for (const QString& f : Theme::list(kind)) add(QFileInfo(f).completeBaseName(), f);
    };
    fill(allGrid_, Theme::All, "themeAll");
    fill(normalGrid_, Theme::Normal, "themeNormal");
}

QWidget* SettingsDialog::buildAppearance() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    l->addWidget(new QLabel(T("Base theme")));
    paletteGrid_ = new QGridLayout; l->addLayout(paletteGrid_);

    auto* grad = new QCheckBox(T("Use gradient on panels and buttons"));
    grad->setChecked(QSettings().value("gradient", true).toBool());
    connect(grad, &QCheckBox::toggled, this, [this](bool v) { QSettings().setValue("gradient", v); Theme::apply(); emit appearanceChanged(); });
    l->addWidget(grad);

    l->addSpacing(8);
    l->addWidget(new QLabel(T("Complete themes — themes/all (change EVERYTHING: colors, game list, groups, dividers, gradient, logo)")));
    allGrid_ = new QGridLayout; l->addLayout(allGrid_);
    l->addSpacing(8);
    l->addWidget(new QLabel(T("Button and font packages — themes/normal (applied on top; work together with complete themes)")));
    normalGrid_ = new QGridLayout; l->addLayout(normalGrid_);

    auto* row = new QHBoxLayout;
    auto* imp = btn(T("Import theme (.css / .qss)…"));
    auto* oa = btn(T("Open themes/all")); auto* on = btn(T("Open themes/normal"));
    row->addWidget(imp); row->addWidget(oa); row->addWidget(on); row->addStretch();
    l->addSpacing(8); l->addLayout(row);
    auto* hint = new QLabel(T("The app automatically detects whether the file is complete (has :root with variables) or just buttons. "
                            "If a normal file has complete theme variables, that part is moved to themes/all."));
    hint->setObjectName("muted"); hint->setWordWrap(true);
    l->addWidget(hint);
    l->addStretch();
    refreshThemeButtons();

    connect(imp, &QPushButton::clicked, this, [this] {
        const QStringList fs = QFileDialog::getOpenFileNames(this, T("Import theme"), {}, "CSS (*.css *.qss)");
        QStringList msg;
        for (const QString& f : fs) msg << QFileInfo(f).fileName() + " → " + (Theme::importFile(f) == Theme::All ? "themes/all" : "themes/normal");
        if (!msg.isEmpty()) barLbl_->setText(T("Imported: ") + msg.join(", "));
        refreshThemeButtons();
    });
    auto open = [](Theme::Kind k) {
        QString dir = Theme::roots().last() + (k == Theme::All ? "/all" : "/normal");   // folder next to .exe, if it exists
        if (!QDir(dir).exists()) { dir = Theme::importDir(k); QDir().mkpath(dir); }
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    };
    connect(oa, &QPushButton::clicked, this, [open] { open(Theme::All); });
    connect(on, &QPushButton::clicked, this, [open] { open(Theme::Normal); });
    return w;
}

// ---------------- Metadata + IGDB ----------------
QWidget* SettingsDialog::buildMeta() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    QSettings s;
    auto chk = [&](const QString& text, const char* key, bool def, QLayout* lay) {
        auto* c = new QCheckBox(text);
        c->setChecked(s.value(key, def).toBool());
        connect(c, &QCheckBox::toggled, this, [this, key](bool v) { QSettings().setValue(key, v); emit generalChanged(); });
        lay->addWidget(c);
    };
    chk(T("Download metadata automatically (on startup and when adding games), in the background"), "autoMeta", true, l);
    chk(T("Use the name stored inside the ROM (instead of the filename)"), "useInternalName", true, l);

    auto* hrs = new QSpinBox; hrs->setRange(0, 720); hrs->setSpecialValueText(T("disabled")); hrs->setSuffix(T(" h"));
    hrs->setValue(s.value("metaHours", 0).toInt());
    connect(hrs, &QSpinBox::valueChanged, this, [this](int v) { QSettings().setValue("metaHours", v); emit generalChanged(); });
    auto* rh = new QHBoxLayout; rh->addWidget(new QLabel(T("Repeat check every:"))); rh->addWidget(hrs); rh->addStretch();
    l->addLayout(rh);

    auto* box = new QGroupBox(T("IGDB (names, box arts, banners and descriptions)"));
    auto* bl = new QVBoxLayout(box);
    auto* info = new QLabel(T("Create a free app at <a href=\"https://dev.twitch.tv/console\">dev.twitch.tv/console</a> and paste the Client ID and Client Secret.")); 
    info->setOpenExternalLinks(true); info->setWordWrap(true);
    bl->addWidget(info);
    auto* id = new QLineEdit(s.value("igdb/clientId").toString()); id->setPlaceholderText("Client ID");
    auto* sec = new QLineEdit(s.value("igdb/clientSecret").toString()); sec->setPlaceholderText("Client Secret"); sec->setEchoMode(QLineEdit::Password);
    connect(id, &QLineEdit::textChanged, this, [](const QString& v) { QSettings s; s.setValue("igdb/clientId", v.trimmed()); s.remove("igdb/token"); });
    connect(sec, &QLineEdit::textChanged, this, [](const QString& v) { QSettings s; s.setValue("igdb/clientSecret", v.trimmed()); s.remove("igdb/token"); });
    bl->addWidget(id); bl->addWidget(sec);
    chk(T("Use IGDB for images and descriptions"), "useIgdb", true, bl);
    chk(T("Use IGDB name as the game name"), "useIgdbName", true, bl);
    auto* test = btn(T("Test connection"));
    auto* res = new QLabel; res->setObjectName("muted"); res->setWordWrap(true);
    auto* tr = new QHBoxLayout; tr->addWidget(test); tr->addWidget(res, 1);
    bl->addLayout(tr);
    connect(test, &QPushButton::clicked, this, [this, res] {
        res->setText(T("Testing…"));
        igdb_->test([res](bool ok, QString m) { res->setText((ok ? QString::fromUtf8("✔ ") : QString::fromUtf8("✖ ")) + m); });
    });
    l->addWidget(box);

    l->addSpacing(6);
    auto* now = btn(T("Download / update metadata now"));
    auto* retry = btn(T("Retry failed ones"));
    auto* clr = btn(T("Clear cache"));
    auto* rb = new QHBoxLayout; rb->addWidget(now); rb->addWidget(retry); rb->addWidget(clr); rb->addStretch();
    l->addLayout(rb);
    cacheLbl_ = new QLabel; cacheLbl_->setObjectName("muted");
    auto upd = [this] { cacheLbl_->setText(T("Cache: %1 MB in %2").arg(meta_->cacheBytes() / 1048576.0, 0, 'f', 1).arg(meta_->cacheDir())); };
    upd();
    l->addWidget(cacheLbl_);
    l->addStretch();
    connect(now, &QPushButton::clicked, this, [this] { emit metadataRequested(false); barLbl_->setText(T("Downloading metadata…")); });
    connect(retry, &QPushButton::clicked, this, [this] { emit metadataRequested(true); barLbl_->setText(T("Downloading metadata…")); });
    connect(clr, &QPushButton::clicked, this, [this, upd] { meta_->clearCache(); upd(); });
    return w;
}

// ---------------- Advanced: when and how things start ----------------
QWidget* SettingsDialog::buildAdvanced() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    QSettings s;

    auto* g1 = new QGroupBox(T("Startup")); auto* v1 = new QVBoxLayout(g1);
    auto chk = [&](const QString& text, const char* key, bool def, QLayout* lay) {
        auto* c = new QCheckBox(text);
        c->setChecked(s.value(key, def).toBool());
        connect(c, &QCheckBox::toggled, this, [this, key](bool v) { QSettings().setValue(key, v); emit generalChanged(); });
        lay->addWidget(c);
        return c;
    };
    auto* os = chk(T("Start EmuCenter along with Windows"), "startWithOS", false, v1);
#ifdef Q_OS_WIN
    connect(os, &QCheckBox::toggled, this, [](bool on) {
        QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        if (on) run.setValue("EmuCenter", "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\"");
        else run.remove("EmuCenter");
    });
#else
    os->setEnabled(false);
#endif
    chk(T("Start minimized in the tray"), "startMinimized", false, v1);

    auto spin = [&](const QString& label, const char* key, int def, int max, QLayout* lay) {
        auto* sp = new QSpinBox; sp->setRange(0, max); sp->setSuffix(T(" s")); sp->setValue(s.value(key, def).toInt());
        connect(sp, &QSpinBox::valueChanged, this, [key](int v) { QSettings().setValue(key, v); });
        auto* r = new QHBoxLayout; r->addWidget(new QLabel(label), 1); r->addWidget(sp);
        lay->addItem(r);
    };
    spin(T("Wait before starting metadata download:"), "startDelayMeta", 3, 600, v1);
    spin(T("Wait before checking emulator updates:"), "startDelayUpdate", 10, 600, v1);
    l->addWidget(g1);

    auto* g2 = new QGroupBox(T("Network")); auto* v2 = new QVBoxLayout(g2);
    auto* par = new QSpinBox; par->setRange(1, 8); par->setValue(s.value("net/parallel", 4).toInt());
    connect(par, &QSpinBox::valueChanged, this, [](int v) { QSettings().setValue("net/parallel", v); });
    auto* r = new QHBoxLayout; r->addWidget(new QLabel(T("Simultaneous metadata downloads:")), 1); r->addWidget(par);
    v2->addLayout(r);
    l->addWidget(g2);

    auto* g3 = new QGroupBox(T("Maintenance")); auto* v3 = new QVBoxLayout(g3);
    auto* data = btn(T("Open data folder"));
    auto* rst = btn(T("Restore default theme"));
    v3->addWidget(data); v3->addWidget(rst); v3->addStretch();
    l->addWidget(g3);
    connect(data, &QPushButton::clicked, this, [] {
        const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(d); QDesktopServices::openUrl(QUrl::fromLocalFile(d));
    });
    connect(rst, &QPushButton::clicked, this, [this] {
        QSettings s; s.setValue("theme", "grafite"); s.setValue("themeAll", ""); s.setValue("themeNormal", ""); s.setValue("gradient", true);
        Theme::apply(); refreshThemeButtons(); emit appearanceChanged();
    });
    l->addWidget(new QLabel(T("Startup changes take effect the next time the app opens.")));
    l->addStretch();
    return w;
}