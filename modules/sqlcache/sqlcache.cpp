/************************************************************************************
 * 
 * Sporks, the learning, scriptable Discord bot!
 *
 * Copyright 2019 Craig Edwards <support@sporks.gg>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/

#include <sporks/bot.h>
#include <sporks/modules.h>
#include <sporks/stringops.h>
#include <sporks/database.h>
#include <sporks/config.h>
#include <sstream>
#include <thread>

/**
 * Provides caching of users, guilds and memberships to an sql database for use by external programs.
 */

class SQLCacheModule : public Module
{
	/* User queue processing thread */
	std::thread* thr_userqueue;

	/* Safety mutex for userqueue */
	std::mutex user_cache_mutex;

	/* True if the thread is to terminate */
	bool terminate;

	/* Userqueue: a queue of users waiting to be written to SQL for the dashboard */
	std::queue<aegis::gateway::objects::user> userqueue;
public:

	void SaveCachedUsersThread() {
		time_t last_message = time(NULL);
		aegis::gateway::objects::user u;
		while (!this->terminate) {
			if (!userqueue.empty()) {
				{
					std::lock_guard<std::mutex> user_cache_lock(user_cache_mutex);
					u = userqueue.front();
					userqueue.pop();
					bot->counters["userqueue"] = userqueue.size();
				};
				std::string userid = std::to_string(u.id.get());
				std::string bot = u.is_bot() ? "1" : "0";
				db::query("INSERT INTO infobot_discord_user_cache (id, username, discriminator, avatar, bot) VALUES(?, '?', '?', '?', ?) ON DUPLICATE KEY UPDATE username = '?', discriminator = '?', avatar = '?'", {userid, u.username, u.discriminator, u.avatar, bot, u.username, u.discriminator, u.avatar});
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			} else {
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			if (time(NULL) > last_message) {
				if (userqueue.size() > 0) {
					bot->core.log->info("User queue size: {} objects", userqueue.size());
				}
				last_message = time(NULL) + 60;
			}
		}
	}

	SQLCacheModule(Bot* instigator, ModuleLoader* ml) : Module(instigator, ml), thr_userqueue(nullptr), terminate(false)
	{
		ml->Attach({ I_OnGuildCreate, I_OnPresenceUpdate, I_OnGuildMemberAdd, I_OnChannelCreate, I_OnChannelDelete, I_OnGuildDelete }, this);
		bot->counters["userqueue"] = 0;
		thr_userqueue = new std::thread(&SQLCacheModule::SaveCachedUsersThread, this);
	}

	virtual ~SQLCacheModule()
	{
		terminate = true;
		bot->DisposeThread(thr_userqueue);
		bot->counters["userqueue"] = 0;
	}

	virtual std::string GetVersion()
	{
		/* NOTE: This version string below is modified by a pre-commit hook on the git repository */
		std::string version = "$ModVer 2$";
		return "1.0." + version.substr(8,version.length() - 9);
	}

	virtual std::string GetDescription()
	{
		return "User/Guild SQL Cache";
	}

	virtual bool OnGuildCreate(const modevent::guild_create &gc)
	{
		db::query("INSERT INTO infobot_shard_map (guild_id, shard_id, name, icon, unavailable) VALUES('?','?','?','?','?') ON DUPLICATE KEY UPDATE shard_id = '?', name = '?', icon = '?', unavailable = '?'",
			{
				std::to_string(gc.guild.id.get()),
				std::to_string(gc.shard.get_id()),
				gc.guild.name,
				gc.guild.icon,
				std::to_string(gc.guild.unavailable),
				std::to_string(gc.shard.get_id()),
				gc.guild.name,
				gc.guild.icon,
				std::to_string(gc.guild.unavailable)
			}
		);

		for (auto i = gc.guild.channels.begin(); i != gc.guild.channels.end(); ++i) {
			getSettings(bot, i->id.get(), gc.guild.id.get());
		}

		for (auto i = gc.guild.members.begin(); i != gc.guild.members.end(); ++i) {
			std::lock_guard<std::mutex> user_cache_lock(user_cache_mutex);
			userqueue.push(i->_user);
			bot->counters["userqueue"] = userqueue.size();
		}

		return true;
	}

	virtual bool OnPresenceUpdate()
	{
		const aegis::shards::shard_mgr& s = bot->core.get_shard_mgr();
		const std::vector<std::unique_ptr<aegis::shards::shard>>& shards = s.get_shards();
		for (auto i = shards.begin(); i != shards.end(); ++i) {
			const aegis::shards::shard* shard = i->get();
			db::query("INSERT INTO infobot_shard_status (id, connected, online, uptime, transfer, transfer_compressed) VALUES('?','?','?','?','?','?') ON DUPLICATE KEY UPDATE connected = '?', online = '?', uptime = '?', transfer = '?', transfer_compressed = '?'",
				{
					std::to_string(shard->get_id()),
					std::to_string(shard->is_connected()),
					std::to_string(shard->is_online()),
					std::to_string(shard->uptime()),
					std::to_string(shard->get_transfer_u()),
					std::to_string(shard->get_transfer()),
					std::to_string(shard->is_connected()),
					std::to_string(shard->is_online()),
					std::to_string(shard->uptime()),
					std::to_string(shard->get_transfer_u()),
					std::to_string(shard->get_transfer())
				}
			);
		}
		return true;
	}

	virtual bool OnGuildMemberAdd(const modevent::guild_member_add &gma)
	{
		std::string userid = std::to_string(gma.member._user.id.get());
		std::string bot = gma.member._user.is_bot() ? "1" : "0";
		db::query("INSERT INTO infobot_discord_user_cache (id, username, discriminator, avatar, bot) VALUES(?, '?', '?', '?', ?) ON DUPLICATE KEY UPDATE username = '?', discriminator = '?', avatar = '?'", {userid, gma.member._user.username, gma.member._user.discriminator, gma.member._user.avatar, bot, gma.member._user.username, gma.member._user.discriminator, gma.member._user.avatar});		
		return true;
	}

	virtual bool OnChannelCreate(const modevent::channel_create channel_create)
	{
		getSettings(bot, channel_create.channel.id.get(), channel_create.channel.guild_id.get());
		return true;
	}

	virtual bool OnChannelDelete(const modevent::channel_delete cd)
	{
		db::query("DELETE FROM infobot_discord_settings WHERE id = '?'", {std::to_string(cd.channel.id.get())});
		return true;
	}

	virtual bool OnGuildDelete(const modevent::guild_delete gd)
	{
		db::query("DELETE FROM infobot_discord_settings WHERE guild_id = '?'", {std::to_string(gd.guild_id.get())});
		db::query("DELETE FROM infobot_shard_map WHERE guild_id = '?'", {std::to_string(gd.guild_id.get())});
		return true;
	}
};

ENTRYPOINT(SQLCacheModule);
