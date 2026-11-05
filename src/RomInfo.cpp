#include "RomInfo.h"
#include <QFile>
#include <QFileInfo>

static QString pretty(const QString& s) {
    bool hasLower = false;
    for (QChar c : s) if (c.isLower()) hasLower = true;
    if (hasLower) return s;                        // already has upper/lowercase
    static const QStringList small = {"of", "the", "in", "to", "a", "an", "and", "vs", "on"};
    QStringList w = s.toLower().split(' ', Qt::SkipEmptyParts);
    for (int i = 0; i < w.size(); ++i) {            // UPPERCASE TITLE -> Title in Uppercase
        bool digit = false;
        for (QChar c : w[i]) if (c.isDigit()) digit = true;
        if (digit) { w[i] = w[i].toUpper(); continue; }
        if (i > 0 && small.contains(w[i])) continue;
        w[i][0] = w[i][0].toUpper();
    }
    return w.join(' ');
}

static QString clean(QByteArray b) {
    const int z = b.indexOf('\0');
    if (z >= 0) b.truncate(z);
    QString s;
    for (uchar c : b) s += (c >= 32 && c < 127) ? QChar(c) : QChar(' ');
    s = s.simplified();
    if (s.size() < 3) return {};
    int alnum = 0;
    for (QChar c : s) if (c.isLetterOrNumber()) ++alnum;
    if (alnum * 10 < s.size() * 6) return {};       // binary garbage
    return pretty(s);
}

static QByteArray readAt(QFile& f, qint64 off, int n) { return f.seek(off) ? f.read(n) : QByteArray(); }

QString RomInfo::internalName(const QString& path, const QString& sys) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QString ext = QFileInfo(path).suffix().toLower();

    if (sys == "n64") {                            // z64 (big endian), v64 (byte-swapped), n64 (little endian)
        QByteArray h = readAt(f, 0, 0x40);
        if (h.size() < 0x40) return {};
        const QByteArray m = h.left(4);
        if (m == QByteArray::fromHex("37804012")) for (int i = 0; i + 1 < h.size(); i += 2) std::swap(h[i], h[i + 1]);
        else if (m == QByteArray::fromHex("40123780")) for (int i = 0; i + 3 < h.size(); i += 4) { std::swap(h[i], h[i + 3]); std::swap(h[i + 1], h[i + 2]); }
        else if (m != QByteArray::fromHex("80371240")) return {};
        return clean(h.mid(0x20, 20));
    }
    if (sys == "gc" || sys == "wii") {
        if (ext != "iso" && ext != "gcm" && ext != "wbfs") return {};   // rvz/ciso/gcz are compressed
        return clean(readAt(f, (ext == "wbfs" ? 0x200 : 0) + 0x20, 64));
    }
    if (sys == "gb" || sys == "gbc") {
        QByteArray t = readAt(f, 0x134, 16);
        if (t.size() == 16 && static_cast<uchar>(t[15]) >= 0x80) t.chop(1);   // GBC flag byte
        return clean(t);
    }
    if (sys == "gba") return clean(readAt(f, 0xA0, 12));
    if (sys == "nds") return clean(readAt(f, 0x00, 12));
    if (sys == "snes") {                           // header at 0x7FC0 (LoROM) or 0xFFC0 (HiROM), with/without 512 extra bytes
        for (qint64 base : {0x7FC0, 0xFFC0, 0x7FC0 + 512, 0xFFC0 + 512}) {
            const QByteArray h = readAt(f, base, 32);
            if (h.size() < 32) continue;
            auto u16 = [&](int i) { return quint16(static_cast<uchar>(h[i]) | (static_cast<uchar>(h[i + 1]) << 8)); };
            if (quint16(u16(0x1C) + u16(0x1E)) == 0xFFFF) return clean(h.left(21));   // checksum complement matches
        }
        return {};
    }
    if (sys == "genesis") {
        if (ext == "smd") return {};
        if (!readAt(f, 0x100, 4).startsWith("SEGA")) return {};
        QString n = clean(readAt(f, 0x150, 48));                             // international name
        return n.isEmpty() ? clean(readAt(f, 0x120, 48)) : n;
    }
    return {};
}