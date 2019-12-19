#include <sporks/bot.h>
#include <sporks/modules.h>
#include <string>
#include <cstdint>
#include <fstream>
#include <streambuf>
#include <sporks/stringops.h>
#include <sporks/database.h>

/**
 * Updates presence and counters on a schedule
 */

class PresenceModule : public Module
{
	uint64_t halfminutes;
public:
	PresenceModule(Bot* instigator, ModuleLoader* ml) : Module(instigator, ml), halfminutes(0)
	{
		ml->Attach({ I_OnPresenceUpdate }, this);
	}

	virtual ~PresenceModule()
	{
	}

	virtual std::string GetVersion()
	{
		/* NOTE: This version string below is modified by a pre-commit hook on the git repository */
		std::string version = "$ModVer 3$";
		return "1.0." + version.substr(8,version.length() - 9);
	}

	virtual std::string GetDescription()
	{
		return "Updates presence and stats counters";
	}

	int64_t GetRSS() {
		int64_t ram = 0;
		std::ifstream self_status("/proc/self/status");
		while (self_status) {
			std::string token;
			self_status >> token;
			if (token == "VmRSS:") {
				self_status >> ram;
				break;
			}
		}
		self_status.close();
		return ram;
	}

	virtual bool OnPresenceUpdate()
	{
		int64_t servers = bot->core.get_guild_count();
		int64_t users = bot->core.get_member_count();
		int64_t channel_count = bot->core.channels.size();
		int64_t ram = GetRSS();

		db::resultset rs_fact = db::query("SELECT count(key_word) AS total FROM infobot", std::vector<std::string>());
		bot->core.update_presence(Comma(from_string<size_t>(rs_fact[0]["total"], std::dec)) + " facts, on " + Comma(servers) + " servers with " + Comma(users) + " users across " + Comma(bot->core.shard_max_count) + " shards", aegis::gateway::objects::activity::Watching);
		db::query("INSERT INTO infobot_discord_counts (shard_id, dev, user_count, server_count, shard_count, channel_count, sent_messages, received_messages, memory_usage) VALUES('?','?','?','?','?','?','?','?','?') ON DUPLICATE KEY UPDATE user_count = '?', server_count = '?', shard_count = '?', channel_count = '?', sent_messages = '?', received_messages = '?', memory_usage = '?'",
			{
				std::to_string(0), std::to_string((uint32_t)bot->IsDevMode()), std::to_string(users), std::to_string(servers), std::to_string(bot->core.shard_max_count),
				std::to_string(channel_count), std::to_string(bot->sent_messages), std::to_string(bot->received_messages), std::to_string(ram),
				std::to_string(users), std::to_string(servers), std::to_string(bot->core.shard_max_count),
				std::to_string(channel_count), std::to_string(bot->sent_messages), std::to_string(bot->received_messages), std::to_string(ram)
			}
		);
		if (++halfminutes > 20) {
			/* Reset counters every 10 mins. Chewey stats uses these counters and expects this */
			halfminutes = bot->sent_messages = bot->received_messages = 0;
		}		
		return true;
	}
};

ENTRYPOINT(PresenceModule);

