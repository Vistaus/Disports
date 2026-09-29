#include "Preferences.h"

#include <QStandardPaths>

namespace {
const QString DmProfilePicturesKey = QStringLiteral("directMessages/profilePictures");
}

Preferences::Preferences(QObject* parent)
    : QObject(parent)
    , m_settings(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                     + QStringLiteral("/preferences.ini"),
                 QSettings::IniFormat)
{
    m_dmProfilePictures = m_settings.value(DmProfilePicturesKey, true).toBool();
}

void Preferences::setDmProfilePictures(bool enabled)
{
    if (m_dmProfilePictures == enabled)
        return;
    m_dmProfilePictures = enabled;
    m_settings.setValue(DmProfilePicturesKey, enabled);
    emit dmProfilePicturesChanged();
}
