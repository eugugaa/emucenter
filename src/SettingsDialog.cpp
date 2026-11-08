#include "SettingsDialog.h"
#include "DiscordRpc.h"
#include "I18n.h"
#include "Theme.h"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTabWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

static QPushButton* btn(const QString& t) { return new QPushButton(t); }

static QWidget* scrolled(QWidget* inner) {
    auto* sa = new QScrollArea;
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setWidget(inner);
    sa->viewport()->setAutoFillBackground(false);
    inner->setAutoFillBackground(false);
    sa->setStyleSheet("QScrollArea { background: transparent; } QScrollArea > QWidget > QWidget { background: transparent; }");
    return sa;
}

SettingsDialog::SettingsDialog(EmulatorManager* emu, Metadata* meta, Igdb* igdb, Updater* updater, QWidget* parent, int tab)
    : QDialog(parent), emu_(emu), meta_(meta), igdb_(igdb), updater_(updater) {
    setWindowTitle(T("Configurações"));
    resize(980, 720);
    auto* l = new QVBoxLayout(this);
    tabs_ = new QTabWidget;
    tabs_->addTab(scrolled(buildGeneral()), T("Geral"));
    tabs_->addTab(buildFolders(), T("Pastas"));
    tabs_->addTab(buildEmus(), T("Emuladores"));
    tabs_->addTab(scrolled(buildAppearance()), T("Aparência"));
    tabs_->addTab(scrolled(buildMeta()), T("Metadados"));
    tabs_->addTab(scrolled(buildAdvanced()), T("Avançado"));
    tabs_->setCurrentIndex(tab);
    l->addWidget(tabs_, 1);

    auto* row = new QHBoxLayout;
    barLbl_ = new QLabel;
    barLbl_->setObjectName("muted");
    bar_ = new QProgressBar;
    bar_->setMaximumWidth(260);
    bar_->hide();
    auto* close = btn(T("Fechar"));
    row->addWidget(barLbl_, 1);
    row->addWidget(bar_);
    row->addWidget(close);
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
        latest_[id] = st == 1 ? T("↑ nova versão: %1").arg(ver) : st == 0 ? T("✔ em dia") : T("versão desconhecida (última: %1)").arg(ver);
        refreshEmuTable();
    });
    connect(emu_, &EmulatorManager::updatesChecked, this, [this](int n) {
        barLbl_->setText(n > 0 ? T("%1 atualização(ões) disponível(is).").arg(n) : T("Tudo atualizado."));
    });
    connect(updater_, &Updater::checked, this, [this](bool, bool newer, const QString&, const QString& msg) {
        updLbl_->setText(msg);
        updBtn_->setVisible(newer);
    });
    connect(updater_, &Updater::progress, this, [this](int p, const QString& t) {
        barLbl_->setText(t);
        bar_->setVisible(p < 100);
        bar_->setRange(0, p < 0 ? 0 : 100);
        if (p >= 0) bar_->setValue(p);
    });
    connect(updater_, &Updater::failed, this, [this](const QString& m) { barLbl_->setText(m); updLbl_->setText(m); });
}

void SettingsDialog::applyUi() {
    Theme::apply();
    emit appearanceChanged();
}

QWidget* SettingsDialog::buildGeneral() {
    auto* w = new QWidget;
    auto* l = new QVBoxLayout(w);
    QSettings s;

    auto* lang = new QComboBox;
    const auto langs = I18n::languages();
    for (const auto& x : langs) lang->addItem(x.second, x.first);
    lang->setCurrentIndex(qMax(0, lang->findData(I18n::lang())));
    auto* rowL = new QHBoxLayout;
    rowL->addWidget(new QLabel(T("Idioma:")));
    rowL->addWidget(lang, 1);
    l->addLayout(rowL);
    connect(lang, &QComboBox::currentIndexChanged, this, [this, lang] {
        I18n::setLang(lang->currentData().toString());
        emit languageChanged();
    });

    auto check = [this, l](const QString& text, const char* key, bool def) {
        auto* c = new QCheckBox(text);
        c->setChecked(QSettings().value(key, def).toBool());
        connect(c, &QCheckBox::toggled, this, [this, key](bool v) { QSettings().setValue(key, v); emit generalChanged(); });
        l->addWidget(c);
    };
    check(T("Barra de título personalizada (botões embutidos no topo) — reinicie para aplicar"), "frameless", true);
    check(T("Continuar em segundo plano (bandeja) ao fechar a janela"), "background", true);
    check(T("Mostrar notificações (metadados prontos, atualizações de emuladores)"), "notify", true);
    check(T("Verificar atualizações dos emuladores ao iniciar"), "autoEmuUpdate", false);
    check(T("Lembrar tamanho e posição da janela"), "rememberGeometry", true);
    check(T("Abrir direto no Big Picture"), "startBigPicture", false);
    check(T("Mostrar o tempo de jogo na lista"), "showPlaytimeInList", false);

    auto* sort = new QComboBox;
    sort->addItem(T("Nome (A-Z)"), "name");
    sort->addItem(T("Jogados recentemente"), "recent");
    sort->addItem(T("Mais jogados"), "playtime");
    sort->setCurrentIndex(qMax(0, sort->findData(s.value("sortOrder", "name"))));
    connect(sort, &QComboBox::currentIndexChanged, this, [this, sort] { QSettings().setValue("sortOrder", sort->currentData()); emit romFoldersChanged(); });
    auto* rs = new QHBoxLayout;
    rs->addWidget(new QLabel(T("Ordenar jogos por:")));
    rs->addWidget(sort, 1);
    l->addLayout(rs);

    auto* arch = new QComboBox;
    arch->addItem(T("Automático"), "auto");
    arch->addItem("x64 (Intel/AMD)", "x64");
    arch->addItem("ARM64", "arm64");
    arch->setCurrentIndex(qMax(0, arch->findData(s.value("arch", "auto"))));
    connect(arch, &QComboBox::currentIndexChanged, this, [this, arch] { QSettings().setValue("arch", arch->currentData()); refreshEmuTable(); });
    auto* rowA = new QHBoxLayout;
    rowA->addWidget(new QLabel(T("Pacotes de emulador para:")));
    rowA->addWidget(arch, 1);
    l->addLayout(rowA);
    auto* det = new QLabel(T("Processador detectado: %1  •  usando: %2").arg(QSysInfo::currentCpuArchitecture(), emu_->arch()));
    det->setObjectName("muted");
    l->addWidget(det);

    auto* gu = new QGroupBox(T("Atualizações do EmuCenter"));
    auto* vu = new QVBoxLayout(gu);
    auto* ver = new QLabel(T("Versão instalada: %1").arg(Updater::currentVersion()));
    ver->setObjectName("heading");
    updLbl_ = new QLabel;
    updLbl_->setObjectName("muted");
    updLbl_->setWordWrap(true);
    auto* chk = btn(T("Verificar agora"));
    updBtn_ = btn(T("Baixar e instalar"));
    updBtn_->setObjectName("play");
    updBtn_->setVisible(updater_->updateAvailable());
    auto* ru = new QHBoxLayout;
    ru->addWidget(chk);
    ru->addWidget(updBtn_);
    ru->addStretch(1);
    vu->addWidget(ver);
    vu->addWidget(updLbl_);
    vu->addLayout(ru);
    auto uchk = [this, vu](const QString& text, const char* key, bool def) {
        auto* c = new QCheckBox(text);
        c->setChecked(QSettings().value(key, def).toBool());
        connect(c, &QCheckBox::toggled, this, [key](bool v) { QSettings().setValue(key, v); });
        vu->addWidget(c);
    };
    uchk(T("Verificar atualizações ao iniciar"), "autoUpdateCheck", true);
    uchk(T("Incluir versões de teste (pré-lançamentos)"), "updatePre", false);
    l->addWidget(gu);
    connect(chk, &QPushButton::clicked, this, [this] {
        updLbl_->setText(T("Verificando…"));
        updater_->check(QSettings().value("updatePre", false).toBool());
    });
    connect(updBtn_, &QPushButton::clicked, this, [this] { updBtn_->setEnabled(false); updater_->install(); });

    auto* wiz = btn(T("Abrir o assistente de configuração"));
    connect(wiz, &QPushButton::clicked, this, &SettingsDialog::welcomeRequested);
    l->addWidget(wiz, 0, Qt::AlignLeft);
    l->addStretch(1);
    return w;
}

void SettingsDialog::refreshRomList() {
    romList_->clear();
    romList_->addItems(QSettings().value("romDirs").toStringList());
}

QWidget* SettingsDialog::buildFolders() {
    auto* w = new QWidget;
    auto* l = new QVBoxLayout(w);
    l->addWidget(new QLabel(T("Pastas com as suas ROMs (o app procura nas subpastas também):")));
    romList_ = new QListWidget;
    l->addWidget(romList_, 1);
    refreshRomList();
    auto* row = new QHBoxLayout;
    auto* add = btn(T("+ Adicionar pasta"));
    auto* rem = btn(T("Remover selecionada"));
    row->addWidget(add);
    row->addWidget(rem);
    row->addStretch();
    l->addLayout(row);
    connect(add, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, T("Escolha a pasta com suas ROMs"));
        if (d.isEmpty()) return;
        QSettings s;
        QStringList v = s.value("romDirs").toStringList();
        if (!v.contains(d)) v << d;
        s.setValue("romDirs", v);
        refreshRomList();
        emit romFoldersChanged();
    });
    connect(rem, &QPushButton::clicked, this, [this] {
        auto* it = romList_->currentItem();
        if (!it) return;
        QSettings s;
        QStringList v = s.value("romDirs").toStringList();
        v.removeAll(it->text());
        s.setValue("romDirs", v);
        refreshRomList();
        emit romFoldersChanged();
    });

    l->addSpacing(14);
    l->addWidget(new QLabel(T("Pasta-base dos emuladores (cada emulador fica em uma subpasta separada):")));
    rootLbl_ = new QLabel(emu_->root());
    rootLbl_->setObjectName("mono");
    auto* r2 = new QHBoxLayout;
    auto* ch = btn(T("Alterar…"));
    auto* op = btn(T("Abrir pasta"));
    r2->addWidget(rootLbl_, 1);
    r2->addWidget(ch);
    r2->addWidget(op);
    l->addLayout(r2);
    l->addWidget(new QLabel(T("Dica: a pasta de cada emulador também pode ser escolhida individualmente na aba Emuladores.")));
    connect(ch, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, T("Escolha a pasta-base dos emuladores"), emu_->root());
        if (d.isEmpty()) return;
        emu_->setRoot(d);
        rootLbl_->setText(d);
        refreshEmuTable();
        emit emulatorsChanged();
    });
    connect(op, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(emu_->root())); });
    return w;
}

QWidget* SettingsDialog::buildEmus() {
    auto* w = new QWidget;
    auto* l = new QVBoxLayout(w);
    auto* top = new QHBoxLayout;
    auto* chk = btn(T("Verificar atualizações"));
    top->addWidget(new QLabel(T("Clique no nome do emulador para abrir o site oficial.")), 1);
    top->addWidget(chk);
    l->addLayout(top);
    emuTable_ = new QTableWidget(Catalog::emulators().size(), 4);
    emuTable_->setHorizontalHeaderLabels({T("Emulador"), T("Versão / estado"), T("Pasta"), T("Ações")});
    emuTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    emuTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    emuTable_->verticalHeader()->hide();
    emuTable_->verticalHeader()->setDefaultSectionSize(54);
    emuTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    emuTable_->setSelectionMode(QAbstractItemView::NoSelection);
    emuTable_->setWordWrap(false);
    emuTable_->setTextElideMode(Qt::ElideMiddle);
    l->addWidget(emuTable_, 1);
    connect(emuTable_, &QTableWidget::cellClicked, this, [](int row, int col) {
        if (col == 0) QDesktopServices::openUrl(QUrl(Catalog::emulators().at(row).homepage));
    });
    connect(chk, &QPushButton::clicked, this, [this] { barLbl_->setText(T("Verificando…")); emu_->checkAllUpdates(); });
    refreshEmuTable();
    return w;
}

void SettingsDialog::refreshEmuTable() {
    if (!emuTable_) return;
    int r = 0;
    for (const auto& e : Catalog::emulators()) {
        const QString exe = emu_->findExe(e.id);
        auto* name = new QTableWidgetItem(e.name);
        QFont f = name->font();
        f.setUnderline(true);
        name->setFont(f);
        name->setForeground(Theme::current().accent);
        name->setToolTip(e.homepage);
        emuTable_->setItem(r, 0, name);

        QString st = exe.isEmpty() ? T("não instalado") : (emu_->installedVersion(e.id).isEmpty() ? T("instalado") : emu_->installedVersion(e.id));
        if (latest_.contains(e.id)) st += "   " + latest_[e.id];
        auto* sti = new QTableWidgetItem(st);
        sti->setForeground(exe.isEmpty() ? Theme::current().err : Theme::current().ok);
        emuTable_->setItem(r, 1, sti);

        auto* dir = new QTableWidgetItem(emu_->emuDir(e.id));
        dir->setToolTip(exe);
        emuTable_->setItem(r, 2, dir);

        auto* cell = new QWidget;
        auto* h = new QHBoxLayout(cell);
        h->setContentsMargins(4, 4, 4, 4);
        const bool upd = latest_.value(e.id).startsWith(QString::fromUtf8("↑"));
        const bool autoSrc = EmulatorManager::hasAutoSource(e);
        auto* dl = btn(exe.isEmpty() ? (autoSrc ? T("Baixar") : T("Site")) : (upd ? T("Atualizar") : T("Reinstalar")));
        auto* im = btn(T("Importar…"));
        auto* fo = btn(T("Pasta…"));
        h->addWidget(dl);
        h->addWidget(im);
        h->addWidget(fo);
        const QString id = e.id;
        const QString home = e.homepage;
        connect(dl, &QPushButton::clicked, this, [this, id, home, autoSrc] {
            if (autoSrc) emu_->install(id, {}, true);
            else QDesktopServices::openUrl(QUrl(home));
        });
        connect(im, &QPushButton::clicked, this, [this, id] {
            const QString p = QFileDialog::getOpenFileName(this, T("Escolha o executável"));
            if (p.isEmpty()) return;
            emu_->importExe(id, p);
            refreshEmuTable();
            emit emulatorsChanged();
        });
        connect(fo, &QPushButton::clicked, this, [this, id] {
            const QString d = QFileDialog::getExistingDirectory(this, T("Pasta deste emulador"), emu_->emuDir(id));
            if (d.isEmpty()) return;
            emu_->setEmuDir(id, d);
            refreshEmuTable();
            emit emulatorsChanged();
        });
        emuTable_->setCellWidget(r, 3, cell);
        ++r;
    }
    emuTable_->resizeColumnToContents(0);
    emuTable_->resizeColumnToContents(3);
}

void SettingsDialog::refreshThemeButtons() {
    auto clear = [](QGridLayout* g) {
        while (QLayoutItem* it = g->takeAt(0)) { delete it->widget(); delete it; }
    };
    clear(paletteGrid_);
    clear(allGrid_);
    clear(normalGrid_);
    QSettings s;
    auto applyNow = [this] {
        Theme::apply();
        refreshThemeButtons();
        if (!Theme::notice().isEmpty()) barLbl_->setText(Theme::notice());
        emit appearanceChanged();
    };
    int i = 0;
    const QList<Palette> pals = Theme::builtins();
    for (const auto& p : pals) {
        auto* b = btn(I18n::tr(p.name));
        b->setCheckable(true);
        b->setChecked(s.value("theme", "grafite") == p.id);
        const QString id = p.id;
        connect(b, &QPushButton::clicked, this, [applyNow, id] { QSettings().setValue("theme", id); applyNow(); });
        paletteGrid_->addWidget(b, i / 3, i % 3);
        ++i;
    }
    auto fill = [&](QGridLayout* g, Theme::Kind kind, const char* key) {
        int n = 0;
        auto add = [&](const QString& label, const QString& file) {
            auto* b = btn(label);
            b->setCheckable(true);
            b->setChecked(s.value(key).toString() == file);
            connect(b, &QPushButton::clicked, this, [applyNow, key, file] { QSettings().setValue(key, file); applyNow(); });
            g->addWidget(b, n / 3, n % 3);
            ++n;
        };
        add(T("Nenhum"), QString());
        const QStringList files = Theme::list(kind);
        for (const QString& f : files) add(QFileInfo(f).completeBaseName(), f);
    };
    fill(allGrid_, Theme::All, "themeAll");
    fill(normalGrid_, Theme::Normal, "themeNormal");
}

QWidget* SettingsDialog::buildAppearance() {
    auto* w = new QWidget;
    auto* l = new QVBoxLayout(w);
    QSettings s;
    l->addWidget(new QLabel(T("Tema base")));
    paletteGrid_ = new QGridLayout;
    l->addLayout(paletteGrid_);

    auto* grad = new QCheckBox(T("Usar degradê (gradient) nos painéis e botões"));
    grad->setChecked(s.value("gradient", true).toBool());
    connect(grad, &QCheckBox::toggled, this, [this](bool v) { QSettings().setValue("gradient", v); applyUi(); });
    l->addWidget(grad);

    l->addSpacing(8);
    l->addWidget(new QLabel(T("Temas completos — themes/all (mudam TUDO: cores, lista de jogos, grupos, separações, degradê, logo)")));
    allGrid_ = new QGridLayout;
    l->addLayout(allGrid_);
    l->addSpacing(8);
    l->addWidget(new QLabel(T("Pacotes de botões e fontes — themes/normal (aplicados por cima; funcionam junto com os completos)")));
    normalGrid_ = new QGridLayout;
    l->addLayout(normalGrid_);

    auto* row = new QHBoxLayout;
    auto* imp = btn(T("Importar tema (.css / .qss)…"));
    auto* oa = btn(T("Abrir themes/all"));
    auto* on = btn(T("Abrir themes/normal"));
    row->addWidget(imp);
    row->addWidget(oa);
    row->addWidget(on);
    row->addStretch();
    l->addSpacing(8);
    l->addLayout(row);
    refreshThemeButtons();

    connect(imp, &QPushButton::clicked, this, [this] {
        const QStringList fs = QFileDialog::getOpenFileNames(this, T("Importar tema"), {}, "CSS (*.css *.qss)");
        QStringList msg;
        for (const QString& f : fs) msg << QFileInfo(f).fileName() + " → " + (Theme::importFile(f) == Theme::All ? "themes/all" : "themes/normal");
        if (!msg.isEmpty()) barLbl_->setText(T("Importado: ") + msg.join(", "));
        refreshThemeButtons();
    });
    auto open = [](Theme::Kind k) {
        QString dir = Theme::roots().constLast() + (k == Theme::All ? "/all" : "/normal");
        if (!QDir(dir).exists()) { dir = Theme::importDir(k); QDir().mkpath(dir); }
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    };
    connect(oa, &QPushButton::clicked, this, [open] { open(Theme::All); });
    connect(on, &QPushButton::clicked, this, [open] { open(Theme::Normal); });

    auto* gc = new QGroupBox(T("Personalizar"));
    auto* form = new QFormLayout(gc);
    form->setLabelAlignment(Qt::AlignLeft);

    auto* accent = btn(T("Escolher cor…"));
    auto* accentReset = btn(T("Padrão do tema"));
    auto* ar = new QHBoxLayout;
    ar->addWidget(accent);
    ar->addWidget(accentReset);
    ar->addStretch(1);
    form->addRow(T("Cor de destaque:"), ar);
    connect(accent, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(Theme::current().accent, this, T("Cor de destaque"));
        if (!c.isValid()) return;
        QSettings().setValue("ui/accent", c.name());
        applyUi();
    });
    connect(accentReset, &QPushButton::clicked, this, [this] { QSettings().remove("ui/accent"); applyUi(); });

    auto* bs = new QComboBox;
    bs->addItem(T("Do tema"), "theme");
    bs->addItem(T("Preenchido"), "filled");
    bs->addItem(T("Contorno"), "outline");
    bs->addItem(T("Plano"), "flat");
    bs->setCurrentIndex(qMax(0, bs->findData(s.value("ui/btnStyle", "theme"))));
    connect(bs, &QComboBox::currentIndexChanged, this, [this, bs] { QSettings().setValue("ui/btnStyle", bs->currentData()); applyUi(); });
    form->addRow(T("Estilo dos botões:"), bs);

    auto spin = [&](const char* key, int lo, int hi) {
        auto* sp = new QSpinBox;
        sp->setRange(lo, hi);
        sp->setSpecialValueText(T("tema"));
        sp->setValue(s.value(key, -1).toInt());
        connect(sp, &QSpinBox::valueChanged, this, [this, key](int v) { QSettings().setValue(key, v); applyUi(); });
        return sp;
    };
    form->addRow(T("Arredondamento dos botões:"), spin("ui/btnRadius", -1, 30));
    form->addRow(T("Arredondamento da lista:"), spin("ui/rowRadius", -1, 30));
    form->addRow(T("Espaço entre jogos:"), spin("ui/rowGap", -1, 20));

    auto* dens = new QComboBox;
    dens->addItem(T("Compacta"), "compact");
    dens->addItem(T("Normal"), "normal");
    dens->addItem(T("Grande"), "large");
    dens->setCurrentIndex(qMax(0, dens->findData(s.value("ui/density", "normal"))));
    connect(dens, &QComboBox::currentIndexChanged, this, [this, dens] { QSettings().setValue("ui/density", dens->currentData()); applyUi(); });
    form->addRow(T("Altura da lista:"), dens);

    auto fontBox = [&](const char* key) {
        auto* cb = new QComboBox;
        cb->addItem(T("(automático)"), "");
        const QStringList fams = Theme::availableFonts();
        for (const QString& f : fams) cb->addItem(f, f);
        cb->setCurrentIndex(qMax(0, cb->findData(s.value(key).toString())));
        cb->setMaxVisibleItems(16);
        connect(cb, &QComboBox::currentIndexChanged, this, [this, cb, key] { QSettings().setValue(key, cb->currentData()); applyUi(); });
        return cb;
    };
    form->addRow(T("Fonte do texto:"), fontBox("ui/fontBody"));
    form->addRow(T("Fonte dos títulos e botões:"), fontBox("ui/fontTitle"));
    form->addRow(T("Fonte de dados (tempo, caminhos):"), fontBox("ui/fontMono"));

    auto* scale = new QSlider(Qt::Horizontal);
    scale->setRange(80, 140);
    scale->setSingleStep(5);
    scale->setValue(s.value("ui/fontScale", 100).toInt());
    auto* scaleLbl = new QLabel(QString("%1%").arg(scale->value()));
    scaleLbl->setMinimumWidth(48);
    auto* sr = new QHBoxLayout;
    sr->addWidget(scale, 1);
    sr->addWidget(scaleLbl);
    form->addRow(T("Tamanho do texto:"), sr);
    connect(scale, &QSlider::valueChanged, this, [scaleLbl](int v) { scaleLbl->setText(QString("%1%").arg(v)); });
    connect(scale, &QSlider::sliderReleased, this, [this, scale] { QSettings().setValue("ui/fontScale", scale->value()); applyUi(); });

    auto imageRow = [&](const char* key, const QString& pickTitle) {
        auto* pick = btn(T("Escolher imagem…"));
        auto* clr = btn(T("Remover"));
        auto* r2 = new QHBoxLayout;
        r2->addWidget(pick);
        r2->addWidget(clr);
        r2->addStretch(1);
        connect(pick, &QPushButton::clicked, this, [this, key, pickTitle] {
            const QString p = QFileDialog::getOpenFileName(this, pickTitle, {}, "Imagens (*.png *.jpg *.jpeg *.webp *.bmp)");
            if (p.isEmpty()) return;
            QSettings().setValue(key, p);
            applyUi();
        });
        connect(clr, &QPushButton::clicked, this, [this, key] { QSettings().remove(key); applyUi(); });
        return r2;
    };
    form->addRow(T("Imagem de fundo:"), imageRow("ui/bgImage", T("Escolha a imagem de fundo")));
    auto* op = new QSlider(Qt::Horizontal);
    op->setRange(0, 100);
    op->setValue(s.value("ui/bgOpacity", 25).toInt());
    form->addRow(T("Opacidade do fundo:"), op);
    connect(op, &QSlider::sliderReleased, this, [this, op] { QSettings().setValue("ui/bgOpacity", op->value()); applyUi(); });
    form->addRow(T("Logo personalizada:"), imageRow("ui/logoImage", T("Escolha a logo")));

    auto* resetAll = btn(T("Restaurar personalização"));
    connect(resetAll, &QPushButton::clicked, this, [this] {
        QSettings s2;
        for (const char* k : {"ui/accent", "ui/btnStyle", "ui/btnRadius", "ui/rowRadius", "ui/rowGap", "ui/density", "ui/fontBody", "ui/fontTitle", "ui/fontMono",
                              "ui/fontScale", "ui/bgImage", "ui/bgOpacity", "ui/logoImage"})
            s2.remove(QString::fromLatin1(k));
        applyUi();
        barLbl_->setText(T("Personalização restaurada. Reabra esta janela para ver os valores."));
    });
    form->addRow(QString(), resetAll);
    l->addWidget(gc);
    l->addStretch(1);
    return w;
}

QWidget* SettingsDialog::buildMeta() {
    auto* w = new QWidget;
    auto* l = new QVBoxLayout(w);
    QSettings s;
    auto chk = [this](const QString& text, const char* key, bool def, QLayout* lay) {
        auto* c = new QCheckBox(text);
        c->setChecked(QSettings().value(key, def).toBool());
        connect(c, &QCheckBox::toggled, this, [this, key](bool v) { QSettings().setValue(key, v); emit generalChanged(); });
        lay->addWidget(c);
    };
    chk(T("Baixar metadados automaticamente (ao iniciar e ao adicionar jogos), em segundo plano"), "autoMeta", true, l);
    chk(T("Usar o nome gravado dentro da ROM (em vez do nome do arquivo)"), "useInternalName", true, l);

    auto* hrs = new QSpinBox;
    hrs->setRange(0, 720);
    hrs->setSpecialValueText(T("desativado"));
    hrs->setSuffix(T(" h"));
    hrs->setValue(s.value("metaHours", 0).toInt());
    connect(hrs, &QSpinBox::valueChanged, this, [this](int v) { QSettings().setValue("metaHours", v); emit generalChanged(); });
    auto* rh = new QHBoxLayout;
    rh->addWidget(new QLabel(T("Repetir a verificação a cada:")));
    rh->addWidget(hrs);
    rh->addStretch();
    l->addLayout(rh);

    auto* box = new QGroupBox(T("IGDB (nomes, capas, banners e descrições)"));
    auto* bl = new QVBoxLayout(box);
    chk(T("Usar o IGDB para imagens e descrições"), "useIgdb", true, bl);
    chk(T("Usar o nome do IGDB como nome do jogo"), "useIgdbName", true, bl);
    auto* res = new QLabel(igdb_->configured() ? T("Credenciais do IGDB incluídas no app.") : T("Este build não tem credenciais do IGDB."));
    res->setObjectName("muted");
    res->setWordWrap(true);
    auto* re = btn(T("Reconectar"));
    re->setEnabled(igdb_->configured());
    auto* tr = new QHBoxLayout;
    tr->addWidget(re);
    tr->addWidget(res, 1);
    bl->addLayout(tr);
    connect(re, &QPushButton::clicked, this, [this, res] {
        res->setText(T("Testando…"));
        igdb_->test([res](bool ok, QString m) { res->setText((ok ? QString::fromUtf8("✔ ") : QString::fromUtf8("✖ ")) + m); });
    });
    l->addWidget(box);

    l->addSpacing(6);
    auto* now = btn(T("Baixar / atualizar metadados agora"));
    auto* retry = btn(T("Refazer os que falharam"));
    auto* clr = btn(T("Limpar cache"));
    auto* rb = new QHBoxLayout;
    rb->addWidget(now);
    rb->addWidget(retry);
    rb->addWidget(clr);
    rb->addStretch();
    l->addLayout(rb);
    cacheLbl_ = new QLabel;
    cacheLbl_->setObjectName("mono");
    auto upd = [this] { cacheLbl_->setText(T("Cache: %1 MB em %2").arg(meta_->cacheBytes() / 1048576.0, 0, 'f', 1).arg(meta_->cacheDir())); };
    upd();
    l->addWidget(cacheLbl_);
    l->addStretch();
    connect(now, &QPushButton::clicked, this, [this] { emit metadataRequested(false); barLbl_->setText(T("Baixando metadados…")); });
    connect(retry, &QPushButton::clicked, this, [this] { emit metadataRequested(true); barLbl_->setText(T("Baixando metadados…")); });
    connect(clr, &QPushButton::clicked, this, [this, upd] { meta_->clearCache(); upd(); });
    return w;
}

QWidget* SettingsDialog::buildAdvanced() {
    auto* w = new QWidget;
    auto* l = new QVBoxLayout(w);
    QSettings s;

    auto* g1 = new QGroupBox(T("Inicialização"));
    auto* v1 = new QVBoxLayout(g1);
    auto chk = [this](const QString& text, const char* key, bool def, QLayout* lay) {
        auto* c = new QCheckBox(text);
        c->setChecked(QSettings().value(key, def).toBool());
        connect(c, &QCheckBox::toggled, this, [this, key](bool v) { QSettings().setValue(key, v); emit generalChanged(); });
        lay->addWidget(c);
        return c;
    };
    auto* os = chk(T("Iniciar o EmuCenter junto com o Windows"), "startWithOS", false, v1);
#ifdef Q_OS_WIN
    connect(os, &QCheckBox::toggled, this, [](bool on) {
        QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        if (on) run.setValue("EmuCenter", "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\"");
        else run.remove("EmuCenter");
    });
#else
    os->setEnabled(false);
#endif
    chk(T("Iniciar minimizado na bandeja"), "startMinimized", false, v1);

    auto spin = [&s](const QString& label, const char* key, int def, int max, QLayout* lay) {
        auto* sp = new QSpinBox;
        sp->setRange(0, max);
        sp->setSuffix(T(" s"));
        sp->setValue(s.value(key, def).toInt());
        QObject::connect(sp, &QSpinBox::valueChanged, sp, [key](int v) { QSettings().setValue(key, v); });
        auto* r = new QHBoxLayout;
        r->addWidget(new QLabel(label), 1);
        r->addWidget(sp);
        static_cast<QVBoxLayout*>(lay)->addLayout(r);
    };
    spin(T("Esperar antes de começar a baixar metadados:"), "startDelayMeta", 3, 600, v1);
    spin(T("Esperar antes de checar atualizações dos emuladores:"), "startDelayUpdate", 10, 600, v1);
    spin(T("Esperar antes de checar atualizações do EmuCenter:"), "startDelayAppUpdate", 8, 600, v1);
    l->addWidget(g1);

    auto* g2 = new QGroupBox(T("Rede"));
    auto* v2 = new QVBoxLayout(g2);
    auto* par = new QSpinBox;
    par->setRange(1, 8);
    par->setValue(s.value("net/parallel", 4).toInt());
    connect(par, &QSpinBox::valueChanged, this, [](int v) { QSettings().setValue("net/parallel", v); });
    auto* r = new QHBoxLayout;
    r->addWidget(new QLabel(T("Downloads de metadados ao mesmo tempo:")), 1);
    r->addWidget(par);
    v2->addLayout(r);
    l->addWidget(g2);

    auto* gd = new QGroupBox(T("Discord Rich Presence"));
    auto* vd = new QVBoxLayout(gd);
    auto dchk = [&s, vd](const QString& text, const char* key, bool def) {
        auto* ck = new QCheckBox(text);
        ck->setChecked(s.value(key, def).toBool());
        QObject::connect(ck, &QCheckBox::toggled, ck, [key](bool v) { QSettings().setValue(key, v); DiscordRpc::instance().applySettings(); });
        vd->addWidget(ck);
    };
    dchk(T("Mostrar no Discord o que estou jogando"), "discord/enabled", false);
    dchk(T("Mostrar o nome do jogo"), "discord/showGame", true);
    dchk(T("Mostrar o console"), "discord/showConsole", true);
    dchk(T("Mostrar o tempo de jogo"), "discord/showTimer", true);
    dchk(T("Mostrar também quando estou só navegando na biblioteca"), "discord/showBrowsing", true);
    auto* dst = new QLabel(DiscordRpc::instance().status());
    dst->setObjectName("muted");
    dst->setWordWrap(true);
    connect(&DiscordRpc::instance(), &DiscordRpc::statusChanged, dst, &QLabel::setText);
    auto* reconnect = btn(T("Reconectar"));
    connect(reconnect, &QPushButton::clicked, this, [] { DiscordRpc::instance().reconnect(); });
    auto* dr = new QHBoxLayout;
    dr->addWidget(reconnect);
    dr->addWidget(dst, 1);
    vd->addLayout(dr);
    l->addWidget(gd);

    auto* g3 = new QGroupBox(T("Manutenção"));
    auto* v3 = new QHBoxLayout(g3);
    auto* data = btn(T("Abrir pasta de dados"));
    auto* rst = btn(T("Restaurar tema padrão"));
    v3->addWidget(data);
    v3->addWidget(rst);
    v3->addStretch();
    l->addWidget(g3);
    connect(data, &QPushButton::clicked, this, [] {
        const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(d);
        QDesktopServices::openUrl(QUrl::fromLocalFile(d));
    });
    connect(rst, &QPushButton::clicked, this, [this] {
        QSettings s2;
        s2.setValue("theme", "grafite");
        s2.setValue("themeAll", "");
        s2.setValue("themeNormal", "");
        s2.setValue("gradient", true);
        Theme::apply();
        refreshThemeButtons();
        emit appearanceChanged();
    });
    l->addWidget(new QLabel(T("Mudanças de inicialização valem na próxima vez que o app abrir.")));
    l->addStretch();
    return w;
}
