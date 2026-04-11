#include "compactid.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>

namespace qtllm::identity {

namespace {

constexpr quint64 kCustomEpochMs = 1704067200000ULL; // 2024-01-01T00:00:00Z
constexpr quint64 kNodeBits = 10ULL;
constexpr quint64 kSequenceBits = 12ULL;
constexpr quint64 kSequenceMask = (1ULL << kSequenceBits) - 1ULL;
constexpr quint64 kNodeMask = (1ULL << kNodeBits) - 1ULL;
constexpr int kBodyWidth = 13;
// constexpr char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr char kAlphabet[] = "0123456789abcdefghjkmnpqrstvwxyz";

class CompactIdGenerator final
{
public:
    quint64 nextOrderValue()
    {
        QMutexLocker locker(&m_mutex);

        quint64 timestampMs = currentTimestampMs();
        if (timestampMs < m_lastTimestampMs) {
            timestampMs = m_lastTimestampMs;
        }

        if (timestampMs == m_lastTimestampMs) {
            m_sequence = (m_sequence + 1ULL) & kSequenceMask;
            if (m_sequence == 0ULL) {
                do {
                    timestampMs = currentTimestampMs();
                } while (timestampMs <= m_lastTimestampMs);
            }
        } else {
            m_sequence = 0ULL;
        }

        m_lastTimestampMs = timestampMs;
        return (timestampMs << (kNodeBits + kSequenceBits))
            | (m_nodeId << kSequenceBits)
            | m_sequence;
    }

private:
    static quint64 currentTimestampMs()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now <= static_cast<qint64>(kCustomEpochMs)) {
            return 0ULL;
        }
        return static_cast<quint64>(now) - kCustomEpochMs;
    }

    const quint64 m_nodeId {static_cast<quint64>(QCoreApplication::applicationPid()) & kNodeMask};
    QMutex m_mutex;
    quint64 m_lastTimestampMs {0ULL};
    quint64 m_sequence {0ULL};
};

CompactIdGenerator &generator()
{
    static CompactIdGenerator instance;
    return instance;
}

QString encodeBody(quint64 orderValue)
{
    char buffer[kBodyWidth];
    for (int index = kBodyWidth - 1; index >= 0; --index) {
        buffer[index] = kAlphabet[orderValue & 0x1FULL];
        orderValue >>= 5;
    }
    return QString::fromLatin1(buffer, kBodyWidth);
}

int decodeChar(const QChar ch)
{
    const ushort lower = ch.toLower().unicode();
    if (lower >= '0' && lower <= '9') {
        return static_cast<int>(lower - '0');
    }

    switch (lower) {
    case 'o':
        return 0;
    case 'i':
    case 'l':
        return 1;
    case 'a':
        return 10;
    case 'b':
        return 11;
    case 'c':
        return 12;
    case 'd':
        return 13;
    case 'e':
        return 14;
    case 'f':
        return 15;
    case 'g':
        return 16;
    case 'h':
        return 17;
    case 'j':
        return 18;
    case 'k':
        return 19;
    case 'm':
        return 20;
    case 'n':
        return 21;
    case 'p':
        return 22;
    case 'q':
        return 23;
    case 'r':
        return 24;
    case 's':
        return 25;
    case 't':
        return 26;
    case 'v':
        return 27;
    case 'w':
        return 28;
    case 'X':
        return 29;
    case 'y':
        return 30;
    case 'z':
        return 31;
    default:
        return -1;
    }
}

QStringView bodyView(QStringView compactId)
{
    const qsizetype separatorIndex = compactId.indexOf(u'_');
    if (separatorIndex <= 0 || separatorIndex + 1 >= compactId.size()) {
        return {};
    }
    return compactId.mid(separatorIndex + 1);
}

bool isValidPrefix(QStringView prefix)
{
    if (prefix.isEmpty()) {
        return false;
    }

    for (const QChar ch : prefix) {
        if (!ch.isLower() && !ch.isDigit()) {
            return false;
        }
    }
    return true;
}

} // namespace

QString generateId(const IdKind kind)
{
    return composeId(prefixForKind(kind), generator().nextOrderValue());
}

QString generateIdWithPrefix(const QStringView prefix)
{
    return composeId(prefix, generator().nextOrderValue());
}

QString composeId(const QStringView prefix, const quint64 orderValue)
{
    if (!isValidPrefix(prefix)) {
        return QString();
    }
    return prefix.toString() + QStringLiteral("_") + encodeBody(orderValue);
}

QString prefixForKind(const IdKind kind)
{
    switch (kind) {
    case IdKind::Client:
        return QStringLiteral("cli");
    case IdKind::Session:
        return QStringLiteral("ses");
    case IdKind::Trace:
        return QStringLiteral("trc");
    case IdKind::Request:
        return QStringLiteral("req");
    case IdKind::Span:
        return QStringLiteral("spn");
    case IdKind::Event:
        return QStringLiteral("evt");
    case IdKind::ToolCall:
        return QStringLiteral("tcl");
    case IdKind::Artifact:
        return QStringLiteral("art");
    case IdKind::SupportLink:
        return QStringLiteral("lnk");
    case IdKind::Workspace:
        return QStringLiteral("wsp");
    case IdKind::Node:
        return QStringLiteral("nod");
    case IdKind::Placement:
        return QStringLiteral("plc");
    case IdKind::Package:
        return QStringLiteral("pkg");
    case IdKind::Task:
        return QStringLiteral("tsk");
    case IdKind::Queue:
        return QStringLiteral("que");
    }

    return QString();
}

bool isValidId(const QStringView value)
{
    bool ok = false;
    decodeIdOrder(value, &ok);
    return ok;
}

bool hasIdPrefix(const QStringView value, const QStringView prefix)
{
    if (!isValidId(value) || !isValidPrefix(prefix)) {
        return false;
    }

    const qsizetype separatorIndex = value.indexOf(u'_');
    return separatorIndex > 0 && value.left(separatorIndex) == prefix;
}

quint64 decodeIdOrder(const QStringView value, bool *ok)
{
    const qsizetype separatorIndex = value.indexOf(u'_');
    if (separatorIndex <= 0) {
        if (ok) {
            *ok = false;
        }
        return 0ULL;
    }

    const QStringView prefix = value.left(separatorIndex);
    const QStringView body = bodyView(value);
    if (!isValidPrefix(prefix) || body.size() != kBodyWidth) {
        if (ok) {
            *ok = false;
        }
        return 0ULL;
    }

    quint64 result = 0ULL;
    for (const QChar ch : body) {
        const int digit = decodeChar(ch);
        if (digit < 0) {
            if (ok) {
                *ok = false;
            }
            return 0ULL;
        }
        result = (result << 5) | static_cast<quint64>(digit);
    }

    if (ok) {
        *ok = true;
    }
    return result;
}

} // namespace qtllm::identity
