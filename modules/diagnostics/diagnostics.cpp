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
#include <sporks/regex.h>
#include <sporks/modules.h>
#include <sporks/stringops.h>
#include <sporks/database.h>
#include <sstream>

/**
 * Provides diagnostic commands for monitoring the bot and debugging it interactively while it's running.
 */

class DiagnosticsModule : public Module
{
	PCRE* diagnosticmessage;
public:
	DiagnosticsModule(Bot* instigator, ModuleLoader* ml) : Module(instigator, ml)
	{
		ml->Attach({ I_OnMessage }, this);
		diagnosticmessage = new PCRE("^sudo(|\\s+(.+?))$", true);
	}

	virtual ~DiagnosticsModule()
	{
		delete diagnosticmessage;
	}

	virtual std::string GetVersion()
	{
		/* NOTE: This version string below is modified by a pre-commit hook on the git repository */
		std::string version = "$ModVer 12$";
		return "1.0." + version.substr(8,version.length() - 9);
	}

	virtual std::string GetDescription()
	{
		return "Diagnostic Commands (sudo), '@Sporks sudo'";
	}

	virtual bool OnMessage(const aegis::gateway::events::message_create &message, const std::string& clean_message, bool mentioned, const std::vector<std::string> &stringmentions)
	{
		std::vector<std::string> param;
		std::string botusername = bot->user.username;
		aegis::gateway::objects::message msg = message.msg;

		if (mentioned && diagnosticmessage->Match(clean_message, param)) {

			std::stringstream tokens(trim(param[2]));
			std::string subcommand;
			tokens >> subcommand;

			/* Get owner snowflake id from config file */
			int64_t owner_id = from_string<int64_t>(Bot::GetConfig("owner"), std::dec);

			/* Only allow these commands to the bot owner */
			if (msg.author.id.get() == owner_id) {

				if (param.size() < 3) {
					/* Invalid number of parameters */
					EmbedSimple("Sudo make me a sandwich.", msg.get_channel_id().get());
				} else {
					/* Module list command */
					if (lowercase(subcommand) == "modules") {
						std::stringstream s;

						// NOTE: GetModuleList's reference is safe from within a module event
						const ModMap& modlist = bot->Loader->GetModuleList();

						s << "```diff" << std::endl;
						s << fmt::format("- ╭─────────────────────────┬───────────┬────────────────────────────────────────────────╮") << std::endl;
						s << fmt::format("- │ Filename                | Version   | Description                                    |") << std::endl;
						s << fmt::format("- ├─────────────────────────┼───────────┼────────────────────────────────────────────────┤") << std::endl;

						for (auto mod = modlist.begin(); mod != modlist.end(); ++mod) {
							s << fmt::format("+ │ {:23} | {:9} | {:46} |", mod->first, mod->second->GetVersion(), mod->second->GetDescription()) << std::endl;
						}
						s << fmt::format("+ ╰─────────────────────────┴───────────┴────────────────────────────────────────────────╯") << std::endl;
						s << "```";

						aegis::channel* c = bot->core.find_channel(msg.get_channel_id().get());
						if (c) {
							c->create_message(s.str());
						}
						
					} else if (lowercase(subcommand) == "load") {
						/* Load a module */
						std::string modfile;
						tokens >> modfile;
						if (bot->Loader->Load(modfile)) {
							EmbedSimple("Loaded module: " + modfile, msg.get_channel_id().get());
						} else {
							EmbedSimple(std::string("Can't do that: ``") + bot->Loader->GetLastError() + "``", msg.get_channel_id().get());
						}
					} else if (lowercase(subcommand) == "unload") {
						/* Unload a module */
						std::string modfile;
						tokens >> modfile;
						if (modfile == "module_diagnostics,so") {
							EmbedSimple("I suppose you think that's funny, dont you? *I'm sorry. can't do that, dave.*", msg.get_channel_id().get());
						} else {
							if (bot->Loader->Unload(modfile)) {
								EmbedSimple("Unloaded module: " + modfile, msg.get_channel_id().get());
							} else {
								EmbedSimple(std::string("Can't do that: ``") + bot->Loader->GetLastError() + "``", msg.get_channel_id().get());
							}
						}
					} else if (lowercase(subcommand) == "reload") {
						/* Reload a currently loaded module */
						std::string modfile;
						tokens >> modfile;
						if (modfile == "module_diagnostics.so") {
							EmbedSimple("I suppose you think that's funny, dont you? *I'm sorry. can't do that, dave.*", msg.get_channel_id().get());
						} else {
							if (bot->Loader->Reload(modfile)) {
								EmbedSimple("Reloaded module: " + modfile, msg.get_channel_id().get());
							} else {
								EmbedSimple(std::string("Can't do that: ``") + bot->Loader->GetLastError() + "``", msg.get_channel_id().get());
							}
						}
					} else if (lowercase(subcommand) == "lock") {
						std::string keyword;
						std::getline(tokens, keyword);
						db::query("UPDATE infobot SET locked = 1 WHERE key_word = '?'", {keyword});
						EmbedSimple("**Locked** key word: " + keyword, msg.get_channel_id().get());
					} else if (lowercase(subcommand) == "unlock") {
						std::string keyword;
						std::getline(tokens, keyword);
						db::query("UPDATE infobot SET locked = 0 WHERE key_word = '?'", {keyword});
						EmbedSimple("**Unlocked** key word: " + keyword, msg.get_channel_id().get());
					} else {
						/* Invalid command */
						EmbedSimple("Sudo **what**? I don't know what that command means.", msg.get_channel_id().get());
					}
				}
			} else {
				/* Access denied */
				EmbedSimple("Make your own sandwich, mortal.", msg.get_channel_id().get());
			}

			/* Eat the event */
			return false;
		}
		return true;
	}
};

ENTRYPOINT(DiagnosticsModule);

