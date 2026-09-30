#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>

#include "discord/network/HTTPClient.hpp"

class QNetworkReply;

// HTTPClient for the core, backed by QNetworkAccessManager. Replies arrive
// on the Qt event loop, so the core's request handlers run on the main
// thread.
class QtHttpClient : public QObject, public HTTPClient
{
    Q_OBJECT

public:
    explicit QtHttpClient(QObject* parent = nullptr);
    ~QtHttpClient() override;

    void Init() override {}
    void Kill() override;
    void StopAllRequests() override;
    void PrepareQuit() override;
    std::string ErrorMessage(int code) const override;

    void PerformRequest(
        bool interactive,
        NetRequest::eType type,
        const std::string& url,
        int itype,
        uint64_t requestKey,
        std::string params,
        std::string authorization,
        std::string additional_data,
        NetRequest::NetworkResponseFunc pRespFunc,
        uint8_t* stream_bytes,
        size_t stream_size) override;

    // Adds Discord's client headers (user agent, super properties, ...).
    static void applyClientHeaders(QNetworkRequest& request);

    QNetworkAccessManager* networkAccessManager() { return &m_nam; }

private:
    QNetworkAccessManager m_nam;
    QSet<QNetworkReply*> m_pending;
    bool m_quitting = false;
};
