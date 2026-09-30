#pragma once

#include <QObject>
#include <QTimer>

typedef struct _GstElement GstElement;

// The system ringtone, looped, for incoming calls (GStreamer, which the
// app already uses for video).
class Ringtone : public QObject
{
    Q_OBJECT

public:
    explicit Ringtone(QObject* parent = nullptr);
    ~Ringtone() override;

    void play();
    void stop();

private:
    void poll();

    GstElement* m_pipeline = nullptr;
    QTimer m_busTimer;
};
