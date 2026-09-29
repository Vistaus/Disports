#pragma once

#include <QObject>
#include <QSettings>

// App preferences that are not part of the Discord account.
class Preferences : public QObject
{
    Q_OBJECT
    // Direct message list: profile pictures, or icons with a status dot.
    Q_PROPERTY(bool dmProfilePictures READ dmProfilePictures WRITE setDmProfilePictures NOTIFY dmProfilePicturesChanged)
    // Play GIFs in the chat while they are on screen, instead of only in
    // the media viewer. Off by default: it costs data and battery.
    Q_PROPERTY(bool autoplayGifs READ autoplayGifs WRITE setAutoplayGifs NOTIFY autoplayGifsChanged)

public:
    explicit Preferences(QObject* parent = nullptr);

    bool dmProfilePictures() const { return m_dmProfilePictures; }
    void setDmProfilePictures(bool enabled);
    bool autoplayGifs() const { return m_autoplayGifs; }
    void setAutoplayGifs(bool enabled);

signals:
    void dmProfilePicturesChanged();
    void autoplayGifsChanged();

private:
    QSettings m_settings;
    bool m_dmProfilePictures = true;
    bool m_autoplayGifs = false;
};
