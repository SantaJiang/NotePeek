#ifndef EMOJIFONTSUPPORT_H
#define EMOJIFONTSUPPORT_H

#include <QByteArray>

namespace EmojiFontSupport {

bool containsEmoji(const QByteArray &utf8Text);
bool hasSystemEmojiFont();
bool shouldRecommendInstallation(const QByteArray &utf8Text, bool emojiFontAvailable);

} // namespace EmojiFontSupport

#endif // EMOJIFONTSUPPORT_H
