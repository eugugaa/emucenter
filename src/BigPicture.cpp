#include "BigPicture.h"
#include "Catalog.h"
#include "I18n.h"
#include "Theme.h"
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {
const QSize kItem(214, 348);
const QSize kCover(188, 262);
enum { KeyRole = Qt::UserRole, NoEmuRole = Qt::UserRole + 1 };

QPixmap coverPixmap(const QString& path, const QSize& size) {
    if (path.isEmpty()) return {};
    const QString key = QString("bp:%1:%2x%3:%4").arg(path).arg(size.width()).arg(size.height()).arg(QFileInfo(path).lastModified().toSecsSinceEpoch());
    QPixmap pm;
    if (QPixmapCache::find(key, &pm)) return pm;
    const QPixmap src(path);
    if (src.isNull()) return {};
    const QPixmap scaled = src.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    pm = scaled.copy((scaled.width() - size.width()) / 2, (scaled.height() - size.height()) / 2, size.width(), size.height());
    QPixmapCache::insert(key, pm);
    return pm;
}

class CardDelegate : public QStyledItemDelegate {
public:
    CardDelegate(Metadata* meta, QObject* parent) : QStyledItemDelegate(parent), meta_(meta) {}
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return kItem; }
    void paint(QPainter* p, const QStyleOptionViewItem& o, const QModelIndex& i) const override {
        const Palette& P = Theme::current();
        const bool sel = o.state & QStyle::State_Selected;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setRenderHint(QPainter::SmoothPixmapTransform);
        QRect cover(o.rect.x() + (o.rect.width() - kCover.width()) / 2, o.rect.y() + 18, kCover.width(), kCover.height());
        if (sel) cover.adjust(-9, -12, 9, 12);
        QPainterPath path;
        path.addRoundedRect(cover, 14, 14);
        if (sel) {
            QColor glow = P.accent;
            glow.setAlpha(90);
            p->setPen(Qt::NoPen);
            p->setBrush(glow);
            p->drawRoundedRect(cover.adjusted(-7, -7, 7, 7), 19, 19);
        }
        p->save();
        p->setClipPath(path);
        const QPixmap pm = coverPixmap(meta_->iconPath(i.data(KeyRole).toString()), cover.size());
        if (pm.isNull()) {
            QLinearGradient g(cover.topLeft(), cover.bottomRight());
            g.setColorAt(0, P.panel2);
            g.setColorAt(1, P.panel);
            p->fillRect(cover, g);
            const QString title = i.data(Qt::DisplayRole).toString();
            QString initials;
            const QStringList words = title.split(' ', Qt::SkipEmptyParts);
            for (const QString& w : words) if (initials.size() < 3 && w[0].isLetterOrNumber()) initials += w[0].toUpper();
            p->setPen(P.muted);
            p->setFont(Theme::font(Theme::Title, 46, QFont::Bold));
            p->drawText(cover, Qt::AlignCenter, initials);
        } else {
            p->drawPixmap(cover.topLeft(), pm);
        }
        p->restore();
        if (i.data(NoEmuRole).toBool()) {
            p->setPen(Qt::NoPen);
            p->setBrush(P.err);
            p->drawRoundedRect(QRect(cover.right() - 34, cover.top() + 10, 24, 24), 12, 12);
            p->setPen(QColor(0x1b1b1b));
            p->setFont(Theme::font(Theme::Title, 16, QFont::Bold));
            p->drawText(QRect(cover.right() - 34, cover.top() + 10, 24, 24), Qt::AlignCenter, "!");
        }
        p->setPen(QPen(sel ? P.accent : P.border, sel ? 3 : 1));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(cover, 14, 14);
        p->setPen(sel ? P.text : P.muted);
        p->setFont(Theme::font(Theme::Title, sel ? 15 : 14, sel ? QFont::Bold : QFont::DemiBold));
        const QRect tr(o.rect.x() + 10, o.rect.y() + kCover.height() + 34, o.rect.width() - 20, 44);
        p->drawText(tr, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, o.fontMetrics.elidedText(i.data(Qt::DisplayRole).toString(), Qt::ElideRight, (tr.width()) * 2 - 24));
        p->restore();
    }
private:
    Metadata* meta_;
};
}

BigPicture::BigPicture(Metadata* meta, QWidget* parent) : QWidget(parent), meta_(meta) {
    setObjectName("bigpicture");
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 22, 36, 28);
    root->setSpacing(14);

    auto* top = new QHBoxLayout;
    back_ = new QPushButton(QString::fromUtf8("←  ") + T("Voltar"));
    full_ = new QPushButton(QString::fromUtf8("⛶"));
    full_->setToolTip(T("Tela cheia (F11)"));
    clock_ = new QLabel;
    clock_->setObjectName("heading");
    chipHost_ = new QWidget;
    chipLayout_ = new QHBoxLayout(chipHost_);
    chipLayout_->setContentsMargins(0, 0, 0, 0);
    chipLayout_->setSpacing(8);
    auto* scroll = new QScrollArea;
    scroll->setWidget(chipHost_);
    scroll->setWidgetResizable(true);
    scroll->setFixedHeight(54);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    chipHost_->setAutoFillBackground(false);
    top->addWidget(back_);
    top->addSpacing(8);
    top->addWidget(scroll, 1);
    top->addSpacing(8);
    top->addWidget(clock_);
    top->addWidget(full_);
    root->addLayout(top);

    list_ = new QListWidget;
    list_->setViewMode(QListView::IconMode);
    list_->setFlow(QListView::LeftToRight);
    list_->setWrapping(false);
    list_->setMovement(QListView::Static);
    list_->setSpacing(6);
    list_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setFrameShape(QFrame::NoFrame);
    list_->setFixedHeight(kItem.height() + 22);
    list_->setItemDelegate(new CardDelegate(meta_, list_));
    list_->setStyleSheet("QListWidget { background: transparent; border: none; }");
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->installEventFilter(this);
    list_->viewport()->installEventFilter(this);
    root->addWidget(list_);

    root->addStretch(1);
    title_ = new QLabel;
    title_->setObjectName("title");
    meta1_ = new QLabel;
    meta1_->setObjectName("mono");
    desc_ = new QLabel;
    desc_->setObjectName("muted");
    desc_->setWordWrap(true);
    desc_->setMaximumHeight(72);
    play_ = new QPushButton(T("JOGAR"));
    play_->setObjectName("play");
    play_->setCursor(Qt::PointingHandCursor);
    auto* info = new QVBoxLayout;
    info->setSpacing(4);
    info->addWidget(title_);
    info->addWidget(meta1_);
    info->addWidget(desc_);
    auto* bottom = new QHBoxLayout;
    bottom->addLayout(info, 1);
    bottom->addSpacing(24);
    bottom->addWidget(play_, 0, Qt::AlignBottom);
    root->addLayout(bottom);

    clockTimer_ = new QTimer(this);
    connect(clockTimer_, &QTimer::timeout, this, [this] { clock_->setText(QTime::currentTime().toString("HH:mm")); });
    clockTimer_->start(15000);
    clock_->setText(QTime::currentTime().toString("HH:mm"));

    connect(back_, &QPushButton::clicked, this, &BigPicture::exitRequested);
    connect(full_, &QPushButton::clicked, this, &BigPicture::fullscreenRequested);
    connect(play_, &QPushButton::clicked, this, [this] {
        if (auto* it = list_->currentItem()) emit playRequested(it->data(KeyRole).toString());
    });
    connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) { emit playRequested(it->data(KeyRole).toString()); });
    connect(list_, &QListWidget::currentItemChanged, this, [this] { updateInfo(); update(); });
    chips_.setExclusive(true);
    connect(&chips_, &QButtonGroup::idClicked, this, [this](int id) {
        auto* b = chips_.button(id);
        system_ = b ? b->property("sys").toString() : QString();
        rebuildList();
    });
}

const Game* BigPicture::gameFor(const QString& key) const {
    for (const Game& g : games_) if (g.key() == key) return &g;
    return nullptr;
}

void BigPicture::setGames(const QList<Game>& games, const std::function<bool(const Game&)>& playable) {
    games_ = games;
    playable_.clear();
    for (const Game& g : games_) if (playable(g)) playable_.insert(g.key());
    rebuildChips();
    rebuildList();
}

void BigPicture::rebuildChips() {
    const auto buttons = chips_.buttons();
    for (auto* b : buttons) { chips_.removeButton(b); delete b; }
    auto add = [this](const QString& label, const QString& sys, int id) {
        auto* b = new QPushButton(label);
        b->setObjectName("chip");
        b->setCheckable(true);
        b->setProperty("sys", sys);
        b->setFocusPolicy(Qt::NoFocus);
        chipLayout_->addWidget(b);
        chips_.addButton(b, id);
        b->setChecked(sys == system_);
    };
    while (QLayoutItem* it = chipLayout_->takeAt(0)) delete it;
    bool known = system_.isEmpty();
    add(T("Todos") + QString(" (%1)").arg(games_.size()), QString(), 0);
    int id = 1;
    for (const auto& s : Catalog::systems()) {
        int n = 0;
        for (const Game& g : games_) if (g.systemId == s.id) ++n;
        if (!n) continue;
        add(s.name + QString(" (%1)").arg(n), s.id, id++);
        if (s.id == system_) known = true;
    }
    chipLayout_->addStretch(1);
    if (!known) system_.clear();
    for (auto* b : chips_.buttons()) b->setChecked(b->property("sys").toString() == system_);
}

void BigPicture::rebuildList() {
    const QString keep = list_->currentItem() ? list_->currentItem()->data(KeyRole).toString() : QString();
    list_->clear();
    for (const Game& g : games_) {
        if (!system_.isEmpty() && g.systemId != system_) continue;
        auto* it = new QListWidgetItem(g.title, list_);
        it->setData(KeyRole, g.key());
        it->setData(NoEmuRole, !playable_.contains(g.key()));
        it->setSizeHint(kItem);
        if (g.key() == keep) list_->setCurrentItem(it);
    }
    if (!list_->currentItem() && list_->count()) list_->setCurrentRow(0);
    updateInfo();
    update();
}

void BigPicture::focusGame(const QString& key) {
    system_.clear();
    rebuildChips();
    rebuildList();
    for (int i = 0; i < list_->count(); ++i)
        if (list_->item(i)->data(KeyRole).toString() == key) { list_->setCurrentRow(i); break; }
}

void BigPicture::updateInfo() {
    auto* it = list_->currentItem();
    const Game* g = it ? gameFor(it->data(KeyRole).toString()) : nullptr;
    if (!g) {
        title_->setText(T("Nenhum jogo aqui ainda"));
        meta1_->clear();
        desc_->clear();
        play_->setEnabled(false);
        return;
    }
    const SystemDef* s = Catalog::system(g->systemId);
    title_->setText(g->title);
    meta1_->setText((s ? s->name : QString()) + "  •  " + T("Tempo de jogo: %1").arg(Library::formatDuration(Library::playSeconds(*g))));
    QString d = meta_->cachedText(*g).simplified();
    if (d.size() > 320) d = d.left(317) + "…";
    desc_->setText(d);
    play_->setEnabled(playable_.contains(g->key()));
}

void BigPicture::refreshVisuals() {
    list_->viewport()->update();
    updateInfo();
    update();
}

void BigPicture::stepChip(int delta) {
    const auto buttons = chips_.buttons();
    int cur = 0;
    for (int i = 0; i < buttons.size(); ++i) if (buttons[i]->isChecked()) cur = chips_.id(buttons[i]);
    const int next = qBound(0, cur + delta, int(buttons.size()) - 1);
    if (next == cur) return;
    if (auto* b = chips_.button(next)) {
        b->setChecked(true);
        system_ = b->property("sys").toString();
        rebuildList();
    }
}

bool BigPicture::eventFilter(QObject* o, QEvent* e) {
    if (o == list_ && e->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        switch (k->key()) {
        case Qt::Key_Up: stepChip(-1); return true;
        case Qt::Key_Down: stepChip(1); return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (auto* it = list_->currentItem()) if (play_->isEnabled()) emit playRequested(it->data(KeyRole).toString());
            return true;
        case Qt::Key_Escape: emit exitRequested(); return true;
        case Qt::Key_F11: emit fullscreenRequested(); return true;
        default: break;
        }
    }
    if (o == list_->viewport() && e->type() == QEvent::Wheel) {
        auto* w = static_cast<QWheelEvent*>(e);
        auto* sb = list_->horizontalScrollBar();
        sb->setValue(sb->value() - (w->angleDelta().y() != 0 ? w->angleDelta().y() : w->angleDelta().x()));
        return true;
    }
    return QWidget::eventFilter(o, e);
}

void BigPicture::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    list_->setFocus();
    clock_->setText(QTime::currentTime().toString("HH:mm"));
}

void BigPicture::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const Palette& P = Theme::current();
    p.fillRect(rect(), P.bg);
    auto* it = list_->currentItem();
    if (it) {
        const QPixmap bg = coverPixmap(meta_->bannerPath(it->data(KeyRole).toString()), size());
        if (!bg.isNull()) p.drawPixmap(0, 0, bg);
    }
    QLinearGradient side(0, 0, width(), 0);
    QColor a = P.bg, b = P.bg;
    a.setAlpha(235);
    b.setAlpha(110);
    side.setColorAt(0, a);
    side.setColorAt(1, b);
    p.fillRect(rect(), side);
    QLinearGradient bottom(0, height() * 0.35, 0, height());
    QColor c0 = P.bg, c1 = P.bg;
    c0.setAlpha(0);
    c1.setAlpha(250);
    bottom.setColorAt(0, c0);
    bottom.setColorAt(1, c1);
    p.fillRect(rect(), bottom);
}
