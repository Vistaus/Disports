#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

class QNetworkAccessManager;
class QNetworkReply;
typedef struct _GstElement GstElement;

// Discord's ringtone, looped, for incoming calls (GStreamer, which the app
// already uses for video). It is downloaded from Discord once, ahead of
// time, and kept in the cache; until then, or when that fails, the
// system's default ringtone plays.
class Ringtone : public QObject
{
    Q_OBJECT

public:
    explicit Ringtone(QNetworkAccessManager* nam, QObject* parent = nullptr);
    ~Ringtone() override;

    // Downloads Discord's ringtone if it isn't cached yet. At most one try
    // a day.
    void fetch();
    void play();
    void stop();

private:
    void poll();
    void downloaded();

    QNetworkAccessManager* m_nam;
    QPointer<QNetworkReply> m_reply;
    GstElement* m_pipeline = nullptr;
    QTimer m_busTimer;
};
