#include "QtHttpClient.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "discord/config/DiscordClientConfig.hpp"
#include "discord/config/LocalSettings.hpp"

QtHttpClient::QtHttpClient(QObject* parent)
    : QObject(parent)
{
    m_nam.setTransferTimeout(30000);
}

QtHttpClient::~QtHttpClient()
{
    PrepareQuit();
}

void QtHttpClient::Kill()
{
    PrepareQuit();
}

void QtHttpClient::StopAllRequests()
{
    // abort() emits finished() synchronously, which removes the reply from
    // m_pending, so iterate over a copy.
    const auto pending = m_pending;
    for (QNetworkReply* reply : pending)
        reply->abort();
}

void QtHttpClient::PrepareQuit()
{
    m_quitting = true;
    StopAllRequests();
    m_quitting = false;
}

std::string QtHttpClient::ErrorMessage(int code) const
{
    switch (code) {
    case HTTP_OOPS:         return "Network error";
    case HTTP_OK:           return "OK";
    case HTTP_CREATED:      return "Created";
    case HTTP_ACCEPTED:     return "Accepted";
    case HTTP_NOCONTENT:    return "No content";
    case HTTP_BADREQUEST:   return "Bad request";
    case HTTP_UNAUTHORIZED: return "Unauthorized";
    case HTTP_FORBIDDEN:    return "Forbidden";
    case HTTP_NOTFOUND:     return "Not found";
    case HTTP_UNSUPPMEDIA:  return "Unsupported media type";
    case HTTP_TOOMANYREQS:  return "Too many requests";
    case HTTP_BADGATEWAY:   return "Bad gateway";
    case HTTP_CANCELED:     return "Canceled";
    default:                return "HTTP " + std::to_string(code);
    }
}

void QtHttpClient::applyClientHeaders(QNetworkRequest& request)
{
    DiscordClientConfig* config = GetClientConfig();
    request.setRawHeader("User-Agent", QByteArray::fromStdString(config->GetUserAgent()));

    if (GetLocalSettings()->AddExtraHeaders()) {
        request.setRawHeader("X-Super-Properties", QByteArray::fromStdString(config->GetSerializedBase64Blob()));
        request.setRawHeader("X-Discord-Timezone", QByteArray::fromStdString(config->GetTimezone()));
        request.setRawHeader("X-Discord-Locale", QByteArray::fromStdString(config->GetLocale()));
        request.setRawHeader("Sec-Ch-Ua", QByteArray::fromStdString(config->GetSecChUa()));
        request.setRawHeader("Sec-Ch-Ua-Mobile", "?0");
        request.setRawHeader("Sec-Ch-Ua-Platform", QByteArray::fromStdString(config->GetOS()));
    }
}

void QtHttpClient::PerformRequest(
    bool /*interactive*/,
    NetRequest::eType type,
    const std::string& url,
    int itype,
    uint64_t requestKey,
    std::string params,
    std::string authorization,
    std::string additional_data,
    NetRequest::NetworkResponseFunc pRespFunc,
    uint8_t* stream_bytes,
    size_t stream_size)
{
    // NetRequest copies the stream bytes, so the caller keeps ownership.
    auto* req = new NetRequest(0, itype, requestKey, type, url, "", params, authorization,
                               additional_data, pRespFunc, stream_bytes, stream_size);

    QNetworkRequest request(QUrl(QString::fromStdString(url)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    applyClientHeaders(request);
    if (!authorization.empty())
        request.setRawHeader("Authorization", QByteArray::fromStdString(authorization));

    const QByteArray body = QByteArray::fromStdString(params);
    const QByteArray octets(reinterpret_cast<const char*>(req->params_bytes.data()),
                            qsizetype(req->params_bytes.size()));
    const QByteArray formType = "application/x-www-form-urlencoded";
    const QByteArray jsonType = "application/json";

    QNetworkReply* reply = nullptr;
    switch (type) {
    case NetRequest::GET:
    case NetRequest::GET_PROGRESS:
        reply = m_nam.get(request);
        break;
    case NetRequest::POST:
        request.setHeader(QNetworkRequest::ContentTypeHeader, formType);
        reply = m_nam.post(request, body);
        break;
    case NetRequest::POST_JSON:
        request.setHeader(QNetworkRequest::ContentTypeHeader, jsonType);
        reply = m_nam.post(request, body);
        break;
    case NetRequest::PUT:
        request.setHeader(QNetworkRequest::ContentTypeHeader, formType);
        reply = m_nam.put(request, body);
        break;
    case NetRequest::PUT_JSON:
        request.setHeader(QNetworkRequest::ContentTypeHeader, jsonType);
        reply = m_nam.put(request, body);
        break;
    case NetRequest::PUT_OCTETS:
    case NetRequest::PUT_OCTETS_PROGRESS:
        request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/octet-stream"));
        reply = m_nam.put(request, octets);
        break;
    case NetRequest::PATCH:
        request.setHeader(QNetworkRequest::ContentTypeHeader, jsonType);
        reply = m_nam.sendCustomRequest(request, "PATCH", body);
        break;
    case NetRequest::DELETE_:
        request.setHeader(QNetworkRequest::ContentTypeHeader, jsonType);
        reply = m_nam.sendCustomRequest(request, "DELETE", body);
        break;
    default:
        qWarning("QtHttpClient: unsupported request type %d for %s", int(type), url.c_str());
        delete req;
        return;
    }

    m_pending.insert(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, req]() {
        m_pending.remove(reply);
        reply->deleteLater();

        const QVariant status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (status.isValid()) {
            req->result = status.toInt();
            req->response = reply->readAll().toStdString();
        } else if (reply->error() == QNetworkReply::OperationCanceledError) {
            req->result = HTTP_CANCELED;
            req->response = "Operation canceled";
        } else {
            req->result = HTTP_OOPS;
            req->response = reply->errorString().toStdString();
        }

        if (!m_quitting)
            req->pFunc(req);
        delete req;
    });
}
