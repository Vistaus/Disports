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
    // Chats: profile pictures next to messages, or none (more room).
    Q_PROPERTY(bool chatProfilePictures READ chatProfilePictures WRITE setChatProfilePictures NOTIFY chatProfilePicturesChanged)
    // 0 light (Ambiance), 1 dark (SuruDark), 2 follow the system.
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    // Messages from blocked users: "hide", "reveal" (a placeholder to tap)
    // or "show".
    Q_PROPERTY(QString blockedMessages READ blockedMessages WRITE setBlockedMessages NOTIFY blockedMessagesChanged)
    // Lines the message box grows to before it scrolls (1-6).
    Q_PROPERTY(int composerMaxLines READ composerMaxLines WRITE setComposerMaxLines NOTIFY composerMaxLinesChanged)
    Q_PROPERTY(bool autoplayGifs READ autoplayGifs WRITE setAutoplayGifs NOTIFY autoplayGifsChanged)

public:
    explicit Preferences(QObject* parent = nullptr);

    bool dmProfilePictures() const { return m_dmProfilePictures; }
    void setDmProfilePictures(bool enabled);
    int themeMode() const { return m_themeMode; }
    void setThemeMode(int mode);
    QString blockedMessages() const { return m_blockedMessages; }
    void setBlockedMessages(const QString& mode);
    int composerMaxLines() const { return m_composerMaxLines; }
    void setComposerMaxLines(int lines);
    bool chatProfilePictures() const { return m_chatProfilePictures; }
    void setChatProfilePictures(bool enabled);
    bool autoplayGifs() const { return m_autoplayGifs; }
    void setAutoplayGifs(bool enabled);

signals:
    void dmProfilePicturesChanged();
    void autoplayGifsChanged();
    void chatProfilePicturesChanged();
    void themeModeChanged();
    void blockedMessagesChanged();
    void composerMaxLinesChanged();

private:
    QSettings m_settings;
    bool m_dmProfilePictures = true;
    bool m_autoplayGifs = false;
    bool m_chatProfilePictures = true;
    int m_themeMode = 2;
    QString m_blockedMessages = QStringLiteral("reveal");
    int m_composerMaxLines = 3;
};
