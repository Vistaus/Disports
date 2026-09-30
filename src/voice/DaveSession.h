#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <dave/dave_interfaces.h>

// DAVE, Discord's end-to-end encryption for voice, on libdave: the MLS group
// handling of the voice gateway's DAVE opcodes (a C++ port of libdave's
// samples/typescript/DaveSessionManager.ts) and the media frame encryptor
// (our audio) and decryptors (everyone else's).
//
// Protocol version 0 means no end-to-end encryption: frames pass through.
class DaveSession
{
public:
    // How to answer the voice gateway.
    struct Callbacks {
        std::function<void(const std::vector<uint8_t>&)> sendKeyPackage;    // opcode 26 (binary)
        std::function<void(uint16_t transitionId)> sendReadyForTransition;  // opcode 23
        std::function<void(const std::vector<uint8_t>&)> sendCommitWelcome; // opcode 28 (binary)
        std::function<void(uint16_t transitionId)> sendInvalidCommitWelcome; // opcode 31
    };

    DaveSession(std::string selfUserId, uint64_t groupId, Callbacks callbacks);
    ~DaveSession();

    static int maxProtocolVersion();
    int protocolVersion() const { return m_protocolVersion; }

    // Call participants (opcodes 11 and 13).
    void addUser(const std::string& userId);
    void removeUser(const std::string& userId);

    // Voice gateway events
    void onSelectProtocolAck(int protocolVersion);                     // opcode 4
    void onPrepareTransition(uint16_t transitionId, int protocolVersion); // 21
    void onExecuteTransition(uint16_t transitionId);                   // 22
    void onPrepareEpoch(uint64_t epoch, int protocolVersion);          // 24
    void onExternalSenderPackage(const std::vector<uint8_t>& package); // 25
    void onProposals(const std::vector<uint8_t>& proposals);           // 27
    void onAnnounceCommitTransition(uint16_t transitionId, const std::vector<uint8_t>& commit); // 29
    void onWelcome(uint16_t transitionId, const std::vector<uint8_t>& welcome);                 // 30

    // Media: an Opus frame of ours (sent with this SSRC), or of a user. False
    // when it cannot go out / be played (no key yet).
    bool encrypt(uint32_t ssrc, const std::vector<uint8_t>& frame, std::vector<uint8_t>& out);
    bool decrypt(const std::string& userId, const std::vector<uint8_t>& frame, std::vector<uint8_t>& out);

private:
    std::set<std::string> recognizedUserIds() const;
    void sendKeyPackage();
    void maybeSendReadyForTransition(uint16_t transitionId);
    void setupKeyRatchetForUser(const std::string& userId, int protocolVersion);
    void handleProtocolInit(int protocolVersion);
    void handlePrepareEpoch(uint64_t epoch, int protocolVersion);
    void handleExecuteTransition(uint16_t transitionId);
    void prepareRatchets(uint16_t transitionId, int protocolVersion);
    discord::dave::IDecryptor* decryptorFor(const std::string& userId);

    std::string m_selfUserId;
    uint64_t m_groupId;
    Callbacks m_callbacks;
    std::unique_ptr<discord::dave::mls::ISession> m_session;
    std::unique_ptr<discord::dave::IEncryptor> m_encryptor;
    std::map<std::string, std::unique_ptr<discord::dave::IDecryptor>> m_decryptors;
    std::set<std::string> m_users;
    std::map<uint16_t, int> m_transitions; // transition id -> protocol version
    int m_latestPreparedVersion = 0;
    int m_protocolVersion = 0;
    std::set<uint32_t> m_assignedSsrcs;
};
