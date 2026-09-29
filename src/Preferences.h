#pragma once

#include <QObject>
#include <QSettings>

// App preferences that are not part of the Discord account.
class Preferences : public QObject
{
    Q_OBJECT
    // Direct message list: profile pictures, or icons with a status dot.
    Q_PROPERTY(bool dmProfilePictures READ dmProfilePictures WRITE setDmProfilePictures NOTIFY dmProfilePicturesChanged)

public:
    explicit Preferences(QObject* parent = nullptr);

    bool dmProfilePictures() const { return m_dmProfilePictures; }
    void setDmProfilePictures(bool enabled);

signals:
    void dmProfilePicturesChanged();

private:
    QSettings m_settings;
    bool m_dmProfilePictures = true;
};
