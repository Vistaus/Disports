#include "Preferences.h"

#include <QStandardPaths>

namespace {
const QString DmProfilePicturesKey = QStringLiteral("directMessages/profilePictures");
const QString AutoplayGifsKey = QStringLiteral("chat/autoplayGifs");
}

Preferences::Preferences(QObject* parent)
    : QObject(parent)
    , m_settings(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                     + QStringLiteral("/preferences.ini"),
                 QSettings::IniFormat)
{
    m_dmProfilePictures = m_settings.value(DmProfilePicturesKey, true).toBool();
    m_autoplayGifs = m_settings.value(AutoplayGifsKey, false).toBool();
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
