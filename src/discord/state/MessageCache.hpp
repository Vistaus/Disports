#pragma once

#include <set>
#include <map>
#include <list>
#include <nlohmann/json.hpp>
#include "../models/Snowflake.hpp"
#include "../models/ScrollDir.hpp"
#include "../models/Message.hpp"

struct MessageChunkList
{
	// int - Offset. How many messages ago was this message posted
	std::map<Snowflake, MessagePtr> m_messages;

	bool m_lastMessagesLoaded = false;
	Snowflake m_guild = 0;

	MessageChunkList();
	void ProcessRequest(ScrollDir::eScrollDir sd, Snowflake anchor, nlohmann::json& j, const std::string& channelName);
	void AddMessage(const Message& msg);
	void EditMessage(const Message& msg);
	void DeleteMessage(Snowflake message);
	int GetMentionCountSince(Snowflake message, Snowflake user);
	MessagePtr GetLoadedMessage(Snowflake message);
};

class MessageCache
{
public:
	MessageCache();
	
	void GetLoadedMessages(Snowflake channel, Snowflake guild, std::list<MessagePtr>& out);

	// note: scroll dir used to add gap message
	void ProcessRequest(Snowflake channel, ScrollDir::eScrollDir sd, Snowflake anchor, nlohmann::json& j, const std::string& channelName);

	void AddMessage(Snowflake channel, const Message& msg);
	void EditMessage(Snowflake channel, const Message& msg);
	void DeleteMessage(Snowflake channel, Snowflake message);
	int GetMentionCountSince(Snowflake channel, Snowflake message, Snowflake user);
	void ClearAllChannels();
	bool IsMessageLoaded(Snowflake channel, Snowflake message);

	MessagePtr GetLoadedMessage(Snowflake channel, Snowflake message);

	// Whether any real messages of the channel are loaded (not just the
	// cache's own rows, like the gap a channel starts with).
	bool HasMessages(Snowflake channel) const;
	// Messages from an offline cache: shown until the channel's newest
	// messages are fetched, which then replace them.
	void LoadCachedMessages(Snowflake channel, nlohmann::json& j, const std::string& channelName);

private:
	std::map <Snowflake, MessageChunkList> m_mapMessages;
	std::set <Snowflake> m_cachedChannels;
};

MessageCache* GetMessageCache();
