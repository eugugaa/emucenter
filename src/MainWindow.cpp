#include "MainWindow.h"
#include "GameSettingsDialog.h"
#include "I18n.h"
#include "SettingsDialog.h"
#include "Theme.h"
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDir>
#include <QDirIterator>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QInputDialog>
#include <QProgressDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QSystemTrayIcon>
#include <QTextBrowser>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

// ---------------------------------------------------------------- Banner
class Banner : public QWidget {
public:
    Banner() { setMinimumHeight(190); setMaximumHeight(280); logo_ = QPixmap(":/logo_icon.png"); }
    void setImage(const QPixmap& p) { pm_ = p; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette& P = Theme::current();
        p.fillRect(rect(), P.panel2);
        if (pm_.isNull()) {
            if (P.logoColor.isValid()) {   // logo colorida pelo tema, direto no fundo
                const QPixmap s = Theme::logo(logo_).scaledToHeight(height() / 2, Qt::SmoothTransformation);
                p.drawPixmap((width() - s.width()) / 2, (height() - s.height()) / 2, s);
            } else {                       // logo original sobre uma placa clara
                const int h = height() * 3 / 5;
                QRect plate(0, 0, h * 5 / 4, h);
                plate.moveCenter(rect().center());
                p.setPen(Qt::NoPen); p.setBrush(P.plate); p.drawRoundedRect(plate, 16, 16);
                const QPixmap s = logo_.scaled(plate.size() * 0.7, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                p.drawPixmap(plate.center().x() - s.width() / 2, plate.center().y() - s.height() / 2, s);
            }
        } else {
            const QPixmap s = pm_.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            p.drawPixmap((width() - s.width()) / 2, (height() - s.height()) / 2, s);
        }
        QLinearGradient g(0, height() * 0.5, 0, height());   // esmaece o banner até a cor do fundo
        QColor t = P.bg; t.setAlpha(0);
        g.setColorAt(0, t); g.setColorAt(1, P.bg);
        p.fillRect(rect(), g);
    }
private:
    QPixmap pm_, logo_;
};

// ---------------------------------------------------------------- Lista: linhas claro / escuro alternadas
class GameDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    enum { Parity = Qt::UserRole + 1, NoEmu = Qt::UserRole + 2 };
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& i) const override {
        const Palette& P = Theme::current();
        return QSize(0, i.parent().isValid() ? 54 + P.rowGap : 42);
    }
    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override {
        const Palette& P = Theme::current();
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const int rad = P.rowRadius;
        if (!i.parent().isValid()) {   // cabeçalho do console
            const QRect g = o.rect.adjusted(8, 5, -8, -1);
            p->setPen(Qt::NoPen);
            p->setBrush(P.groupBg);
            p->drawRoundedRect(g, rad, rad);
            p->setBrush(P.accent);
            p->drawRoundedRect(QRect(g.left(), g.top() + 7, 4, g.height() - 14), 2, 2);
            QFont f = o.font; f.setBold(true); p->setFont(f); p->setPen(P.groupText);
            const auto* tv = qobject_cast<const QTreeView*>(o.widget);
            const QString arrow = (tv && tv->isExpanded(i)) ? QString::fromUtf8("▾  ") : QString::fromUtf8("▸  ");
            p->drawText(g.adjusted(18, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, arrow + i.data().toString());
        } else {
            const QRect row = o.rect.adjusted(8, P.rowGap / 2, -8, -(P.rowGap - P.rowGap / 2));
            const bool sel = o.state & QStyle::State_Selected;
            const bool dark = i.data(Parity).toInt() == 1;
            const QColor bg = sel ? P.rowSel : (dark ? P.rowDark : P.rowLight);
            QColor fg = sel ? P.rowSelText : (dark ? P.rowDarkText : P.rowLightText);
            if (!sel && i.data(NoEmu).toBool()) fg = dark ? P.err : QColor("#b3261e");
            p->setPen(Qt::NoPen);
            p->setBrush(bg);
            p->drawRoundedRect(row, rad, rad);
            if (!sel && P.rowGap == 0) {   // sem espaço entre linhas: uma linha fina separa
                p->setPen(P.separator);
                p->drawLine(row.left() + rad, row.bottom(), row.right() - rad, row.bottom());
            }
            const QRect ir(row.left() + 10, row.top() + (row.height() - 40) / 2, 40, 40);
            QPainterPath path; path.addRoundedRect(ir, 7, 7);
            p->setClipPath(path);
            p->fillRect(ir, dark ? QColor("#222733") : QColor("#c9cfdc"));
            const QIcon ic = i.data(Qt::DecorationRole).value<QIcon>();
            if (!ic.isNull()) ic.paint(p, ir);
            p->setClipping(false);
            p->setPen(fg);
            const int tx = ir.right() + 12;
            const QString t = o.fontMetrics.elidedText(i.data().toString(), Qt::ElideRight, row.right() - tx - 10);
            p->drawText(QRect(tx, row.top(), row.right() - tx - 10, row.height()), Qt::AlignVCenter | Qt::AlignLeft, t);
        }
        p->restore();
    }
};

// ---------------------------------------------------------------- Barra de título
class TitleBar : public QWidget {
public:
    std::function<void()> onDouble;
    TitleBar() { setObjectName("titlebar"); setFixedHeight(48); }
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && window()->windowHandle()) window()->windowHandle()->startSystemMove();
    }
    void mouseDoubleClickEvent(QMouseEvent*) override { if (onDouble) onDouble(); }
};

static QToolButton* tbtn(const QString& t, const QString& tip, const char* name = nullptr) {
    auto* b = new QToolButton; b->setText(t); b->setToolTip(tip); b->setCursor(Qt::PointingHandCursor);
    if (name) b->setObjectName(name);
    return b;
}

// ---------------------------------------------------------------- MainWindow
MainWindow::MainWindow() : meta_(&igdb_) {
    QSettings st;
    frameless_ = st.value("frameless", true).toBool();
    setWindowTitle("EmuCenter");
    setWindowIcon(QIcon(":/app_icon.png"));
    resize(1180, 720);
    setMinimumSize(900, 560);
    if (frameless_) setWindowFlag(Qt::FramelessWindowHint);

    // ---------- barra de título com botões embutidos ----------
    auto* bar = new TitleBar;
    bar->onDouble = [this] { if (frameless_) toggleMaximize(); };
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(12, 0, 0, 0); bl->setSpacing(2);
    auto* ico = new QLabel; ico->setPixmap(QPixmap(":/app_icon.png").scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto* ttl = new QLabel("EmuCenter"); ttl->setObjectName("muted");
    bl->addWidget(ico); bl->addSpacing(6); bl->addWidget(ttl); bl->addStretch();
    auto* addB = tbtn(QString::fromUtf8("＋ ") + T("ROMs"), T("Adicionar pasta de ROMs"));
    auto* metaB = tbtn(QString::fromUtf8("⟳ ") + T("Metadados"), T("Baixar / atualizar metadados"));
    auto* cfgB = tbtn(QString::fromUtf8("⚙ ") + T("Configurações"), T("Configurações"));
    bl->addWidget(addB); bl->addWidget(metaB); bl->addWidget(cfgB);
    if (frameless_) {
        bl->addSpacing(8);
        auto* mn = tbtn(QString::fromUtf8("—"), T("Minimizar"), "winbtn");
        maxBtn_ = tbtn(QString::fromUtf8("▢"), T("Maximizar"), "winbtn");
        auto* cl = tbtn(QString::fromUtf8("✕"), T("Fechar"), "closebtn");
        connect(mn, &QToolButton::clicked, this, &QWidget::showMinimized);
        connect(maxBtn_, &QToolButton::clicked, this, &MainWindow::toggleMaximize);
        connect(cl, &QToolButton::clicked, this, &QWidget::close);
        bl->addWidget(mn); bl->addWidget(maxBtn_); bl->addWidget(cl);
        qApp->installEventFilter(this);   // redimensionar pelas bordas
    }
    {   // botão ROMs: escolher entre adicionar uma pasta ou importar arquivos
        auto* romMenu = new QMenu(addB);
        romMenu->addAction(T("Adicionar pasta de ROMs…"), this, &MainWindow::addRomFolder);
        romMenu->addAction(T("Importar arquivo(s) de ROM…"), this, &MainWindow::importRoms);
        addB->setMenu(romMenu);
        addB->setPopupMode(QToolButton::InstantPopup);
    }
    connect(metaB, &QToolButton::clicked, this, [this] { refreshMetadata(false); });
    connect(cfgB, &QToolButton::clicked, this, [this] { openSettings(0); });

    // ---------- esquerda: logo (abre menu) + lista ----------
    auto* side = new QWidget; side->setObjectName("side");
    auto* sl = new QVBoxLayout(side);
    sl->setContentsMargins(10, 10, 0, 10);
    auto* logo = new QToolButton; logo->setObjectName("logoBtn");
    logoBtn_ = logo;
    logo->setIconSize(QSize(210, 22));
    applyLogo();
    logo->setToolTip(T("Menu do EmuCenter"));
    logo->setCursor(Qt::PointingHandCursor);
    logo->setPopupMode(QToolButton::InstantPopup);
    logoMenu_ = new QMenu(this);
    logo->setMenu(logoMenu_);
    connect(logoMenu_, &QMenu::aboutToShow, this, &MainWindow::fillLogoMenu);
    sl->addWidget(logo);
    sl->addSpacing(6);

    tree_ = new QTreeWidget;
    tree_->setHeaderHidden(true);
    tree_->setIndentation(0);
    tree_->setRootIsDecorated(false);
    tree_->setExpandsOnDoubleClick(false);
    tree_->setIconSize(QSize(40, 40));
    tree_->setItemDelegate(new GameDelegate(tree_));
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setFrameShape(QFrame::NoFrame);
    sl->addWidget(tree_, 1);
    side->setMinimumWidth(300);

    // ---------- direita: banner + jogar + informações ----------
    auto* main = new QWidget; main->setObjectName("root");
    auto* ml = new QVBoxLayout(main);
    ml->setContentsMargins(0, 0, 0, 12);
    banner_ = new Banner;
    ml->addWidget(banner_);
    auto* body = new QVBoxLayout;
    body->setContentsMargins(20, 10, 20, 0);
    ml->addLayout(body, 1);
    title_ = new QLabel(T("Selecione um jogo")); title_->setObjectName("title");
    info_ = new QLabel; info_->setObjectName("muted");
    auto* row = new QHBoxLayout; row->setSpacing(12);
    playBtn_ = new QPushButton(T("JOGAR")); playBtn_->setObjectName("play"); playBtn_->setCursor(Qt::PointingHandCursor);
    installBtn_ = new QPushButton;
    status_ = new QLabel; status_->setTextFormat(Qt::RichText);
    row->addWidget(playBtn_); row->addWidget(installBtn_); row->addWidget(status_, 1);
    desc_ = new QTextBrowser; desc_->setOpenExternalLinks(true);
    body->addWidget(title_); body->addWidget(info_); body->addSpacing(6); body->addLayout(row); body->addSpacing(6); body->addWidget(desc_, 1);

    auto* split = new QSplitter;
    split->addWidget(side); split->addWidget(main);
    split->setStretchFactor(1, 1); split->setCollapsible(0, false); split->setHandleWidth(1);

    auto* central = new QWidget;
    auto* cl = new QVBoxLayout(central);
    cl->setContentsMargins(0, 0, 0, 0); cl->setSpacing(0);
    cl->addWidget(bar); cl->addWidget(split, 1);
    setCentralWidget(central);

    // ---------- barra de status: progresso dos metadados ----------
    metaLbl_ = new QLabel; metaBar_ = new QProgressBar; metaBar_->setFixedWidth(220); metaBar_->hide();
    statusBar()->addPermanentWidget(metaLbl_);
    statusBar()->addPermanentWidget(metaBar_);
    statusBar()->showMessage(T("Pronto."));

    // ---------- bandeja ----------
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        tray_ = new QSystemTrayIcon(QIcon(":/app_icon.png"), this);
        auto* tm = new QMenu(this);
        tm->addAction(T("Abrir EmuCenter"), this, [this] { showNormal(); raise(); activateWindow(); });
        tm->addAction(T("Atualizar metadados"), this, [this] { refreshMetadata(false); });
        tm->addSeparator();
        tm->addAction(T("Sair"), this, [this] { quitting_ = true; close(); });
        tray_->setContextMenu(tm);
        tray_->setToolTip("EmuCenter");
        connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
            if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) { showNormal(); raise(); activateWindow(); }
        });
        tray_->show();
    }
    QApplication::setQuitOnLastWindowClosed(false);

    // ---------- ligações ----------
    connect(playBtn_, &QPushButton::clicked, this, &MainWindow::play);
    connect(installBtn_, &QPushButton::clicked, this, &MainWindow::installMissing);
    connect(tree_, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onSelect);
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* it) { if (it->parent()) play(); });
    connect(tree_, &QTreeWidget::itemClicked, this, [](QTreeWidgetItem* it) { if (!it->parent()) it->setExpanded(!it->isExpanded()); });
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, &MainWindow::gameMenu);

    connect(&emu_, &EmulatorManager::status, this, [this](const QString& m) { statusBar()->showMessage(m); });
    connect(&emu_, &EmulatorManager::progress, this, [this](int p) {
        metaBar_->setRange(0, p < 0 ? 0 : 100);
        if (p >= 0) metaBar_->setValue(p);
        metaBar_->setVisible(p < 100);
        if (p >= 100) metaLbl_->clear();
    });
    connect(&emu_, &EmulatorManager::finished, this, [this](bool ok, const QString& m) {
        statusBar()->showMessage(m, 8000);
        if (!ok) QMessageBox::information(this, "EmuCenter", m);
        rebuildTree();
    });
    connect(&emu_, &EmulatorManager::updatesChecked, this, [this](int n) {
        if (n > 0) notify("EmuCenter", T("%1 atualização(ões) de emulador disponível(is). Veja em Configurações › Emuladores.").arg(n));
    });

    connect(&meta_, &Metadata::imageReady, this, [this](const QString& key, int kind, const QPixmap& pm) {
        if (kind == Metadata::Icon) {
            const QIcon ic(pm.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            icons_[key] = ic;
            if (auto* it = items_.value(key)) it->setIcon(0, ic);
        } else if (const Game* g = current(); g && g->key() == key) banner_->setImage(pm);
    });
    connect(&meta_, &Metadata::nameReady, this, [this](const QString& key, const QString& name) {
        for (auto& g : games_) if (g.key() == key) g.title = name;
        if (auto* it = items_.value(key)) it->setText(0, name);
        if (const Game* g = current(); g && g->key() == key) title_->setText(name);
        tree_->viewport()->update();
    });
    connect(&meta_, &Metadata::textReady, this, [this](const QString& key, const QString& t) {
        if (const Game* g = current(); g && g->key() == key) desc_->setPlainText(t);
    });
    connect(&meta_, &Metadata::progress, this, [this](int d, int t) {
        metaBar_->setRange(0, t); metaBar_->setValue(d);
        metaBar_->setVisible(t > 0 && d < t);
        metaLbl_->setText(t > 0 && d < t ? T("Metadados: %1/%2").arg(d).arg(t) : QString());
    });
    connect(&meta_, &Metadata::batchFinished, this, [this](int, int dl) {
        metaBar_->hide(); metaLbl_->clear();
        if (dl > 0) { statusBar()->showMessage(T("Metadados atualizados."), 6000); notify("EmuCenter", T("Metadados dos jogos atualizados (%1 itens novos).").arg(dl)); }
    });

    metaTimer_ = new QTimer(this);
    connect(metaTimer_, &QTimer::timeout, this, [this] { refreshMetadata(false); });
    applyGeneral();

    // lista aparece na hora (ícones do cache); rede só depois dos atrasos configurados em Avançado
    rescanImpl(false);
    if (QSettings().value("romDirs").toStringList().isEmpty()) QTimer::singleShot(400, this, &MainWindow::addRomFolder);
    {
        QSettings s;
        if (s.value("autoMeta", true).toBool())
            QTimer::singleShot(s.value("startDelayMeta", 3).toInt() * 1000, this, [this] { meta_.enqueueAll(games_); });
        if (s.value("autoEmuUpdate", false).toBool())
            QTimer::singleShot(s.value("startDelayUpdate", 10).toInt() * 1000, this, &MainWindow::checkEmuUpdates);
        if (!Theme::notice().isEmpty()) QTimer::singleShot(1500, this, [this] { notify("EmuCenter", Theme::notice()); });
    }
}

void MainWindow::bringToFront() {
    if (isMinimized()) showNormal(); else show();
    raise();
    activateWindow();
}

void MainWindow::applyGeneral() {
    const int h = QSettings().value("metaHours", 0).toInt();
    if (h > 0) metaTimer_->start(h * 3600 * 1000); else metaTimer_->stop();
}

void MainWindow::notify(const QString& title, const QString& msg) {
    if (!QSettings().value("notify", true).toBool()) return;
    if (tray_ && tray_->isVisible()) tray_->showMessage(title, msg, QSystemTrayIcon::Information, 6000);
    else statusBar()->showMessage(msg, 8000);
}

// ---------------------------------------------------------------- janela
void MainWindow::toggleMaximize() {
    if (isMaximized()) { showNormal(); if (maxBtn_) maxBtn_->setText(QString::fromUtf8("▢")); }
    else { showMaximized(); if (maxBtn_) maxBtn_->setText(QString::fromUtf8("❐")); }
}

bool MainWindow::eventFilter(QObject* o, QEvent* ev) {
    if (!frameless_ || isMaximized() || ev->type() != QEvent::MouseButtonPress) return false;
    auto* w = qobject_cast<QWidget*>(o);
    if (!w || w->window() != this) return false;
    auto* me = static_cast<QMouseEvent*>(ev);
    if (me->button() != Qt::LeftButton) return false;
    const QPoint p = mapFromGlobal(me->globalPosition().toPoint());
    const int m = 6;
    Qt::Edges ed;
    if (p.x() < m) ed |= Qt::LeftEdge;
    if (p.x() > width() - m) ed |= Qt::RightEdge;
    if (p.y() < m) ed |= Qt::TopEdge;
    if (p.y() > height() - m) ed |= Qt::BottomEdge;
    if (ed && windowHandle()) { windowHandle()->startSystemResize(ed); return true; }
    return false;
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (!quitting_ && tray_ && tray_->isVisible() && QSettings().value("background", true).toBool()) {
        hide();
        e->ignore();
        if (!shownTrayHint_) { shownTrayHint_ = true; tray_->showMessage("EmuCenter", T("O EmuCenter continua em segundo plano. Clique no ícone da bandeja para abrir."), QSystemTrayIcon::Information, 5000); }
        return;
    }
    e->accept();
    qApp->quit();
}

// ---------------------------------------------------------------- menu da logo
void MainWindow::fillLogoMenu() {
    QMenu* m = logoMenu_;
    m->clear();
    m->addAction(T("Configurações…"), this, [this] { openSettings(0); });
    m->addSeparator();
    m->addAction(T("Adicionar pasta de ROMs…"), this, &MainWindow::addRomFolder);
    m->addAction(T("Importar arquivo(s) de ROM…"), this, &MainWindow::importRoms);
    m->addAction(T("Procurar jogos novamente"), this, &MainWindow::rescan);
    m->addAction(T("Atualizar metadados"), this, [this] { refreshMetadata(false); });
    m->addAction(T("Refazer metadados que falharam"), this, [this] { refreshMetadata(true); });
    m->addAction(T("Verificar atualizações dos emuladores"), this, &MainWindow::checkEmuUpdates);
    m->addSeparator();

    QMenu* lang = m->addMenu(T("Idioma"));
    auto* lg = new QActionGroup(lang);
    for (auto [code, name] : {std::pair<QString, QString>{"pt", "Português (Brasil)"}, {"en", "English"}}) {
        QAction* a = lang->addAction(name); a->setCheckable(true); a->setChecked(I18n::lang() == code); lg->addAction(a);
        connect(a, &QAction::triggered, this, [this, code = code] {
            I18n::setLang(code);
            QMessageBox::information(this, "EmuCenter", T("Reinicie o EmuCenter para aplicar o idioma."));
        });
    }
    QMenu* th = m->addMenu(T("Tema"));
    auto* tg = new QActionGroup(th);
    for (const auto& p : Theme::builtins()) {
        QAction* a = th->addAction(p.name); a->setCheckable(true); a->setChecked(QSettings().value("theme", "grafite") == p.id); tg->addAction(a);
        connect(a, &QAction::triggered, this, [this, id = p.id] { QSettings().setValue("theme", id); Theme::apply(); applyLogo(); rebuildTree(); banner_->update(); });
    }
    m->addSeparator();
    m->addAction(T("Abrir pasta de dados"), this, [] {
        const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(d); QDesktopServices::openUrl(QUrl::fromLocalFile(d));
    });
    m->addAction(T("Abrir pasta dos emuladores"), this, [this] { QDir().mkpath(emu_.root()); QDesktopServices::openUrl(QUrl::fromLocalFile(emu_.root())); });
    m->addAction(T("Sobre o EmuCenter"), this, [this] {
        QMessageBox::about(this, "EmuCenter", "<b>EmuCenter 0.2</b><br>" + T("Central leve de emuladores.") + "<br><br>" +
                                                  T("Imagens: libretro-thumbnails  •  Descrições: Wikipédia"));
    });
    m->addSeparator();
    m->addAction(T("Sair"), this, [this] { quitting_ = true; close(); });
}

// ---------------------------------------------------------------- biblioteca
void MainWindow::addRomFolder() {
    const QString d = QFileDialog::getExistingDirectory(this, T("Escolha a pasta com suas ROMs"));
    if (d.isEmpty()) return;
    QSettings s;
    QStringList dirs = s.value("romDirs").toStringList();
    if (!dirs.contains(d)) dirs << d;
    s.setValue("romDirs", dirs);
    rescan();
}

void MainWindow::importRoms() {
    QStringList dirs = QSettings().value("romDirs").toStringList();
    if (dirs.isEmpty()) {   // precisa de uma pasta de biblioteca para onde importar
        addRomFolder();
        dirs = QSettings().value("romDirs").toStringList();
        if (dirs.isEmpty()) return;
    }
    QStringList exts;
    for (const auto& sd : Catalog::systems()) for (const auto& e : sd.exts) if (!exts.contains("*." + e)) exts << "*." + e;
    const QStringList files = QFileDialog::getOpenFileNames(this, T("Escolha as ROMs para importar"), {}, T("ROMs (%1)").arg(exts.join(' ')));
    if (files.isEmpty()) return;

    QString dest = dirs.first();
    if (dirs.size() > 1) {
        bool ok = false;
        dest = QInputDialog::getItem(this, "EmuCenter", T("Importar para qual pasta?"), dirs, 0, false, &ok);
        if (!ok) return;
    }
    QMessageBox box(QMessageBox::Question, "EmuCenter", T("Copiar ou mover os arquivos para a biblioteca?"), QMessageBox::NoButton, this);
    auto* copyB = box.addButton(T("Copiar"), QMessageBox::AcceptRole);
    auto* moveB = box.addButton(T("Mover"), QMessageBox::DestructiveRole);
    box.addButton(T("Cancelar"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() != copyB && box.clickedButton() != moveB) return;
    const bool move = box.clickedButton() == moveB;

    QProgressDialog pd(T("Importando…"), T("Cancelar"), 0, files.size(), this);
    pd.setWindowModality(Qt::WindowModal);
    pd.setMinimumDuration(0);
    int okCount = 0;
    QStringList failed;
    for (int i = 0; i < files.size(); ++i) {
        pd.setValue(i);
        pd.setLabelText(T("Importando…") + "\n" + QFileInfo(files[i]).fileName());
        if (pd.wasCanceled()) break;
        QString err;
        if (Library::importFile(files[i], dest, move, [&pd] { return pd.wasCanceled(); }, &err)) ++okCount;
        else failed << QFileInfo(files[i]).fileName() + " — " + err;
    }
    pd.setValue(files.size());
    rescan();
    statusBar()->showMessage(T("%1 ROM(s) importada(s).").arg(okCount), 8000);
    if (!failed.isEmpty()) QMessageBox::warning(this, "EmuCenter", T("Não foi possível importar:") + "\n" + failed.join("\n"));
}

void MainWindow::rescan() { rescanImpl(true); }

void MainWindow::rescanImpl(bool network) {
    games_ = Library::scan(QSettings().value("romDirs").toStringList());
    rebuildTree();
    meta_.loadCachedIcons(games_);   // ícones que já estão no disco, sem rede
    if (!network) return;
    if (QSettings().value("autoMeta", true).toBool()) meta_.enqueueAll(games_);
    else for (const auto& g : games_) if (!icons_.contains(g.key())) meta_.requestIcon(g);
}

void MainWindow::applyLogo() {
    if (!logoBtn_) return;
    const QPixmap px = Theme::logo(QPixmap(":/logo_text.png")).scaledToWidth(420, Qt::SmoothTransformation);
    logoBtn_->setIcon(QIcon(px));
}

void MainWindow::refreshMetadata(bool force) {
    if (force) meta_.clearFailures();
    if (games_.isEmpty()) { statusBar()->showMessage(T("Nenhum jogo para atualizar."), 5000); return; }
    statusBar()->showMessage(T("Baixando metadados…"), 4000);
    meta_.enqueueAll(games_);
}

void MainWindow::checkEmuUpdates() {
    statusBar()->showMessage(T("Verificando atualizações dos emuladores…"), 4000);
    emu_.checkAllUpdates();
}

void MainWindow::rebuildTree() {
    const QString keep = current() ? current()->key() : QString();
    tree_->blockSignals(true);
    tree_->clear(); items_.clear();
    int n = 0;   // paridade contínua em TODA a lista: claro, escuro, claro...
    for (const auto& s : Catalog::systems()) {
        QTreeWidgetItem* top = nullptr;
        const bool ok = std::any_of(s.emus.begin(), s.emus.end(), [&](const EmuLink& l) { return emu_.available(l); });
        for (const auto& g : games_) {
            if (g.systemId != s.id) continue;
            if (!top) {
                top = new QTreeWidgetItem(tree_, {s.name});
                top->setFlags(Qt::ItemIsEnabled);
            }
            auto* it = new QTreeWidgetItem(top, {g.title});
            it->setData(0, Qt::UserRole, g.key());
            it->setData(0, GameDelegate::Parity, n++ % 2);          // claro, escuro, claro...
            it->setData(0, GameDelegate::NoEmu, !ok);
            if (icons_.contains(g.key())) it->setIcon(0, icons_[g.key()]);
            items_[g.key()] = it;
        }
        if (top) { top->setText(0, QString("%1  (%2)").arg(s.name).arg(top->childCount())); top->setExpanded(true); }
    }
    tree_->blockSignals(false);
    if (items_.contains(keep)) tree_->setCurrentItem(items_[keep]);
    else onSelect();
    tree_->viewport()->update();
}

const Game* MainWindow::current() const {
    auto* it = tree_->currentItem();
    if (!it || !it->parent()) return nullptr;
    const QString k = it->data(0, Qt::UserRole).toString();
    for (const auto& g : games_) if (g.key() == k) return &g;
    return nullptr;
}

void MainWindow::onSelect() {
    const Game* g = current();
    banner_->setImage({});
    desc_->clear();
    if (!g) {
        title_->setText(T("Selecione um jogo")); info_->clear(); status_->clear();
        playBtn_->setEnabled(false); installBtn_->hide();
        return;
    }
    title_->setText(g->title);
    refreshStatus();
    meta_.requestBanner(*g);
    meta_.requestText(*g);
}

void MainWindow::refreshStatus() {
    const Game* g = current();
    if (!g) return;
    const Palette& P = Theme::current();
    const SystemDef* s = Catalog::system(g->systemId);
    const auto last = Library::lastPlayed(*g);
    info_->setText(T("%1  •  Tempo de jogo: %2  •  Último: %3")
                       .arg(s->name, Library::formatDuration(Library::playSeconds(*g)),
                            last.isValid() ? last.toString("dd/MM/yyyy") : QString::fromUtf8("—")));

    auto name = [](const EmuLink& l) {
        QString n = Catalog::emu(l.emuId)->name;
        return l.core.isEmpty() ? n : n + " (" + QString(l.core).remove("_libretro") + ")";
    };
    const bool primaryOk = emu_.available(s->emus.first());
    bool anyOk = primaryOk;
    QString html;
    if (primaryOk)
        html = "<span style='color:" + P.ok.name() + "'><b>✔ " + T("Pronto") + "</b> — " + name(s->emus.first()).toHtmlEscaped() + "</span>";
    else
        html = "<span style='color:" + P.err.name() + "'><b>" + T("1 erro") + "</b><br>need &gt;&gt; " + name(s->emus.first()).toHtmlEscaped() + "</span>";
    for (int i = 1; i < s->emus.size(); ++i) {
        const bool ok = emu_.available(s->emus[i]);
        anyOk |= ok;
        html += "<br><span style='color:" + P.warn.name() + "'>or &gt;&gt; " + name(s->emus[i]).toHtmlEscaped() + (ok ? " ✔" : "") + "</span>";
    }
    status_->setText(html);
    playBtn_->setEnabled(anyOk);
    installBtn_->setVisible(!primaryOk);
    installBtn_->setText(T("Instalar ") + Catalog::emu(s->emus.first().emuId)->name);
}

void MainWindow::installMissing() {
    const Game* g = current();
    if (!g) return;
    const EmuLink l = Catalog::system(g->systemId)->emus.first();
    emu_.install(l.emuId, l.core);
}

// ---------------------------------------------------------------- jogar
void MainWindow::play() {
    const Game* g = current();
    if (!g) return;
    const SystemDef* s = Catalog::system(g->systemId);
    const auto opts = Library::options(*g);

    QList<EmuLink> order;   // emulador escolhido para este jogo vem primeiro
    for (const EmuLink& l : s->emus) if (!opts.emu.isEmpty() && opts.emu == l.emuId + "|" + l.core) order << l;
    for (const EmuLink& l : s->emus) if (opts.emu != l.emuId + "|" + l.core) order << l;

    for (const EmuLink& l : order) {
        QString prog; QStringList args;
        if (!emu_.available(l) || !emu_.buildCommand(l, g->path, prog, args)) continue;
        if (!opts.args.trimmed().isEmpty()) args << QProcess::splitCommand(opts.args);   // comandos extras do jogo
        auto* p = new QProcess(this);
        const Game game = *g;
        auto* t = new QElapsedTimer; t->start();
        connect(p, &QProcess::finished, this, [=]() {
            Library::addPlayTime(game, t->elapsed() / 1000);
            delete t; p->deleteLater();
            refreshStatus();
        });
        p->setWorkingDirectory(QFileInfo(prog).absolutePath());
        p->start(prog, args);
        statusBar()->showMessage(T("Executando %1…").arg(g->title));
        return;
    }
    installMissing();
}

// ---------------------------------------------------------------- menu e opções por jogo
void MainWindow::gameMenu(const QPoint& pos) {
    auto* it = tree_->itemAt(pos);
    if (!it || !it->parent()) return;
    tree_->setCurrentItem(it);
    const Game* gp = current();
    if (!gp) return;
    const Game g = *gp;
    QMenu m;
    m.addAction(T("Jogar"), this, &MainWindow::play);
    m.addAction(T("Configurações do jogo…"), this, [this, g] { gameSettings(g); });
    m.addSeparator();
    m.addAction(T("Mostrar o arquivo na pasta"), this, [g] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(g.path).absolutePath())); });
    m.addAction(T("Copiar caminho"), this, [g] { QApplication::clipboard()->setText(g.path); });
    m.addAction(T("Baixar metadados deste jogo de novo"), this, [this, g] {
        meta_.refreshGame(g);
        QList<Game> one{g}; meta_.enqueueAll(one); meta_.requestBanner(g); meta_.requestText(g);
    });
    m.addAction(T("Zerar tempo de jogo"), this, [this, g] { Library::resetPlayTime(g); refreshStatus(); });
    m.exec(tree_->viewport()->mapToGlobal(pos));
}

void MainWindow::gameSettings(const Game& g) {
    GameSettingsDialog d(g, &emu_, &meta_, &igdb_, this);
    if (d.exec() != QDialog::Accepted || !d.changed()) return;
    rescanImpl(false);                                   // recalcula nomes e recarrega ícones personalizados
    QList<Game> one{g};
    meta_.enqueueAll(one);                               // baixa o que faltar com as novas escolhas
    onSelect();
}

// ---------------------------------------------------------------- configurações
void MainWindow::openSettings(int tab) {
    SettingsDialog d(&emu_, &meta_, &igdb_, this, tab);
    connect(&d, &SettingsDialog::romFoldersChanged, this, &MainWindow::rescan);
    connect(&d, &SettingsDialog::emulatorsChanged, this, [this] { rebuildTree(); refreshStatus(); });
    connect(&d, &SettingsDialog::appearanceChanged, this, [this] { applyLogo(); rebuildTree(); banner_->update(); refreshStatus(); });
    connect(&d, &SettingsDialog::generalChanged, this, &MainWindow::applyGeneral);
    connect(&d, &SettingsDialog::metadataRequested, this, &MainWindow::refreshMetadata);
    d.exec();
    rebuildTree();
    refreshStatus();
}
