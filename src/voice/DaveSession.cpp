#include "DaveSession.h"

#include <QDebug>

#include <dave/logger.h>

using namespace discord::dave;

namespace {

constexpr uint64_t NewGroupEpoch = 1;
constexpr uint16_t InitTransitionId = 0;

void logSink(LoggingSeverity severity, const char*, int, const std::string& message)
{
    if (severity >= LS_WARNING)
        qWarning("DAVE: %s", message.c_str());
}

}

DaveSession::DaveSession(std::string selfUserId, uint64_t groupId, Callbacks callbacks)
    : m_selfUserId(std::move(selfUserId))
    , m_groupId(groupId)
    , m_callbacks(std::move(callbacks))
{
    SetLogSink(&logSink);
    // Persistent keys are not used: a new identity key per call.
    m_session = mls::CreateSession(nullptr, std::string(), [](const std::string& source, const std::string& reason) {
        qWarning("DAVE MLS failure: %s: %s", source.c_str(), reason.c_str());
    });
    m_encryptor = CreateEncryptor();
    // Until DAVE is set up, media is not end-to-end encrypted.
    m_encryptor->SetPassthroughMode(true);
}

DaveSession::~DaveSession() = default;

int DaveSession::maxProtocolVersion()
{
    return MaxSupportedProtocolVersion();
}

std::set<std::string> DaveSession::recognizedUserIds() const
{
    std::set<std::string> ids = m_users;
    ids.insert(m_selfUserId);
    return ids;
}

IDecryptor* DaveSession::decryptorFor(const std::string& userId)
{
    auto it = m_decryptors.find(userId);
    if (it == m_decryptors.end()) {
        auto decryptor = CreateDecryptor();
        decryptor->TransitionToPassthroughMode(m_latestPreparedVersion == kDisabledVersion);
        it = m_decryptors.emplace(userId, std::move(decryptor)).first;
    }
    return it->second.get();
}

void DaveSession::addUser(const std::string& userId)
{
    m_users.insert(userId);
    decryptorFor(userId);
    setupKeyRatchetForUser(userId, m_latestPreparedVersion);
}

void DaveSession::removeUser(const std::string& userId)
{
    m_users.erase(userId);
    m_decryptors.erase(userId);
}

void DaveSession::onSelectProtocolAck(int protocolVersion)
{
    handleProtocolInit(protocolVersion);
}

void DaveSession::onPrepareTransition(uint16_t transitionId, int protocolVersion)
{
    prepareRatchets(transitionId, protocolVersion);
    maybeSendReadyForTransition(transitionId);
}

void DaveSession::onExecuteTransition(uint16_t transitionId)
{
    handleExecuteTransition(transitionId);
}

void DaveSession::onPrepareEpoch(uint64_t epoch, int protocolVersion)
{
    handlePrepareEpoch(epoch, protocolVersion);
    if (epoch == NewGroupEpoch)
        sendKeyPackage();
}

void DaveSession::onExternalSenderPackage(const std::vector<uint8_t>& package)
{
    m_session->SetExternalSender(package);
}

void DaveSession::onProposals(const std::vector<uint8_t>& proposals)
{
    auto commitWelcome = m_session->ProcessProposals(proposals, recognizedUserIds());
    if (commitWelcome && m_callbacks.sendCommitWelcome)
        m_callbacks.sendCommitWelcome(*commitWelcome);
}

void DaveSession::onAnnounceCommitTransition(uint16_t transitionId, const std::vector<uint8_t>& commit)
{
    RosterVariant result = m_session->ProcessCommit(commit);
    if (std::holds_alternative<ignored_t>(result))
        return;
    if (std::holds_alternative<RosterMap>(result)) {
        prepareRatchets(transitionId, m_session->GetProtocolVersion());
        maybeSendReadyForTransition(transitionId);
    } else {
        if (m_callbacks.sendInvalidCommitWelcome)
            m_callbacks.sendInvalidCommitWelcome(transitionId);
        handleProtocolInit(m_session->GetProtocolVersion());
    }
}

void DaveSession::onWelcome(uint16_t transitionId, const std::vector<uint8_t>& welcome)
{
    auto roster = m_session->ProcessWelcome(welcome, recognizedUserIds());
    if (roster) {
        prepareRatchets(transitionId, m_session->GetProtocolVersion());
        maybeSendReadyForTransition(transitionId);
    } else {
        if (m_callbacks.sendInvalidCommitWelcome)
            m_callbacks.sendInvalidCommitWelcome(transitionId);
        sendKeyPackage();
    }
}

void DaveSession::sendKeyPackage()
{
    if (m_callbacks.sendKeyPackage)
        m_callbacks.sendKeyPackage(m_session->GetMarshalledKeyPackage());
}

void DaveSession::maybeSendReadyForTransition(uint16_t transitionId)
{
    if (transitionId != InitTransitionId && m_callbacks.sendReadyForTransition)
        m_callbacks.sendReadyForTransition(transitionId);
}

void DaveSession::setupKeyRatchetForUser(const std::string& userId, int protocolVersion)
{
    std::unique_ptr<IKeyRatchet> ratchet;
    if (protocolVersion != kDisabledVersion)
        ratchet = m_session->GetKeyRatchet(userId);

    if (userId == m_selfUserId) {
        m_protocolVersion = protocolVersion;
        m_encryptor->SetPassthroughMode(protocolVersion == kDisabledVersion);
        if (ratchet)
            m_encryptor->SetKeyRatchet(std::move(ratchet));
        return;
    }
    IDecryptor* decryptor = decryptorFor(userId);
    if (protocolVersion == kDisabledVersion)
        decryptor->TransitionToPassthroughMode(true);
    else if (ratchet)
        decryptor->TransitionToKeyRatchet(std::move(ratchet));
}

void DaveSession::handleProtocolInit(int protocolVersion)
{
    if (protocolVersion > 0) {
        handlePrepareEpoch(NewGroupEpoch, protocolVersion);
        sendKeyPackage();
    } else {
        prepareRatchets(InitTransitionId, protocolVersion);
        handleExecuteTransition(InitTransitionId);
    }
}

void DaveSession::handlePrepareEpoch(uint64_t epoch, int protocolVersion)
{
    if (epoch != NewGroupEpoch)
        return;
    std::shared_ptr<::mlspp::SignaturePrivateKey> transientKey;
    m_session->Init(ProtocolVersion(protocolVersion), m_groupId, m_selfUserId, transientKey);
}

void DaveSession::handleExecuteTransition(uint16_t transitionId)
{
    auto it = m_transitions.find(transitionId);
    if (it == m_transitions.end())
        return;
    const int protocolVersion = it->second;
    m_transitions.erase(it);
    if (protocolVersion == kDisabledVersion)
        m_session->Reset();
    setupKeyRatchetForUser(m_selfUserId, protocolVersion);
}

void DaveSession::prepareRatchets(uint16_t transitionId, int protocolVersion)
{
    for (const std::string& userId : recognizedUserIds()) {
        if (userId != m_selfUserId)
            setupKeyRatchetForUser(userId, protocolVersion);
    }
    if (transitionId == InitTransitionId)
        setupKeyRatchetForUser(m_selfUserId, protocolVersion);
    else
        m_transitions[transitionId] = protocolVersion;
    m_latestPreparedVersion = protocolVersion;
}

bool DaveSession::encrypt(uint32_t ssrc, const std::vector<uint8_t>& frame, std::vector<uint8_t>& out)
{
    if (m_assignedSsrcs.insert(ssrc).second)
        m_encryptor->AssignSsrcToCodec(ssrc, Codec::Opus);
    if (m_encryptor->IsPassthroughMode()) {
        out = frame;
        return true;
    }
    out.resize(m_encryptor->GetMaxCiphertextByteSize(MediaType::Audio, frame.size()));
    size_t written = 0;
    const auto result = m_encryptor->Encrypt(MediaType::Audio, ssrc,
                                             MakeArrayView(frame.data(), frame.size()),
                                             MakeArrayView(out.data(), out.size()), &written);
    if (result != IEncryptor::Success)
        return false;
    out.resize(written);
    return true;
}

bool DaveSession::decrypt(const std::string& userId, const std::vector<uint8_t>& frame, std::vector<uint8_t>& out)
{
    IDecryptor* decryptor = decryptorFor(userId);
    out.resize(decryptor->GetMaxPlaintextByteSize(MediaType::Audio, frame.size()));
    size_t written = 0;
    const auto result = decryptor->Decrypt(MediaType::Audio, MakeArrayView(frame.data(), frame.size()),
                                           MakeArrayView(out.data(), out.size()), &written);
    if (result != IDecryptor::Success)
        return false;
    out.resize(written);
    return true;
}
