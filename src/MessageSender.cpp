#include "MessageSender.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

#include <ctime>

#include "discord/DiscordInstance.hpp"
#include "discord/state/MessageCache.hpp"

#include "ChannelPermissions.h"
#include "DiscordUrls.h"
#include "Session.h"
#include "models/MessageListModel.h"

namespace {

// Discord's largest upload (with Nitro).
constexpr qint64 MaxUploadBytes = 500ll * 1024 * 1024;

}

MessageSender::MessageSender(Session* session)
    : QObject(session)
    , m_session(session)
{
    m_slowmodeTick.setInterval(1000);
    connect(&m_slowmodeTick, &QTimer::timeout, this, [this]() {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (auto it = m_slowmodeEnd.begin(); it != m_slowmodeEnd.end();)
            it = it.value() <= now ? m_slowmodeEnd.erase(it) : std::next(it);
        if (m_slowmodeEnd.isEmpty())
            m_slowmodeTick.stop();
        emit slowmodeChanged();
    });
}

int MessageSender::slowmodeRemaining() const
{
    const qint64 left = m_slowmodeEnd.value(m_session->messages()->channel()) - QDateTime::currentMSecsSinceEpoch();
    return left > 0 ? int((left + 999) / 1000) : 0;
}

bool MessageSender::slowedDown()
{
    const int wait = slowmodeRemaining();
    if (wait)
        m_session->showNotice(tr("Slowmode is on: you can send another message in %n second(s).", nullptr, wait));
    return wait > 0;
}

void MessageSender::startSlowmode()
{
    const int seconds = m_session->permissions()->slowmodeSeconds();
    if (!seconds)
        return;
    m_slowmodeEnd.insert(m_session->instance()->GetCurrentChannelID(),
                         QDateTime::currentMSecsSinceEpoch() + seconds * 1000);
    m_slowmodeTick.start();
    emit slowmodeChanged();
}

void MessageSender::showPending(Snowflake nonce, const QString& text)
{
    DiscordInstance* instance = m_session->instance();
    Message pending;
    pending.m_snowflake = nonce;
    pending.m_type = MessageType::SENDING_MESSAGE;
    pending.m_message = text.toStdString();
    if (Profile* me = instance->GetProfile()) {
        pending.m_author_snowflake = me->m_snowflake;
        pending.m_author = !me->m_globalName.empty() ? me->m_globalName : me->m_name;
        pending.m_avatar = me->m_avatarlnk;
    }
    pending.SetTime(time(nullptr));
    GetMessageCache()->AddMessage(instance->GetCurrentChannelID(), pending);
    m_session->messages()->sync();
}

void MessageSender::send(const QString& text, const QString& replyToId)
{
    DiscordInstance* instance = m_session->instance();
    const QString content = text.trimmed();
    if (!instance || content.isEmpty() || slowedDown())
        return;

    Snowflake nonce = 0;
    const Snowflake replyTo = replyToId.isEmpty() ? 0 : DiscordUrls::fromId(replyToId);
    if (!instance->SendMessageToCurrentChannel(content.toStdString(), nonce, replyTo, true)) {
        m_session->showNotice(tr("You can't send messages in this channel."));
        return;
    }
    startSlowmode();
    showPending(nonce, content);
}

bool MessageSender::sendFile(const QString& fileUrl, const QString& text)
{
    DiscordInstance* instance = m_session->instance();
    if (!instance || !m_session->connected())
        return false;
    if (m_uploading) {
        m_session->showNotice(tr("Wait for the file being sent to finish."));
        return false;
    }
    if (!m_session->permissions()->canAttachFiles()) {
        m_session->showNotice(tr("You can't attach files in this channel."));
        return false;
    }
    if (slowedDown())
        return false;

    const QUrl url(fileUrl);
    QFile file(url.isLocalFile() ? url.toLocalFile() : fileUrl);
    const QString name = QFileInfo(file).fileName();
    if (file.size() > MaxUploadBytes) {
        m_session->showNotice(tr("%1 is too large to send.").arg(name));
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_session->showNotice(tr("%1 could not be read.").arg(name));
        return false;
    }
    QByteArray data = file.readAll();
    const QString content = text.trimmed();

    Snowflake nonce = 0;
    if (!instance->SendMessageAndAttachmentToCurrentChannel(content.toStdString(), nonce,
            reinterpret_cast<uint8_t*>(data.data()), size_t(data.size()), name.toStdString())) {
        m_session->showNotice(tr("You can't attach files in this channel."));
        return false;
    }
    m_uploading = true;
    m_uploadCancelled = false;
    m_uploadName = name;
    m_uploadProgress = 0;
    emit uploadChanged();

    startSlowmode();
    showPending(nonce, content.isEmpty() ? name : content + QLatin1Char('\n') + name);
    return true;
}

void MessageSender::cancelUpload()
{
    if (m_uploading)
        m_uploadCancelled = true;
}

void MessageSender::uploadStarted(const QString& name)
{
    m_uploadName = name;
    emit uploadChanged();
}

bool MessageSender::uploadProgressed(size_t offset, size_t length)
{
    if (length)
        m_uploadProgress = qreal(offset) / qreal(length);
    emit uploadChanged();
    return m_uploadCancelled;
}

void MessageSender::uploadFinished()
{
    m_uploading = false;
    m_uploadProgress = 0;
    emit uploadChanged();
}

void MessageSender::uploadFailed(const QString& name, int error)
{
    const bool cancelled = m_uploadCancelled;
    m_uploadCancelled = false;
    uploadFinished();
    if (cancelled)
        return;
    // Discord answers 400 or 413 to files over the server's limit.
    if (error == 400 || error == 413)
        m_session->showNotice(tr("%1 could not be sent: it may be larger than this server allows.").arg(name));
    else
        m_session->showNotice(tr("%1 could not be sent (error %2).").arg(name).arg(error));
}

void MessageSender::clear()
{
    m_slowmodeEnd.clear();
    m_slowmodeTick.stop();
    m_uploadCancelled = false;
    if (m_uploading)
        uploadFinished();
    emit slowmodeChanged();
}
