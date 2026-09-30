#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include "discord/models/Snowflake.hpp"

class Session;

// Sends text and files to the open channel. A sent message shows up as
// pending until Discord's copy of it arrives. Also keeps the slowmode
// cooldown after sending, per channel. One upload at a time.
class MessageSender : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int slowmodeRemaining READ slowmodeRemaining NOTIFY slowmodeChanged)
    Q_PROPERTY(bool uploading READ uploading NOTIFY uploadChanged)
    Q_PROPERTY(QString uploadName READ uploadName NOTIFY uploadChanged)
    Q_PROPERTY(qreal uploadProgress READ uploadProgress NOTIFY uploadChanged)

public:
    explicit MessageSender(Session* session);

    // Seconds until the open channel takes another message.
    int slowmodeRemaining() const;
    bool uploading() const { return m_uploading; }
    QString uploadName() const { return m_uploadName; }
    qreal uploadProgress() const { return m_uploadProgress; }

    Q_INVOKABLE void send(const QString& text, const QString& replyToId = QString());
    // A local file (a file:// URL, from Content Hub) with an optional text.
    Q_INVOKABLE bool sendFile(const QString& fileUrl, const QString& text);
    Q_INVOKABLE void cancelUpload();

    // From the core.
    void uploadStarted(const QString& name);
    bool uploadProgressed(size_t offset, size_t length); // true: cancel it
    void uploadFinished();
    void uploadFailed(const QString& name, int error);

    void clear();

signals:
    void slowmodeChanged();
    void uploadChanged();

private:
    bool slowedDown();
    void startSlowmode();
    void showPending(Snowflake nonce, const QString& text);

    Session* m_session;
    QHash<Snowflake, qint64> m_slowmodeEnd; // channel -> ms since epoch
    QTimer m_slowmodeTick;
    bool m_uploading = false;
    bool m_uploadCancelled = false;
    QString m_uploadName;
    qreal m_uploadProgress = 0;
};
