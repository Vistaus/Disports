#include "Preferences.h"

#include <QStandardPaths>

namespace {
const QString DmProfilePicturesKey = QStringLiteral("directMessages/profilePictures");
const QString AutoplayGifsKey = QStringLiteral("chat/autoplayGifs");
const QString ChatProfilePicturesKey = QStringLiteral("chat/profilePictures");
const QString ThemeModeKey = QStringLiteral("appearance/theme");
const QString BlockedMessagesKey = QStringLiteral("chat/blockedMessages");
const QString ComposerMaxLinesKey = QStringLiteral("chat/composerMaxLines");
}

Preferences::Preferences(QObject* parent)
    : QObject(parent)
    , m_settings(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                     + QStringLiteral("/preferences.ini"),
                 QSettings::IniFormat)
{
    m_dmProfilePictures = m_settings.value(DmProfilePicturesKey, true).toBool();
    m_autoplayGifs = m_settings.value(AutoplayGifsKey, false).toBool();
    m_chatProfilePictures = m_settings.value(ChatProfilePicturesKey, true).toBool();
    m_themeMode = qBound(0, m_settings.value(ThemeModeKey, 2).toInt(), 2);
    m_blockedMessages = m_settings.value(BlockedMessagesKey, QStringLiteral("reveal")).toString();
    if (m_blockedMessages != QLatin1String("hide") && m_blockedMessages != QLatin1String("show"))
        m_blockedMessages = QStringLiteral("reveal");
    m_composerMaxLines = qBound(1, m_settings.value(ComposerMaxLinesKey, 3).toInt(), 6);
}

void Preferences::setDmProfilePictures(bool enabled)
{
    if (m_dmProfilePictures == enabled)
        return;
    m_dmProfilePictures = enabled;
    m_settings.setValue(DmProfilePicturesKey, enabled);
    emit dmProfilePicturesChanged();
}

void Preferences::setAutoplayGifs(bool enabled)
{
    if (m_autoplayGifs == enabled)
        return;
    m_autoplayGifs = enabled;
    m_settings.setValue(AutoplayGifsKey, enabled);
    emit autoplayGifsChanged();
}

void Preferences::setChatProfilePictures(bool enabled)
{
    if (m_chatProfilePictures == enabled)
        return;
    m_chatProfilePictures = enabled;
    m_settings.setValue(ChatProfilePicturesKey, enabled);
    emit chatProfilePicturesChanged();
}

void Preferences::setThemeMode(int mode)
{
    mode = qBound(0, mode, 2);
    if (m_themeMode == mode)
        return;
    m_themeMode = mode;
    m_settings.setValue(ThemeModeKey, mode);
    emit themeModeChanged();
}

void Preferences::setBlockedMessages(const QString& mode)
{
    if (m_blockedMessages == mode
            || (mode != QLatin1String("hide") && mode != QLatin1String("reveal") && mode != QLatin1String("show")))
        return;
    m_blockedMessages = mode;
    m_settings.setValue(BlockedMessagesKey, mode);
    emit blockedMessagesChanged();
}

void Preferences::setComposerMaxLines(int lines)
{
    lines = qBound(1, lines, 6);
    if (m_composerMaxLines == lines)
        return;
    m_composerMaxLines = lines;
    m_settings.setValue(ComposerMaxLinesKey, lines);
    emit composerMaxLinesChanged();
}
