#include "GameSettingsDialog.h"
#include "I18n.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

GameSettingsDialog::GameSettingsDialog(const Game& g, EmulatorManager* emu, Metadata* meta, Igdb* igdb, QWidget* parent)
    : QDialog(parent), game_(g), emu_(emu), meta_(meta), igdb_(igdb) {
    setWindowTitle(T("Configurações do jogo") + " — " + g.title);
    resize(860, 640);
    auto* l = new QVBoxLayout(this);
    auto* tabs = new QTabWidget;
    tabs->addTab(buildInfo(), T("Informações"));
    tabs->addTab(buildImages(), T("Imagens"));
    tabs->addTab(buildRun(), T("Execução"));
    l->addWidget(tabs, 1);

    status_ = new QLabel; status_->setObjectName("muted"); status_->setWordWrap(true);
    l->addWidget(status_);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto* reset = bb->addButton(T("Restaurar automático"), QDialogButtonBox::ResetRole);
    l->addWidget(bb);
    connect(bb, &QDialogButtonBox::accepted, this, &GameSettingsDialog::save);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(reset, &QPushButton::clicked, this, [this] {
        meta_->resetGame(game_);
        changed_ = true;
        accept();
    });
}

QWidget* GameSettingsDialog::buildInfo() {
    auto* w = new QWidget; auto* l = new QVBoxLayout(w);
    QSettings s;
    auto* f = new QFormLayout;
    auto* internal = new QLabel(game_.internalName.isEmpty() ? QString::fromUtf8("—") : game_.internalName);
    internal->setObjectName("muted");
    auto* file = new QLabel(game_.rawName); file->setObjectName("muted");
    name_ = new QLineEdit(s.value("game/" + game_.key() + "/name").toString());
    name_->setPlaceholderText(game_.title);
    f->addRow(T("Arquivo:"), file);
    f->addRow(T("Nome dentro da ROM:"), internal);
    f->addRow(T("Nome exibido:"), name_);
    l->addLayout(f);

    auto* row = new QHBoxLayout;
    searchBtn_ = new QPushButton(T("Buscar no IGDB"));
    searchBtn_->setEnabled(igdb_->configured());
    if (!igdb_->configured()) searchBtn_->setToolTip(T("Configure o IGDB em Configurações › Metadados."));
    row->addWidget(searchBtn_);
    row->addWidget(new QLabel(T("Escolha o jogo certo na lista para preencher nome, descrição e imagens.")), 1);
    l->addLayout(row);
    connect(searchBtn_, &QPushButton::clicked, this, &GameSettingsDialog::searchIgdb);

    resultList_ = new QListWidget; resultList_->setMaximumHeight(150);
    l->addWidget(resultList_);
    connect(resultList_, &QListWidget::itemClicked, this, &GameSettingsDialog::pickResult);

    l->addWidget(new QLabel(T("Descrição:")));
    desc_ = new QTextEdit;
    desc_->setAcceptRichText(false);
    QString cur = s.value("game/" + game_.key() + "/desc").toString();
    if (cur.isEmpty()) {
        QFile cf(meta_->cacheDir() + "/" + game_.key() + "_" + I18n::lang() + ".txt");
        if (cf.open(QIODevice::ReadOnly)) cur = QString::fromUtf8(cf.readAll());
    }
    desc_->setPlainText(cur);
    origDesc_ = cur;
    l->addWidget(desc_, 1);
    return w;
}

void GameSettingsDialog::searchIgdb() {
    const QString q = name_->text().trimmed().isEmpty() ? (game_.internalName.isEmpty() ? Library::cleanTitle(game_.rawName) : game_.internalName)
                                                         : name_->text().trimmed();
    status_->setText(T("Buscando no IGDB…"));
    resultList_->clear();
    igdb_->search(q, game_.systemId, [this](QList<IgdbGame> l, QString err) {
        results_ = l;
        for (const auto& g : l)
            resultList_->addItem(g.name + (g.year ? QString(" (%1)").arg(g.year) : QString()));
        status_->setText(!err.isEmpty() ? err : (l.isEmpty() ? T("Nada encontrado. Tente editar o nome e buscar de novo.") : T("%1 resultado(s).").arg(l.size())));
    });
}

void GameSettingsDialog::pickResult() {
    const int i = resultList_->currentRow();
    if (i < 0 || i >= results_.size()) return;
    const IgdbGame& g = results_[i];
    pickedId_ = g.id;
    name_->setText(g.name);
    if (!g.summary.isEmpty()) desc_->setPlainText(g.summary);
    fillOptions(g);
    status_->setText(T("Selecionado: %1. Veja a aba Imagens para escolher ícone e banner.").arg(g.name));
}

QWidget* GameSettingsDialog::buildImages() {
    auto* w = new QWidget; auto* l = new QHBoxLayout(w);

    auto column = [&](const QString& title, QLabel*& prev, QListWidget*& opts, QSize prevSize, QSize iconSize, bool isIcon) {
        auto* box = new QGroupBox(title); auto* v = new QVBoxLayout(box);
        prev = new QLabel; prev->setFixedSize(prevSize); prev->setAlignment(Qt::AlignCenter);
        prev->setStyleSheet("border: 1px solid #555; border-radius: 8px;");
        v->addWidget(prev, 0, Qt::AlignHCenter);
        auto* row = new QHBoxLayout;
        auto* file = new QPushButton(T("Arquivo…")); auto* rst = new QPushButton(T("Automático"));
        row->addWidget(file); row->addWidget(rst);
        v->addLayout(row);
        v->addWidget(new QLabel(T("Opções do IGDB:")));
        opts = new QListWidget;
        opts->setViewMode(QListView::IconMode); opts->setIconSize(iconSize); opts->setResizeMode(QListView::Adjust);
        opts->setMovement(QListView::Static); opts->setWrapping(true);
        v->addWidget(opts, 1);
        l->addWidget(box, isIcon ? 2 : 3);

        connect(file, &QPushButton::clicked, this, [this, isIcon] {
            const QString p = QFileDialog::getOpenFileName(this, T("Escolha uma imagem"), {}, "Imagens (*.png *.jpg *.jpeg *.webp *.bmp)");
            if (p.isEmpty()) return;
            QPixmap pm(p);
            if (pm.isNull()) return;
            if (isIcon) { pendIcon_ = pm; resetIcon_ = false; showIcon(pm); } else { pendBanner_ = pm; resetBanner_ = false; showBanner(pm); }
        });
        connect(rst, &QPushButton::clicked, this, [this, isIcon] {
            if (isIcon) { pendIcon_ = QPixmap(); resetIcon_ = true; iconPrev_->clear(); } else { pendBanner_ = QPixmap(); resetBanner_ = true; bannerPrev_->clear(); }
        });
        connect(opts, &QListWidget::itemClicked, this, [this, isIcon](QListWidgetItem* it) {
            status_->setText(T("Baixando imagem…"));
            igdb_->getImage(QUrl(it->data(Qt::UserRole).toString()), [this, isIcon](QPixmap pm) {
                if (pm.isNull()) { status_->setText(T("Não consegui baixar essa imagem.")); return; }
                status_->setText({});
                if (isIcon) { pendIcon_ = pm; resetIcon_ = false; showIcon(pm); } else { pendBanner_ = pm; resetBanner_ = false; showBanner(pm); }
            });
        });
    };
    column(T("Ícone / capa"), iconPrev_, iconOpts_, QSize(150, 150), QSize(90, 120), true);
    column(T("Banner"), bannerPrev_, bannerOpts_, QSize(300, 150), QSize(160, 90), false);

    auto cur = [&](const QString& suffix) {
        QString p = meta_->customDir() + "/" + game_.key() + suffix;
        if (!QFile::exists(p)) p = meta_->cacheDir() + "/" + game_.key() + suffix;
        return QPixmap(p);
    };
    if (QPixmap p = cur("_icon.png"); !p.isNull()) showIcon(p);
    if (QPixmap p = cur("_banner.png"); !p.isNull()) showBanner(p);
    return w;
}

void GameSettingsDialog::showIcon(const QPixmap& pm) { iconPrev_->setPixmap(pm.scaled(iconPrev_->size() - QSize(4, 4), Qt::KeepAspectRatio, Qt::SmoothTransformation)); }
void GameSettingsDialog::showBanner(const QPixmap& pm) { bannerPrev_->setPixmap(pm.scaled(bannerPrev_->size() - QSize(4, 4), Qt::KeepAspectRatio, Qt::SmoothTransformation)); }

void GameSettingsDialog::fillOptions(const IgdbGame& g) {
    iconOpts_->clear(); bannerOpts_->clear();
    auto add = [this](QListWidget* lw, const QString& id, const QString& thumb, const QString& full) {
        auto* it = new QListWidgetItem(lw);
        it->setData(Qt::UserRole, Igdb::imageUrl(id, full).toString());
        it->setSizeHint(lw->iconSize() + QSize(12, 12));
        igdb_->getImage(Igdb::imageUrl(id, thumb), [it](QPixmap pm) { if (!pm.isNull()) it->setIcon(QIcon(pm)); });
    };
    if (!g.cover.isEmpty()) add(iconOpts_, g.cover, "cover_big", "cover_big");
    for (const QString& a : g.artworks) { add(bannerOpts_, a, "screenshot_med", "1080p"); }
    for (const QString& s : g.screenshots) { add(bannerOpts_, s, "screenshot_med", "1080p"); }
}

QWidget* GameSettingsDialog::buildRun() {
    auto* w = new QWidget; auto* f = new QFormLayout(w);
    const auto o = Library::options(game_);
    const SystemDef* s = Catalog::system(game_.systemId);
    emuBox_ = new QComboBox;
    emuBox_->addItem(T("Automático (recomendado primeiro)"), "");
    for (const EmuLink& l : s->emus)
        emuBox_->addItem(Catalog::emu(l.emuId)->name + (l.core.isEmpty() ? "" : " (" + QString(l.core).remove("_libretro") + ")"), l.emuId + "|" + l.core);
    emuBox_->setCurrentIndex(qMax(0, emuBox_->findData(o.emu)));
    args_ = new QLineEdit(o.args);
    args_->setPlaceholderText(T("ex.: --fullscreen  (adicionado ao final do comando)"));
    f->addRow(T("Emulador:"), emuBox_);
    f->addRow(T("Comandos extras:"), args_);
    return w;
}

void GameSettingsDialog::save() {
    QSettings s;
    const QString k = game_.key();
    QDir().mkpath(meta_->customDir());

    const QString nm = name_->text().trimmed();
    if (nm.isEmpty()) s.remove("game/" + k + "/name"); else s.setValue("game/" + k + "/name", nm);

    const QString d = desc_->toPlainText().trimmed();
    if (d != origDesc_.trimmed()) {
        if (d.isEmpty()) s.remove("game/" + k + "/desc"); else s.setValue("game/" + k + "/desc", d);
    }
    if (pickedId_) {
        s.setValue("game/" + k + "/igdbId", pickedId_);

        QFile::remove(meta_->cacheDir() + "/" + k + "_igdb.json");
        QFile::remove(meta_->cacheDir() + "/" + k + "_igdb.json.none");
        for (const char* suf : {"_icon.png", "_banner.png", "_pt.txt", "_en.txt"}) { QFile::remove(meta_->cacheDir() + "/" + k + suf); QFile::remove(meta_->cacheDir() + "/" + k + suf + ".none"); }
    }
    auto store = [&](const char* suffix, const QPixmap& pm, bool reset) {
        const QString p = meta_->customDir() + "/" + k + suffix;
        if (reset) { QFile::remove(p); QFile::remove(meta_->cacheDir() + "/" + k + suffix); QFile::remove(meta_->cacheDir() + "/" + k + suffix + ".none"); }
        if (!pm.isNull()) pm.save(p, "PNG");
    };
    store("_icon.png", pendIcon_, resetIcon_);
    store("_banner.png", pendBanner_, resetBanner_);

    Library::setOptions(game_, {emuBox_->currentData().toString(), args_->text()});
    changed_ = true;
    accept();
}
