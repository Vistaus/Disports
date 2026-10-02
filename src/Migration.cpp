#include "Migration.h"

#include <QFile>
#include <QSettings>
#include <QStandardPaths>

#include "Preferences.h"
#include "discord/config/LocalSettings.hpp"

namespace Migration {

bool fromVersion08(Preferences* preferences)
{
    const QString folder = QStringLiteral("/disports.jukfiuu");
    const QString tokenPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                              + folder + QStringLiteral("/token");
    const QString settingsPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                                 + folder + folder + QStringLiteral(".conf");
    const bool hasToken = QFile::exists(tokenPath);
    const bool hasSettings = QFile::exists(settingsPath);
    if (!hasToken && !hasSettings)
        return false;

    QString token;
    QFile file(tokenPath);
    if (file.open(QIODevice::ReadOnly))
        token = QString::fromUtf8(file.readAll()).trimmed();
    if (token.isEmpty() && hasSettings)
        token = QSettings(settingsPath, QSettings::IniFormat).value(QStringLiteral("token")).toString().trimmed();
    // The sign-in, unless this version already has one.
    if (!token.isEmpty() && GetLocalSettings()->GetToken().empty()) {
        GetLocalSettings()->SetToken(token.toStdString());
        GetLocalSettings()->Save();
    }

    // The settings both versions have, unless some were changed here.
    if (hasSettings && preferences->untouched()) {
        const QSettings old(settingsPath, QSettings::IniFormat);
        if (old.contains(QStringLiteral("themeMode")))
            preferences->setThemeMode(qBound(0, old.value(QStringLiteral("themeMode")).toInt(), 2));
        if (old.contains(QStringLiteral("inlineGifPlayback")))
            preferences->setAutoplayGifs(old.value(QStringLiteral("inlineGifPlayback")).toBool());
        if (old.contains(QStringLiteral("blockedMessageVisibility")))
            preferences->setBlockedMessages(old.value(QStringLiteral("blockedMessageVisibility")).toString());
        if (old.contains(QStringLiteral("maxComposerLines")))
            preferences->setComposerMaxLines(qBound(1, old.value(QStringLiteral("maxComposerLines")).toInt(), 6));
    }

    // Nothing of 0.8 is kept: not a second copy of the token either.
    QFile::remove(tokenPath);
    QFile::remove(settingsPath);
    return !token.isEmpty();
}

}
