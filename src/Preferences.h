#pragma once

#include <QObject>
#include <QSettings>

// App preferences that are not part of the Discord account.
class Preferences : public QObject
{
    Q_OBJECT
    // Direct message list: profile pictures, or icons with a status dot.
    Q_PROPERTY(bool dmProfilePictures READ dmProfilePictures WRITE setDmProfilePictures NOTIFY dmProfilePicturesChanged)
    // Profile pictures next to messages, or none (more room).
    Q_PROPERTY(bool chatProfilePictures READ chatProfilePictures WRITE setChatProfilePictures NOTIFY chatProfilePicturesChanged)
    // 0 light (Ambiance), 1 dark (SuruDark), 2 follow the system.
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    // Messages from blocked users: "hide", "reveal" (a placeholder to tap)
    // or "show".
    Q_PROPERTY(QString blockedMessages READ blockedMessages WRITE setBlockedMessages NOTIFY blockedMessagesChanged)
    // Lines the message box grows to before it scrolls (1-6).
    Q_PROPERTY(int composerMaxLines READ composerMaxLines WRITE setComposerMaxLines NOTIFY composerMaxLinesChanged)
    // Play GIFs in the chat while on screen, not only in the media viewer.
    Q_PROPERTY(bool autoplayGifs READ autoplayGifs WRITE setAutoplayGifs NOTIFY autoplayGifsChanged)
    // Calls: echo cancellation and gain control, and RNNoise.
    Q_PROPERTY(bool voiceProcessing READ voiceProcessing WRITE setVoiceProcessing NOTIFY voiceProcessingChanged)
    Q_PROPERTY(bool noiseSuppression READ noiseSuppression WRITE setNoiseSuppression NOTIFY noiseSuppressionChanged)

public:
    explicit Preferences(QObject* parent = nullptr);

    // Nothing changed from the defaults yet (a first start).
    bool untouched() const { return m_settings.allKeys().isEmpty(); }

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
    bool voiceProcessing() const { return m_voiceProcessing; }
    void setVoiceProcessing(bool enabled);
    bool noiseSuppression() const { return m_noiseSuppression; }
    void setNoiseSuppression(bool enabled);

signals:
    void dmProfilePicturesChanged();
    void autoplayGifsChanged();
    void voiceProcessingChanged();
    void noiseSuppressionChanged();
    void chatProfilePicturesChanged();
    void themeModeChanged();
    void blockedMessagesChanged();
    void composerMaxLinesChanged();

private:
    QSettings m_settings;
    bool m_dmProfilePictures = true;
    bool m_autoplayGifs = false;
    bool m_voiceProcessing = true;
    bool m_noiseSuppression = true;
    bool m_chatProfilePictures = true;
    int m_themeMode = 2;
    QString m_blockedMessages = QStringLiteral("reveal");
    int m_composerMaxLines = 3;
};
