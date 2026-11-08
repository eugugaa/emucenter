#include "MainWindow.h"
#include "DiscordRpc.h"
#include "GameSettingsDialog.h"
#include "I18n.h"
#include "SettingsDialog.h"
#include "Theme.h"
#include "WelcomeDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
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
#include <utility>

class Banner : public QWidget {
public:
    Banner() {
        setMinimumHeight(190);
        setMaximumHeight(280);
        logo_ = QPixmap(":/logo_icon.png");
    }
    void setImage(const QPixmap& p) { pm_ = p; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.setRenderHint(QPainter::Antialiasing);
        const Palette& P = Theme::current();
        p.fillRect(rect(), P.panel2);
        if (pm_.isNull()) {
            if (P.logoColor.isValid()) {
                const QPixmap s = Theme::logo(logo_).scaledToHeight(height() / 2, Qt::SmoothTransformation);
                p.drawPixmap((width() - s.width()) / 2, (height() - s.height()) / 2, s);
            } else {
                const int h = height() * 3 / 5;
                QRect plate(0, 0, h * 5 / 4, h);
                plate.moveCenter(rect().center());
                p.setPen(Qt::NoPen);
                p.setBrush(P.plate);
                p.drawRoundedRect(plate, 16, 16);
                const QPixmap s = logo_.scaled(plate.size() * 0.7, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                p.drawPixmap(plate.center().x() - s.width() / 2, plate.center().y() - s.height() / 2, s);
            }
        } else {
            const QPixmap s = pm_.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            p.drawPixmap((width() - s.width()) / 2, (height() - s.height()) / 2, s);
        }
        QLinearGradient g(0, height() * 0.5, 0, height());
        QColor t = P.bg;
        t.setAlpha(0);
        g.setColorAt(0, t);
        g.setColorAt(1, P.bg);
        p.fillRect(rect(), g);
    }
private:
    QPixmap pm_, logo_;
};

class BgWidget : public QWidget {
public:
    void setImage(const QString& path, int opacity) {
        path_ = path;
        opacity_ = opacity;
        src_ = path.isEmpty() ? QPixmap() : QPixmap(path);
        scaled_ = QPixmap();
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        if (src_.isNull()) return;
        if (scaled_.size() != size()) {
            const QPixmap s = src_.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            scaled_ = s.copy((s.width() - width()) / 2, (s.height() - height()) / 2, width(), height());
        }
        QPainter p(this);
        p.setOpacity(opacity_ / 100.0);
        p.drawPixmap(0, 0, scaled_);
    }
private:
    QString path_;
    int opacity_ = 25;
    QPixmap src_, scaled_;
};

class GameDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    enum { Parity = Qt::UserRole + 1, NoEmu = Qt::UserRole + 2, PlayText = Qt::UserRole + 3 };
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& i) const override {
        const Palette& P = Theme::current();
        return QSize(0, i.parent().isValid() ? Theme::rowHeight() + P.rowGap : 44);
    }
    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override {
        const Palette& P = Theme::current();
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const int rad = P.rowRadius;
        if (!i.parent().isValid()) {
            const QRect g = o.rect.adjusted(8, 5, -8, -1);
            p->setPen(Qt::NoPen);
            p->setBrush(P.groupBg);
            p->drawRoundedRect(g, rad, rad);
            p->setBrush(P.accent);
            p->drawRoundedRect(QRect(g.left(), g.top() + 7, 4, g.height() - 14), 2, 2);
            p->setFont(Theme::font(Theme::Title, 14, QFont::Bold));
            p->setPen(P.groupText);
            const auto* tv = qobject_cast<const QTreeView*>(o.widget);
            const QString arrow = (tv && tv->isExpanded(i)) ? QString::fromUtf8("▾  ") : QString::fromUtf8("▸  ");
            p->drawText(g.adjusted(18, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, arrow + i.data().toString());
        } else {
            const QRect row = o.rect.adjusted(8, P.rowGap / 2, -8, -(P.rowGap - P.rowGap / 2));
            const bool sel = o.state & QStyle::State_Selected;
            const bool dark = i.data(Parity).toInt() == 1;
            const QColor bg = sel ? P.rowSel : (dark ? P.rowDark : P.rowLight);
            QColor fg = sel ? P.rowSelText : (dark ? P.rowDarkText : P.rowLightText);
            if (!sel && i.data(NoEmu).toBool()) fg = P.err;
            p->setPen(Qt::NoPen);
            p->setBrush(bg);
            p->drawRoundedRect(row, rad, rad);
            if (!sel && P.rowGap == 0) {
                p->setPen(P.separator);
                p->drawLine(row.left() + rad, row.bottom(), row.right() - rad, row.bottom());
            }
            const int isz = qMin(row.height() - 10, 44);
            const QRect ir(row.left() + 10, row.top() + (row.height() - isz) / 2, isz, isz);
            QPainterPath path;
            path.addRoundedRect(ir, 7, 7);
            p->setClipPath(path);
            p->fillRect(ir, P.panel2);
            const QIcon ic = i.data(Qt::DecorationRole).value<QIcon>();
            if (!ic.isNull()) ic.paint(p, ir);
            p->setClipping(false);
            const QString pt = i.data(PlayText).toString();
            int right = row.right() - 12;
            if (!pt.isEmpty()) {
                p->setFont(Theme::font(Theme::Mono, 12));
                const int w = QFontMetrics(p->font()).horizontalAdvance(pt);
                QColor mc = fg;
                mc.setAlpha(170);
                p->setPen(mc);
                p->drawText(QRect(right - w, row.top(), w, row.height()), Qt::AlignVCenter | Qt::AlignRight, pt);
                right -= w + 10;
            }
            const QFont tf = Theme::font(Theme::Body, 14, sel ? QFont::DemiBold : QFont::Normal);
            p->setFont(tf);
            p->setPen(fg);
            const int tx = ir.right() + 12;
            const QString t = QFontMetrics(tf).elidedText(i.data().toString(), Qt::ElideRight, qMax(40, right - tx));
            p->drawText(QRect(tx, row.top(), qMax(40, right - tx), row.height()), Qt::AlignVCenter | Qt::AlignLeft, t);
        }
        p->restore();
    }
};

class TitleBar : public QWidget {
public:
    std::function<void()> onDouble;
    TitleBar() {
        setObjectName("titlebar");
        setFixedHeight(50);
    }
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && window()->windowHandle()) window()->windowHandle()->startSystemMove();
    }
    void mouseDoubleClickEvent(QMouseEvent*) override { if (onDouble) onDouble(); }
};

static QToolButton* tbtn(const QString& t, const QString& tip, const char* name = nullptr) {
    auto* b = new QToolButton;
    b->setText(t);
    b->setToolTip(tip);
    b->setCursor(Qt::PointingHandCursor);
    if (name) b->setObjectName(QString::fromLatin1(name));
    return b;
}

MainWindow::MainWindow() : meta_(&igdb_) {
    QSettings st;
    frameless_ = st.value("frameless", true).toBool();
    setWindowTitle("EmuCenter");
    setWindowIcon(QIcon(":/app_icon.png"));
    setMinimumSize(920, 580);
    if (frameless_) setWindowFlag(Qt::FramelessWindowHint);
    if (!(st.value("rememberGeometry", true).toBool() && restoreGeometry(st.value("window/geometry").toByteArray()))) resize(1240, 760);

    auto* bar = new TitleBar;
    bar->onDouble = [this] { if (frameless_) toggleMaximize(); };
    auto* bl = new QHBoxLayout(bar);
    bl->setContentsMargins(12, 0, 0, 0);
    bl->setSpacing(2);
    auto* ico = new QLabel;
    ico->setPixmap(QPixmap(":/app_icon.png").scaled(28, 28, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto* ttl = new QLabel("EmuCenter");
    ttl->setObjectName("muted");
    bl->addWidget(ico);
    bl->addSpacing(6);
    bl->addWidget(ttl);
    bl->addStretch();
    auto* addB = tbtn(QString::fromUtf8("＋ ") + T("ROMs"), T("Adicionar pasta de ROMs"));
    auto* bigB = tbtn(QString::fromUtf8("▦ ") + T("Big Picture"), T("Abrir o Big Picture"));
    auto* metaB = tbtn(QString::fromUtf8("⟳ ") + T("Metadados"), T("Baixar / atualizar metadados"));
    auto* cfgB = tbtn(QString::fromUtf8("⚙ ") + T("Configurações"), T("Configurações"));
    bl->addWidget(addB);
    bl->addWidget(bigB);
    bl->addWidget(metaB);
    bl->addWidget(cfgB);
    if (frameless_) {
        bl->addSpacing(8);
        auto* mn = tbtn(QString::fromUtf8("—"), T("Minimizar"), "winbtn");
        maxBtn_ = tbtn(QString::fromUtf8("▢"), T("Maximizar"), "winbtn");
        auto* cl = tbtn(QString::fromUtf8("✕"), T("Fechar"), "closebtn");
        connect(mn, &QToolButton::clicked, this, &QWidget::showMinimized);
        connect(maxBtn_, &QToolButton::clicked, this, &MainWindow::toggleMaximize);
        connect(cl, &QToolButton::clicked, this, &QWidget::close);
        bl->addWidget(mn);
        bl->addWidget(maxBtn_);
        bl->addWidget(cl);
        qApp->installEventFilter(this);
    }
    {
        auto* romMenu = new QMenu(addB);
        romMenu->addAction(T("Adicionar pasta de ROMs…"), this, &MainWindow::addRomFolder);
        romMenu->addAction(T("Importar arquivo(s) de ROM…"), this, &MainWindow::importRoms);
        addB->setMenu(romMenu);
        addB->setPopupMode(QToolButton::InstantPopup);
    }
    connect(bigB, &QToolButton::clicked, this, &MainWindow::enterBigPicture);
    connect(metaB, &QToolButton::clicked, this, [this] { refreshMetadata(false); });
    connect(cfgB, &QToolButton::clicked, this, &MainWindow::openSettings);

    auto* side = new QWidget;
    side->setObjectName("side");
    auto* sl = new QVBoxLayout(side);
    sl->setContentsMargins(10, 10, 0, 10);
    logoBtn_ = new QToolButton;
    logoBtn_->setObjectName("logoBtn");
    logoBtn_->setIconSize(QSize(210, 24));
    logoBtn_->setToolTip(T("Menu do EmuCenter"));
    logoBtn_->setCursor(Qt::PointingHandCursor);
    logoBtn_->setPopupMode(QToolButton::InstantPopup);
    logoMenu_ = new QMenu(this);
    logoBtn_->setMenu(logoMenu_);
    connect(logoMenu_, &QMenu::aboutToShow, this, &MainWindow::fillLogoMenu);
    sl->addWidget(logoBtn_);
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
    side->setMinimumWidth(320);

    auto* main = new QWidget;
    main->setObjectName("root");
    auto* ml = new QVBoxLayout(main);
    ml->setContentsMargins(0, 0, 0, 12);
    banner_ = new Banner;
    ml->addWidget(banner_);
    auto* body = new QVBoxLayout;
    body->setContentsMargins(24, 10, 24, 0);
    ml->addLayout(body, 1);
    title_ = new QLabel(T("Selecione um jogo"));
    title_->setObjectName("title");
    info_ = new QLabel;
    info_->setObjectName("mono");
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    playBtn_ = new QPushButton(T("JOGAR"));
    playBtn_->setObjectName("play");
    playBtn_->setCursor(Qt::PointingHandCursor);
    installBtn_ = new QPushButton;
    status_ = new QLabel;
    status_->setTextFormat(Qt::RichText);
    row->addWidget(playBtn_);
    row->addWidget(installBtn_);
    row->addWidget(status_, 1);
    desc_ = new QTextBrowser;
    desc_->setOpenExternalLinks(true);
    body->addWidget(title_);
    body->addWidget(info_);
    body->addSpacing(6);
    body->addLayout(row);
    body->addSpacing(6);
    body->addWidget(desc_, 1);

    auto* split = new QSplitter;
    split->addWidget(side);
    split->addWidget(main);
    split->setStretchFactor(1, 1);
    split->setCollapsible(0, false);
    split->setHandleWidth(1);

    bp_ = new BigPicture(&meta_);
    stack_ = new QStackedWidget;
    stack_->addWidget(split);
    stack_->addWidget(bp_);

    bg_ = new BgWidget;
    auto* cl = new QVBoxLayout(bg_);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    cl->addWidget(bar);
    cl->addWidget(stack_, 1);
    setCentralWidget(bg_);

    metaLbl_ = new QLabel;
    metaBar_ = new QProgressBar;
    metaBar_->setFixedWidth(220);
    metaBar_->hide();
    statusBar()->addPermanentWidget(metaLbl_);
    statusBar()->addPermanentWidget(metaBar_);
    statusBar()->showMessage(T("Pronto."));

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

    connect(playBtn_, &QPushButton::clicked, this, &MainWindow::play);
    connect(installBtn_, &QPushButton::clicked, this, &MainWindow::installMissing);
    connect(tree_, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onSelect);
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* it) { if (it->parent()) play(); });
    connect(tree_, &QTreeWidget::itemClicked, this, [](QTreeWidgetItem* it) { if (!it->parent()) it->setExpanded(!it->isExpanded()); });
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, &MainWindow::gameMenu);

    connect(bp_, &BigPicture::playRequested, this, [this](const QString& key) {
        if (const Game* g = gameByKey(key)) playGame(*g);
    });
    connect(bp_, &BigPicture::exitRequested, this, &MainWindow::leaveBigPicture);
    connect(bp_, &BigPicture::fullscreenRequested, this, &MainWindow::toggleFullscreen);

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

    bpTimer_ = new QTimer(this);
    bpTimer_->setSingleShot(true);
    connect(bpTimer_, &QTimer::timeout, this, [this] { if (stack_->currentIndex() == 1) bp_->refreshVisuals(); });

    connect(&meta_, &Metadata::imageReady, this, [this](const QString& key, int kind, const QPixmap& pm) {
        if (kind == Metadata::Icon) {
            const QIcon ic(pm.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            icons_[key] = ic;
            if (auto* it = items_.value(key)) it->setIcon(0, ic);
        } else if (const Game* g = current(); g && g->key() == key) {
            banner_->setImage(pm);
        }
        if (stack_->currentIndex() == 1) bpTimer_->start(150);
    });
    connect(&meta_, &Metadata::nameReady, this, [this](const QString& key, const QString& name) {
        for (auto& g : games_) if (g.key() == key) g.title = name;
        if (auto* it = items_.value(key)) it->setText(0, name);
        if (const Game* g = current(); g && g->key() == key) title_->setText(name);
        tree_->viewport()->update();
    });
    connect(&meta_, &Metadata::textReady, this, [this](const QString& key, const QString& t) {
        if (const Game* g = current(); g && g->key() == key) desc_->setPlainText(t);
        if (stack_->currentIndex() == 1) bpTimer_->start(150);
    });
    connect(&meta_, &Metadata::progress, this, [this](int d, int t) {
        metaBar_->setRange(0, t);
        metaBar_->setValue(d);
        metaBar_->setVisible(t > 0 && d < t);
        metaLbl_->setText(t > 0 && d < t ? T("Metadados: %1/%2").arg(d).arg(t) : QString());
    });
    connect(&meta_, &Metadata::batchFinished, this, [this](int, int dl) {
        metaBar_->hide();
        metaLbl_->clear();
        if (dl > 0) {
            statusBar()->showMessage(T("Metadados atualizados."), 6000);
            notify("EmuCenter", T("Metadados dos jogos atualizados (%1 itens novos).").arg(dl));
        }
    });

    connect(&updater_, &Updater::checked, this, [this](bool ok, bool newer, const QString& tag, const QString& msg) {
        const bool manual = manualUpdate_;
        manualUpdate_ = false;
        if (ok && newer) { askInstallUpdate(tag); return; }
        if (manual) QMessageBox::information(this, "EmuCenter", msg);
    });
    connect(&updater_, &Updater::progress, this, [this](int p, const QString& t) {
        statusBar()->showMessage(t);
        metaBar_->setRange(0, p < 0 ? 0 : 100);
        if (p >= 0) metaBar_->setValue(p);
        metaBar_->setVisible(p < 100);
    });
    connect(&updater_, &Updater::failed, this, [this](const QString& m) {
        metaBar_->hide();
        QMessageBox::warning(this, "EmuCenter", m);
    });

    metaTimer_ = new QTimer(this);
    connect(metaTimer_, &QTimer::timeout, this, [this] { refreshMetadata(false); });
    applyGeneral();
    applyLogo();
    applyBackground();

    rescanImpl(false);
    DiscordRpc::instance().applySettings();
    DiscordRpc::instance().setBrowsing(games_.size());

    if (!st.value("welcomeDone", false).toBool()) QTimer::singleShot(250, this, &MainWindow::showWelcome);
    if (st.value("autoMeta", true).toBool())
        QTimer::singleShot(st.value("startDelayMeta", 3).toInt() * 1000, this, [this] { meta_.enqueueAll(games_); });
    if (st.value("autoEmuUpdate", false).toBool())
        QTimer::singleShot(st.value("startDelayUpdate", 10).toInt() * 1000, this, &MainWindow::checkEmuUpdates);
    if (st.value("autoUpdateCheck", true).toBool())
        QTimer::singleShot(st.value("startDelayAppUpdate", 8).toInt() * 1000, this, [this] {
            updater_.check(QSettings().value("updatePre", false).toBool());
        });
    if (!Theme::notice().isEmpty()) QTimer::singleShot(1500, this, [this] { notify("EmuCenter", Theme::notice()); });
    if (st.value("startBigPicture", false).toBool()) QTimer::singleShot(0, this, &MainWindow::enterBigPicture);
}

void MainWindow::applyGeneral() {
    const int h = QSettings().value("metaHours", 0).toInt();
    if (h > 0) metaTimer_->start(h * 3600 * 1000);
    else metaTimer_->stop();
}

void MainWindow::applyBackground() {
    bg_->setImage(Theme::backgroundImage(), Theme::backgroundOpacity());
}

void MainWindow::notify(const QString& title, const QString& msg) {
    if (!QSettings().value("notify", true).toBool()) return;
    if (tray_ && tray_->isVisible()) tray_->showMessage(title, msg, QSystemTrayIcon::Information, 6000);
    else statusBar()->showMessage(msg, 8000);
}

void MainWindow::toggleMaximize() {
    if (isMaximized()) { showNormal(); if (maxBtn_) maxBtn_->setText(QString::fromUtf8("▢")); }
    else { showMaximized(); if (maxBtn_) maxBtn_->setText(QString::fromUtf8("❐")); }
}

void MainWindow::toggleFullscreen() {
    if (isFullScreen()) showNormal();
    else showFullScreen();
}

bool MainWindow::eventFilter(QObject* o, QEvent* ev) {
    if (!frameless_ || isMaximized() || isFullScreen() || ev->type() != QEvent::MouseButtonPress) return false;
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
    QSettings().setValue("window/geometry", saveGeometry());
    if (!quitting_ && tray_ && tray_->isVisible() && QSettings().value("background", true).toBool()) {
        hide();
        e->ignore();
        if (!shownTrayHint_) {
            shownTrayHint_ = true;
            tray_->showMessage("EmuCenter", T("O EmuCenter continua em segundo plano. Clique no ícone da bandeja para abrir."), QSystemTrayIcon::Information, 5000);
        }
        return;
    }
    e->accept();
    qApp->quit();
}

void MainWindow::restartApp() {
    QSettings().setValue("window/geometry", saveGeometry());
    quitting_ = true;
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--restarted"});
    qApp->quit();
}

void MainWindow::promptRestart() {
    if (QMessageBox::question(this, "EmuCenter", T("Reiniciar o EmuCenter agora para aplicar o idioma?")) == QMessageBox::Yes) restartApp();
}

void MainWindow::fillLogoMenu() {
    QMenu* m = logoMenu_;
    m->clear();
    m->addAction(T("Configurações…"), this, &MainWindow::openSettings);
    m->addAction(T("Big Picture"), this, &MainWindow::enterBigPicture);
    m->addSeparator();
    m->addAction(T("Adicionar pasta de ROMs…"), this, &MainWindow::addRomFolder);
    m->addAction(T("Importar arquivo(s) de ROM…"), this, &MainWindow::importRoms);
    m->addAction(T("Procurar jogos novamente"), this, &MainWindow::rescan);
    m->addAction(T("Atualizar metadados"), this, [this] { refreshMetadata(false); });
    m->addAction(T("Refazer metadados que falharam"), this, [this] { refreshMetadata(true); });
    m->addAction(T("Verificar atualizações dos emuladores"), this, &MainWindow::checkEmuUpdates);
    m->addAction(T("Verificar atualizações do EmuCenter"), this, &MainWindow::checkAppUpdate);
    m->addAction(T("Assistente de configuração"), this, &MainWindow::showWelcome);
    m->addSeparator();

    QMenu* lang = m->addMenu(T("Idioma"));
    const auto langs = I18n::languages();
    for (const auto& l : langs) {
        QAction* a = lang->addAction(l.second);
        a->setCheckable(true);
        a->setChecked(I18n::lang() == l.first);
        connect(a, &QAction::triggered, this, [this, code = l.first] {
            I18n::setLang(code);
            promptRestart();
        });
    }
    QMenu* th = m->addMenu(T("Tema"));
    const QList<Palette> pals = Theme::builtins();
    const QString curTheme = QSettings().value("theme", "grafite").toString();
    for (const auto& p : pals) {
        QAction* a = th->addAction(I18n::tr(p.name));
        a->setCheckable(true);
        a->setChecked(curTheme == p.id);
        connect(a, &QAction::triggered, this, [this, id = p.id] {
            QSettings().setValue("theme", id);
            Theme::apply();
            applyLogo();
            rebuildTree();
            banner_->update();
        });
    }
    m->addSeparator();
    m->addAction(T("Abrir pasta de dados"), this, [] {
        const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(d);
        QDesktopServices::openUrl(QUrl::fromLocalFile(d));
    });
    m->addAction(T("Abrir pasta dos emuladores"), this, [this] {
        QDir().mkpath(emu_.root());
        QDesktopServices::openUrl(QUrl::fromLocalFile(emu_.root()));
    });
    m->addAction(T("Sobre o EmuCenter"), this, [this] {
        QMessageBox::about(this, "EmuCenter", "<b>EmuCenter " + Updater::currentVersion() + "</b><br>" + T("Central leve de emuladores.") + "<br><br>" +
                                                  T("Imagens: IGDB e libretro-thumbnails  •  Descrições: IGDB e Wikipédia"));
    });
    m->addSeparator();
    m->addAction(T("Sair"), this, [this] { quitting_ = true; close(); });
}

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
    if (dirs.isEmpty()) {
        addRomFolder();
        dirs = QSettings().value("romDirs").toStringList();
        if (dirs.isEmpty()) return;
    }
    QStringList exts;
    for (const auto& sd : Catalog::systems())
        for (const auto& e : sd.exts + sd.hintExts) if (!exts.contains("*." + e)) exts << "*." + e;
    const QStringList files = QFileDialog::getOpenFileNames(this, T("Escolha as ROMs para importar"), {}, T("ROMs (%1)").arg(exts.join(' ')));
    if (files.isEmpty()) return;

    QString dest = dirs.constFirst();
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
    DiscordRpc::instance().setBrowsing(games_.size());
    meta_.loadCachedIcons(games_);
    if (!network) return;
    if (QSettings().value("autoMeta", true).toBool()) {
        meta_.enqueueAll(games_);
    } else {
        for (const auto& g : std::as_const(games_)) if (!icons_.contains(g.key())) meta_.requestIcon(g);
    }
}

void MainWindow::applyLogo() {
    if (!logoBtn_) return;
    const QString custom = Theme::customLogo();
    QPixmap px = custom.isEmpty() ? Theme::logo(QPixmap(":/logo_text.png")) : QPixmap(custom);
    px = px.scaledToHeight(48, Qt::SmoothTransformation);
    if (px.width() > 460) px = px.scaledToWidth(460, Qt::SmoothTransformation);
    logoBtn_->setIcon(QIcon(px));
    logoBtn_->setIconSize(QSize(qMin(220, px.width() / 2), px.height() / 2));
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

void MainWindow::checkAppUpdate() {
    manualUpdate_ = true;
    statusBar()->showMessage(T("Verificando…"), 4000);
    updater_.check(QSettings().value("updatePre", false).toBool());
}

void MainWindow::askInstallUpdate(const QString& tag) {
    const auto r = QMessageBox::question(this, "EmuCenter", T("A versão %1 do EmuCenter está disponível. Baixar e instalar agora? O app reinicia sozinho.").arg(tag));
    if (r == QMessageBox::Yes) updater_.install();
}

bool MainWindow::canPlay(const Game& g) const {
    const SystemDef* s = Catalog::system(g.systemId);
    if (!s) return false;
    for (const auto& l : s->emus) if (emu_.available(l)) return true;
    return false;
}

const Game* MainWindow::gameByKey(const QString& key) const {
    for (const Game& g : games_) if (g.key() == key) return &g;
    return nullptr;
}

void MainWindow::refreshBigPicture() {
    bp_->setGames(games_, [this](const Game& g) { return canPlay(g); });
}

void MainWindow::enterBigPicture() {
    refreshBigPicture();
    stack_->setCurrentIndex(1);
    if (const Game* g = current()) bp_->focusGame(g->key());
    bp_->setFocus();
}

void MainWindow::leaveBigPicture() {
    stack_->setCurrentIndex(0);
    if (isFullScreen()) showNormal();
}

void MainWindow::rebuildTree() {
    const QString keep = current() ? current()->key() : QString();
    const bool showPlay = QSettings().value("showPlaytimeInList", false).toBool();
    tree_->blockSignals(true);
    tree_->clear();
    items_.clear();
    int n = 0;
    for (const auto& s : Catalog::systems()) {
        QTreeWidgetItem* top = nullptr;
        bool ok = false;
        for (const auto& l : s.emus) if (emu_.available(l)) ok = true;
        for (const auto& g : std::as_const(games_)) {
            if (g.systemId != s.id) continue;
            if (!top) {
                top = new QTreeWidgetItem(tree_, {s.name});
                top->setFlags(Qt::ItemIsEnabled);
            }
            auto* it = new QTreeWidgetItem(top, {g.title});
            it->setData(0, Qt::UserRole, g.key());
            it->setData(0, GameDelegate::Parity, n++ % 2);
            it->setData(0, GameDelegate::NoEmu, !ok);
            if (showPlay) {
                const qint64 secs = Library::playSeconds(g);
                if (secs > 0) it->setData(0, GameDelegate::PlayText, Library::formatDuration(secs));
            }
            if (icons_.contains(g.key())) it->setIcon(0, icons_[g.key()]);
            items_[g.key()] = it;
        }
        if (top) {
            top->setText(0, QString("%1  (%2)").arg(s.name).arg(top->childCount()));
            top->setExpanded(true);
        }
    }
    tree_->blockSignals(false);
    if (items_.contains(keep)) tree_->setCurrentItem(items_[keep]);
    else onSelect();
    tree_->viewport()->update();
    if (stack_ && stack_->currentIndex() == 1) refreshBigPicture();
}

const Game* MainWindow::current() const {
    auto* it = tree_->currentItem();
    if (!it || !it->parent()) return nullptr;
    return gameByKey(it->data(0, Qt::UserRole).toString());
}

void MainWindow::onSelect() {
    const Game* g = current();
    banner_->setImage({});
    desc_->clear();
    if (!g) {
        title_->setText(T("Selecione um jogo"));
        info_->clear();
        status_->clear();
        playBtn_->setEnabled(false);
        installBtn_->hide();
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

    if (s->emus.isEmpty()) {
        status_->setText("<span style='color:" + P.warn.name() + "'><b>" + T("Sem emulador conhecido") + "</b><br>" +
                         T("Ainda não existe um emulador utilizável para este console.") + "</span>");
        playBtn_->setEnabled(false);
        installBtn_->hide();
        return;
    }

    auto name = [](const EmuLink& l) {
        QString n = Catalog::emu(l.emuId)->name;
        return l.core.isEmpty() ? n : n + " (" + QString(l.core).remove("_libretro") + ")";
    };
    const bool primaryOk = emu_.available(s->emus.constFirst());
    bool anyOk = primaryOk;
    QString html;
    if (primaryOk)
        html = "<span style='color:" + P.ok.name() + "'><b>✔ " + T("Pronto") + "</b> — " + name(s->emus.constFirst()).toHtmlEscaped() + "</span>";
    else
        html = "<span style='color:" + P.err.name() + "'><b>" + T("1 erro") + "</b><br>need &gt;&gt; " + name(s->emus.constFirst()).toHtmlEscaped() + "</span>";
    for (int i = 1; i < s->emus.size(); ++i) {
        const bool ok = emu_.available(s->emus[i]);
        anyOk = anyOk || ok;
        html += "<br><span style='color:" + P.warn.name() + "'>or &gt;&gt; " + name(s->emus[i]).toHtmlEscaped() + (ok ? " ✔" : "") + "</span>";
    }
    status_->setText(html);
    playBtn_->setEnabled(anyOk);
    installBtn_->setVisible(!primaryOk);
    installBtn_->setText(T("Instalar ") + Catalog::emu(s->emus.constFirst().emuId)->name);
}

void MainWindow::installMissing() {
    const Game* g = current();
    if (!g) return;
    const SystemDef* s = Catalog::system(g->systemId);
    if (!s || s->emus.isEmpty()) return;
    const EmuLink l = s->emus.constFirst();
    emu_.install(l.emuId, l.core);
}

void MainWindow::play() {
    if (const Game* g = current()) playGame(*g);
}

void MainWindow::playGame(const Game& g) {
    const SystemDef* s = Catalog::system(g.systemId);
    if (!s) return;
    if (s->emus.isEmpty()) {
        QMessageBox::information(this, "EmuCenter", T("Ainda não existe um emulador utilizável para este console."));
        return;
    }
    const auto opts = Library::options(g);

    QList<EmuLink> order;
    for (const EmuLink& l : s->emus) if (!opts.emu.isEmpty() && opts.emu == l.emuId + "|" + l.core) order << l;
    for (const EmuLink& l : s->emus) if (opts.emu != l.emuId + "|" + l.core) order << l;

    for (const EmuLink& l : std::as_const(order)) {
        QString prog;
        QStringList args;
        if (!emu_.available(l) || !emu_.buildCommand(l, g.path, prog, args)) continue;
        if (!opts.args.trimmed().isEmpty()) args << QProcess::splitCommand(opts.args);
        auto* p = new QProcess(this);
        const Game game = g;
        auto* t = new QElapsedTimer;
        t->start();
        connect(p, &QProcess::finished, this, [this, p, t, game]() {
            Library::addPlayTime(game, t->elapsed() / 1000);
            delete t;
            p->deleteLater();
            DiscordRpc::instance().stopPlaying(games_.size());
            refreshStatus();
        });
        p->setWorkingDirectory(QFileInfo(prog).absolutePath());
        p->start(prog, args);
        DiscordRpc::instance().setPlaying(g.title, g.systemId, s->name);
        statusBar()->showMessage(T("Executando %1…").arg(g.title));
        return;
    }
    const EmuLink primary = s->emus.constFirst();
    emu_.install(primary.emuId, primary.core);
}

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
        const QList<Game> one{g};
        meta_.enqueueAll(one);
        meta_.requestBanner(g);
        meta_.requestText(g);
    });
    m.addAction(T("Zerar tempo de jogo"), this, [this, g] { Library::resetPlayTime(g); refreshStatus(); });
    m.exec(tree_->viewport()->mapToGlobal(pos));
}

void MainWindow::gameSettings(const Game& g) {
    GameSettingsDialog d(g, &emu_, &meta_, &igdb_, this);
    if (d.exec() != QDialog::Accepted || !d.changed()) return;
    rescanImpl(false);
    const QList<Game> one{g};
    meta_.enqueueAll(one);
    onSelect();
}

void MainWindow::showWelcome() {
    WelcomeDialog d(&emu_, this);
    d.exec();
    applyLogo();
    applyBackground();
    rescan();
    DiscordRpc::instance().applySettings();
    if (d.languageChanged()) restartApp();
}

void MainWindow::openSettings() { openSettingsTab(0); }

void MainWindow::openSettingsTab(int tab) {
    SettingsDialog d(&emu_, &meta_, &igdb_, &updater_, this, tab);
    connect(&d, &SettingsDialog::romFoldersChanged, this, &MainWindow::rescan);
    connect(&d, &SettingsDialog::emulatorsChanged, this, [this] { rebuildTree(); refreshStatus(); });
    connect(&d, &SettingsDialog::appearanceChanged, this, [this] {
        applyLogo();
        applyBackground();
        rebuildTree();
        banner_->update();
        refreshStatus();
    });
    connect(&d, &SettingsDialog::generalChanged, this, &MainWindow::applyGeneral);
    connect(&d, &SettingsDialog::metadataRequested, this, &MainWindow::refreshMetadata);
    connect(&d, &SettingsDialog::languageChanged, this, [this, &d] { d.accept(); promptRestart(); });
    connect(&d, &SettingsDialog::welcomeRequested, this, [this, &d] { d.accept(); QTimer::singleShot(0, this, &MainWindow::showWelcome); });
    d.exec();
    rebuildTree();
    refreshStatus();
}
