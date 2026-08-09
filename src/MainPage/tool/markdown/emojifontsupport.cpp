#include "tool/markdown/emojifontsupport.h"

#include <QFont>
#include <QFontDatabase>
#include <QRawFont>
#include <QString>
#include <QStringList>

namespace {

bool isEmojiCodePoint(char32_t codePoint)
{
    return (codePoint >= 0x1f000 && codePoint <= 0x1faff)
           || (codePoint >= 0x231a && codePoint <= 0x231b)
           || (codePoint >= 0x23e9 && codePoint <= 0x23ec)
           || codePoint == 0x23f0
           || codePoint == 0x23f3
           || (codePoint >= 0x25fd && codePoint <= 0x25fe)
           || (codePoint >= 0x2614 && codePoint <= 0x2615)
           || (codePoint >= 0x2648 && codePoint <= 0x2653)
           || codePoint == 0x267f
           || codePoint == 0x2693
           || codePoint == 0x26a1
           || (codePoint >= 0x26aa && codePoint <= 0x26ab)
           || (codePoint >= 0x26bd && codePoint <= 0x26be)
           || (codePoint >= 0x26c4 && codePoint <= 0x26c5)
           || codePoint == 0x26ce
           || codePoint == 0x26d4
           || codePoint == 0x26ea
           || (codePoint >= 0x26f2 && codePoint <= 0x26f3)
           || codePoint == 0x26f5
           || codePoint == 0x26fa
           || codePoint == 0x26fd
           || codePoint == 0x2705
           || (codePoint >= 0x270a && codePoint <= 0x270b)
           || codePoint == 0x2728
           || codePoint == 0x274c
           || codePoint == 0x274e
           || (codePoint >= 0x2753 && codePoint <= 0x2755)
           || codePoint == 0x2757
           || (codePoint >= 0x2795 && codePoint <= 0x2797)
           || codePoint == 0x27b0
           || codePoint == 0x27bf
           || (codePoint >= 0x2b1b && codePoint <= 0x2b1c)
           || codePoint == 0x2b50
           || codePoint == 0x2b55
           || codePoint == 0xfe0f
           || codePoint == 0x20e3;
}

char32_t nextUtf8CodePoint(const QByteArray &text, qsizetype *offset)
{
    const auto *data = reinterpret_cast<const unsigned char *>(text.constData());
    const qsizetype size = text.size();
    const unsigned char first = data[(*offset)++];
    if (first < 0x80)
        return first;

    int continuationCount = 0;
    char32_t codePoint = 0;
    if ((first & 0xe0) == 0xc0)
    {
        continuationCount = 1;
        codePoint = first & 0x1f;
    }
    else if ((first & 0xf0) == 0xe0)
    {
        continuationCount = 2;
        codePoint = first & 0x0f;
    }
    else if ((first & 0xf8) == 0xf0)
    {
        continuationCount = 3;
        codePoint = first & 0x07;
    }
    else
    {
        return 0xfffd;
    }

    if (*offset + continuationCount > size)
    {
        *offset = size;
        return 0xfffd;
    }
    for (int index = 0; index < continuationCount; ++index)
    {
        const unsigned char next = data[(*offset)++];
        if ((next & 0xc0) != 0x80)
            return 0xfffd;
        codePoint = (codePoint << 6) | (next & 0x3f);
    }
    return codePoint;
}

bool familyNameLooksLikeEmojiFont(const QString &family)
{
    const QString normalized = family.toCaseFolded();
    return normalized.contains(QStringLiteral("emoji"))
           || normalized.contains(QStringLiteral("openmoji"))
           || normalized.contains(QStringLiteral("joypixels"))
           || normalized == QStringLiteral("symbola");
}

} // namespace

bool EmojiFontSupport::containsEmoji(const QByteArray &utf8Text)
{
    qsizetype offset = 0;
    while (offset < utf8Text.size())
    {
        if (isEmojiCodePoint(nextUtf8CodePoint(utf8Text, &offset)))
            return true;
    }
    return false;
}

bool EmojiFontSupport::hasSystemEmojiFont()
{
    static const bool available = []() {
        const QStringList families = QFontDatabase::families();
        for (const QString &family : families)
        {
            if (familyNameLooksLikeEmojiFont(family))
                return true;

            const QRawFont rawFont = QRawFont::fromFont(QFont(family));
            if (rawFont.isValid()
                && rawFont.supportsCharacter(char32_t(0x1f600))
                && rawFont.supportsCharacter(char32_t(0x1f44d)))
            {
                return true;
            }
        }
        return false;
    }();
    return available;
}

bool EmojiFontSupport::shouldRecommendInstallation(const QByteArray &utf8Text,
                                                   bool emojiFontAvailable)
{
    return !emojiFontAvailable && containsEmoji(utf8Text);
}
