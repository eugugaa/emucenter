#include "WelcomeDialog.h"
#include "Catalog.h"
#include "I18n.h"
#include "Theme.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QStackedWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

WelcomeDialog::WelcomeDialog(EmulatorManager* emu, QWidget* parent) : QDialog(parent), emu_(emu) {
    setWindowTitle(T("Bem-vindo ao EmuCenter"));
    setModal(true);
    resize(820, 620);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    build();
    connect(emu_, &EmulatorManager::status, this, [this](const QString& m) { if (emuStatus_) emuStatus_->setText(m); });
    connect(emu_, &EmulatorManager::progress, this, [this](int p) {
        if (!bar_) return;
        bar_->setVisible(installing_ && p < 100);
        bar_->setRange(0, p < 0 ? 0 : 100);
        if (p >= 0) bar_->setValue(p);
    });
    connect(emu_, &EmulatorManager::finished, this, [this](bool ok, const QString& msg) {
        if (!installing_) return;
        if (!ok && emuStatus_) emuStatus_->setText(msg);
        installNext();
    });
}

void WelcomeDialog::build() {
    const int keep = page_;
    if (root_) { layout()->removeWidget(root_); delete root_; }
    root_ = new QWidget(this);
    layout()->addWidget(root_);
    auto* v = new QVBoxLayout(root_);
    v->setContentsMargins(34, 28, 34, 24);
    v->setSpacing(16);

    auto* head = new QHBoxLayout;
    auto* logo = new QLabel;
    logo->setPixmap(Theme::logo(QPixmap(":/logo_text.png")).scaledToHeight(34, Qt::SmoothTransformation));
    head->addWidget(logo);
    head->addStretch(1);
    v->addLayout(head);

    stack_ = new QStackedWidget;
    stack_->addWidget(pageLanguage());
    stack_->addWidget(pageRoms());
    stack_->addWidget(pageEmus());
    stack_->addWidget(pageOptions());
    stack_->addWidget(pageDone());
    v->addWidget(stack_, 1);

    auto* nav = new QHBoxLayout;
    skip_ = new QPushButton(T("Pular"));
    back_ = new QPushButton(T("Voltar"));
    next_ = new QPushButton(T("Próximo"));
    next_->setObjectName("play");
    nav->addWidget(skip_);
    nav->addStretch(1);
    nav->addWidget(back_);
    nav->addWidget(next_);
    v->addLayout(nav);

    connect(skip_, &QPushButton::clicked, this, [this] { QSettings().setValue("welcomeDone", true); accept(); });
    connect(back_, &QPushButton::clicked, this, [this] { go(page_ - 1); });
    connect(next_, &QPushButton::clicked, this, [this] {
        if (page_ >= stack_->count() - 1) { QSettings().setValue("welcomeDone", true); accept(); }
        else go(page_ + 1);
    });
    go(keep);
}

void WelcomeDialog::go(int page) {
    page_ = qBound(0, page, stack_->count() - 1);
    stack_->setCurrentIndex(page_);
    back_->setEnabled(page_ > 0);
    next_->setText(page_ == stack_->count() - 1 ? T("Concluir") : T("Próximo"));
    skip_->setVisible(page_ < stack_->count() - 1);
    if (page_ == 2) refreshEmus();
    if (page_ == 4) {
        const QList<Game> g = found_;
        QSet<QString> consoles;
        for (const Game& x : g) consoles.insert(x.systemId);
        summary_->setText(T("Tudo pronto! %1 jogo(s) em %2 console(s).").arg(g.size()).arg(consoles.size()));
    }
}

static QWidget* titled(const QString& title, const QString& sub, QVBoxLayout*& body) {
    auto* w = new QWidget;
    body = new QVBoxLayout(w);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(12);
    auto* t = new QLabel(title);
    t->setObjectName("title");
    t->setWordWrap(true);
    auto* s = new QLabel(sub);
    s->setObjectName("muted");
    s->setWordWrap(true);
    body->addWidget(t);
    body->addWidget(s);
    return w;
}

QWidget* WelcomeDialog::pageLanguage() {
    QVBoxLayout* b;
    auto* w = titled(T("Bem-vindo ao EmuCenter"), T("Vamos configurar tudo em poucos passos. Primeiro, escolha o idioma e o visual."), b);
    auto* lang = new QComboBox;
    const auto langs = I18n::languages();
    for (const auto& l : langs) lang->addItem(l.second, l.first);
    lang->setCurrentIndex(qMax(0, lang->findData(I18n::lang())));
    b->addWidget(new QLabel(T("Idioma:")));
    b->addWidget(lang);
    connect(lang, &QComboBox::currentIndexChanged, this, [this, lang] {
        I18n::setLang(lang->currentData().toString());
        langChanged_ = true;
        QMetaObject::invokeMethod(this, [this] { build(); }, Qt::QueuedConnection);
    });
    b->addSpacing(8);
    b->addWidget(new QLabel(T("Tema:")));
    auto* row = new QHBoxLayout;
    const QString cur = QSettings().value("theme", "grafite").toString();
    const QList<Palette> pals = Theme::builtins();
    for (const Palette& p : pals) {
        auto* btn = new QPushButton(I18n::tr(p.name));
        btn->setCheckable(true);
        btn->setChecked(cur == p.id);
        const QString id = p.id;
        connect(btn, &QPushButton::clicked, this, [this, id] {
            QSettings().setValue("theme", id);
            Theme::apply();
            QMetaObject::invokeMethod(this, [this] { build(); }, Qt::QueuedConnection);
        });
        row->addWidget(btn);
    }
    b->addLayout(row);
    b->addStretch(1);
    return w;
}

void WelcomeDialog::refreshRoms() {
    const QStringList dirs = QSettings().value("romDirs").toStringList();
    romList_->clear();
    romList_->addItems(dirs);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    found_ = Library::scan(dirs);
    QApplication::restoreOverrideCursor();
    QSet<QString> consoles;
    for (const Game& g : std::as_const(found_)) consoles.insert(g.systemId);
    romInfo_->setText(dirs.isEmpty() ? T("Nenhuma pasta ainda.") : T("Encontrei %1 jogo(s) em %2 console(s).").arg(found_.size()).arg(consoles.size()));
}

QWidget* WelcomeDialog::pageRoms() {
    QVBoxLayout* b;
    auto* w = titled(T("Suas ROMs"), T("Escolha a(s) pasta(s) onde ficam os seus jogos. O EmuCenter procura nas subpastas também e descobre o console de cada arquivo."), b);
    romList_ = new QListWidget;
    romInfo_ = new QLabel;
    romInfo_->setObjectName("mono");
    auto* add = new QPushButton(T("+ Adicionar pasta"));
    auto* rem = new QPushButton(T("Remover selecionada"));
    auto* row = new QHBoxLayout;
    row->addWidget(add);
    row->addWidget(rem);
    row->addStretch(1);
    b->addWidget(romList_, 1);
    b->addLayout(row);
    b->addWidget(romInfo_);
    connect(add, &QPushButton::clicked, this, [this] {
        const QString d = QFileDialog::getExistingDirectory(this, T("Escolha a pasta com suas ROMs"));
        if (d.isEmpty()) return;
        QSettings s;
        QStringList v = s.value("romDirs").toStringList();
        if (!v.contains(d)) v << d;
        s.setValue("romDirs", v);
        refreshRoms();
    });
    connect(rem, &QPushButton::clicked, this, [this] {
        auto* it = romList_->currentItem();
        if (!it) return;
        QSettings s;
        QStringList v = s.value("romDirs").toStringList();
        v.removeAll(it->text());
        s.setValue("romDirs", v);
        refreshRoms();
    });
    refreshRoms();
    return w;
}

QWidget* WelcomeDialog::pageEmus() {
    QVBoxLayout* b;
    auto* w = titled(T("Emuladores"), T("Estes são os emuladores recomendados para os consoles que encontrei. Marque os que quer baixar agora; os outros você instala depois em Configurações."), b);
    emuTable_ = new QTableWidget(0, 4);
    emuTable_->setHorizontalHeaderLabels({QString(), T("Emulador"), T("Consoles"), T("Estado")});
    emuTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    emuTable_->verticalHeader()->hide();
    emuTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    emuTable_->setSelectionMode(QAbstractItemView::NoSelection);
    emuTable_->setWordWrap(false);
    emuTable_->setTextElideMode(Qt::ElideRight);
    emuTable_->verticalHeader()->setDefaultSectionSize(44);
    b->addWidget(emuTable_, 1);
    emuStatus_ = new QLabel;
    emuStatus_->setObjectName("muted");
    emuStatus_->setWordWrap(true);
    bar_ = new QProgressBar;
    bar_->hide();
    auto* inst = new QPushButton(T("Baixar selecionados"));
    auto* row = new QHBoxLayout;
    row->addWidget(inst);
    row->addWidget(emuStatus_, 1);
    b->addLayout(row);
    b->addWidget(bar_);
    connect(inst, &QPushButton::clicked, this, [this] { installSelected(); });
    return w;
}

void WelcomeDialog::refreshEmus() {
    rows_.clear();
    emuTable_->setRowCount(0);
    for (const Game& g : std::as_const(found_)) {
        const SystemDef* s = Catalog::system(g.systemId);
        if (!s || s->emus.isEmpty()) continue;
        const EmuLink l = s->emus.constFirst();
        int idx = -1;
        for (int i = 0; i < rows_.size(); ++i) if (rows_[i].emuId == l.emuId) idx = i;
        if (idx < 0) { Row r; r.emuId = l.emuId; rows_ << r; idx = rows_.size() - 1; }
        if (!l.core.isEmpty() && !rows_[idx].cores.contains(l.core)) rows_[idx].cores << l.core;
        if (!rows_[idx].consoles.contains(s->name)) rows_[idx].consoles << s->name;
    }
    emuTable_->setRowCount(rows_.size());
    for (int i = 0; i < rows_.size(); ++i) {
        Row& r = rows_[i];
        const EmuDef* e = Catalog::emu(r.emuId);
        bool ready = !emu_->findExe(r.emuId).isEmpty();
        for (const QString& c : std::as_const(r.cores)) if (emu_->corePath(c).isEmpty()) ready = false;
        const bool autoSrc = EmulatorManager::hasAutoSource(*e);
        r.check = new QCheckBox;
        r.check->setChecked(!ready && autoSrc);
        r.check->setEnabled(!ready && autoSrc);
        auto* cell = new QWidget;
        auto* h = new QHBoxLayout(cell);
        h->setContentsMargins(10, 0, 0, 0);
        h->addWidget(r.check);
        emuTable_->setCellWidget(i, 0, cell);
        emuTable_->setItem(i, 1, new QTableWidgetItem(e->name));
        emuTable_->setItem(i, 2, new QTableWidgetItem(r.consoles.join(", ")));
        QString st;
        QColor col = Theme::current().ok;
        if (ready) st = T("instalado");
        else if (autoSrc) { st = T("pronto para baixar"); col = Theme::current().warn; }
        else { st = T("manual (site oficial)"); col = Theme::current().err; }
        auto* sti = new QTableWidgetItem(st);
        sti->setForeground(col);
        emuTable_->setItem(i, 3, sti);
        if (!ready && !autoSrc) {
            const QString url = e->homepage;
            emuTable_->item(i, 1)->setToolTip(url);
        }
    }
    emuTable_->resizeColumnToContents(0);
    emuTable_->resizeColumnToContents(1);
    emuTable_->resizeColumnToContents(3);
    if (rows_.isEmpty()) emuStatus_->setText(T("Nenhum console encontrado ainda. Volte e adicione sua pasta de ROMs."));
}

void WelcomeDialog::installSelected() {
    queue_.clear();
    for (const Row& r : std::as_const(rows_)) {
        if (!r.check || !r.check->isChecked()) continue;
        if (r.cores.isEmpty()) queue_ << qMakePair(r.emuId, QString());
        else for (const QString& c : r.cores) queue_ << qMakePair(r.emuId, c);
    }
    if (queue_.isEmpty()) { emuStatus_->setText(T("Nada selecionado para baixar.")); return; }
    installing_ = true;
    next_->setEnabled(false);
    back_->setEnabled(false);
    installNext();
}

void WelcomeDialog::installNext() {
    if (queue_.isEmpty()) {
        installing_ = false;
        next_->setEnabled(true);
        back_->setEnabled(true);
        bar_->hide();
        emuStatus_->setText(T("Downloads concluídos."));
        refreshEmus();
        return;
    }
    const auto job = queue_.takeFirst();
    emuStatus_->setText(T("Instalando %1…").arg(Catalog::emu(job.first)->name));
    emu_->install(job.first, job.second);
}

QWidget* WelcomeDialog::pageOptions() {
    QVBoxLayout* b;
    auto* w = titled(T("Recursos"), T("Ajuste o que o EmuCenter faz sozinho. Você pode mudar tudo depois em Configurações."), b);
    QSettings s;
    auto chk = [&](const QString& text, const char* key, bool def) {
        auto* c = new QCheckBox(text);
        c->setChecked(s.value(key, def).toBool());
        connect(c, &QCheckBox::toggled, this, [key](bool v) { QSettings().setValue(key, v); });
        b->addWidget(c);
    };
    chk(T("Baixar capas, banners e descrições dos jogos automaticamente (IGDB)"), "autoMeta", true);
    chk(T("Mostrar no Discord o que estou jogando"), "discord/enabled", false);
    chk(T("Verificar atualizações do EmuCenter ao iniciar"), "autoUpdateCheck", true);
    chk(T("Continuar em segundo plano (bandeja) ao fechar a janela"), "background", true);
    chk(T("Usar o nome gravado dentro da ROM"), "useInternalName", true);
    b->addStretch(1);
    return w;
}

QWidget* WelcomeDialog::pageDone() {
    QVBoxLayout* b;
    auto* w = titled(T("Tudo pronto!"), T("Dica: clique na logo para abrir o menu, use o botão Big Picture para a visão em tela cheia e o clique direito em um jogo para as opções dele."), b);
    summary_ = new QLabel;
    summary_->setObjectName("heading");
    summary_->setWordWrap(true);
    b->addWidget(summary_);
    b->addStretch(1);
    return w;
}
