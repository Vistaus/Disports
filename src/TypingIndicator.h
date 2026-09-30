#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

#include "discord/models/Snowflake.hpp"

class Session;

// Who else is typing in the open channel ("Alice is typing..."), and
// telling Discord when we are.
class TypingIndicator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)

public:
    explicit TypingIndicator(Session* session);

    QString text() const { return m_text; }

    // We are typing (Discord shows it for about 10 s).
    Q_INVOKABLE void notifyTyping();

    void userTyping(Snowflake user, Snowflake channel);
    // Their message arrived: they are done.
    void userSent(Snowflake user);
    void clear();
    // Names may have changed (server nicknames arrived).
    void refresh();

signals:
    void textChanged();

private:
    Session* m_session;
    QHash<Snowflake, qint64> m_typingUntil; // user -> ms since epoch
    QTimer m_expiry;
    QString m_text;
};
