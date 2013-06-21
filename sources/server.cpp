////////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////

#include "otpch.h"
#include "server.h"

#include "actions.h"
#include "admin.h"
#include "chat.h"
#include "configmanager.h"
#include "creatureevent.h"
#include "databasemanager.h"
#include "dispatcher.h"
#include "game.h"
#include "globalevent.h"
#include "items.h"
#include "monsters.h"
#include "movement.h"
#include "npc.h"
#include "scheduler.h"
#include "spells.h"
#include "talkaction.h"
#include "town.h"
#include "weapons.h"


static const std::string LOGGING_CONFIGURATION_FILE("configuration/logging.xml");
static const std::string LOGGING_DIRECTORY("logs");

LOGGER_DEFINITION(Server);

Server server;


Server::Server()
	: _ready(false)
{}


Actions& Server::actions() const {
	assert(_ready);
	return *_actions;
}


Admin& Server::admin() const {
	assert(_ready);
	return *_admin;
}


Chat& Server::chat() const {
	assert(_ready);
	return *_chat;
}


ConfigManager& Server::configManager() const {
	assert(_ready);
	return *_configManager;
}


CreatureEvents& Server::creatureEvents() const {
	assert(_ready);
	return *_creatureEvents;
}


Database& Server::database() const {
	assert(_ready);
	return _databaseManager->getDatabase();
}


DatabaseManager& Server::databaseManager() const {
	assert(_ready);
	return *_databaseManager;
}


void Server::destroy() {
	if (!_ready) {
		return;
	}

	_scheduler->waitUntilStopped();
	_dispatcher->waitUntilStopped();

	_ready = false;

	LOGi("Bye!\n");

	_actions.reset();
	_admin.reset();
	_chat.reset();
	_configManager.reset();
	_creatureEvents.reset();
	_databaseManager.reset();
	_dispatcher.reset();
	_game.reset();
	_globalEvents.reset();
	_items.reset();
	_monsters.reset();
	_moveEvents.reset();
	_npcs.reset();
	_scheduler.reset();
	_spells.reset();
	_talkActions.reset();
	_towns.reset();
	_weapons.reset();
}


Dispatcher& Server::dispatcher() const {
	assert(_ready);
	return *_dispatcher;
}


Game& Server::game() const {
	assert(_ready);
	return *_game;
}


GlobalEvents& Server::globalEvents() const {
	assert(_ready);
	return *_globalEvents;
}


Items& Server::items() const {
	assert(_ready);
	return *_items;
}


Monsters& Server::monsters() const {
	assert(_ready);
	return *_monsters;
}


MoveEvents& Server::moveEvents() const {
	assert(_ready);
	return *_moveEvents;
}


Npcs& Server::npcs() const {
	assert(_ready);
	return *_npcs;
}


bool Server::isReady() const {
	return _ready;
}


void Server::run() {
	if (!_ready) {
		return;
	}

	_dispatcher->start();
	_scheduler->start();
}


Scheduler& Server::scheduler() const {
	assert(_ready);
	return *_scheduler;
}


void Server::setup() {
	if (_ready) {
		return;
	}

	setupLogging();
	setupComponents();

	_ready = true;
}


void Server::setupComponents() {
	_actions.reset(new Actions);
	_admin.reset(new Admin);
	_chat.reset(new Chat);
	_configManager.reset(new ConfigManager);
	_creatureEvents.reset(new CreatureEvents);
	_databaseManager.reset(new DatabaseManager);
	_dispatcher.reset(new Dispatcher);
	_game.reset(new Game);
	_globalEvents.reset(new GlobalEvents);
	_items.reset(new Items);
	_monsters.reset(new Monsters);
	_moveEvents.reset(new MoveEvents);
	_npcs.reset(new Npcs);
	_scheduler.reset(new Scheduler);
	_spells.reset(new Spells);
	_talkActions.reset(new TalkActions);
	_towns.reset(new Towns);
	_weapons.reset(new Weapons);
}


void Server::setupLogging() {
	using namespace boost;
	using namespace log4cxx;
	using namespace log4cxx::helpers;
	using namespace log4cxx::xml;

	LoggerPtr rootLogger = Logger::getRootLogger();
	rootLogger->addAppender(new ConsoleAppender(new PatternLayout("%d | %5p | %m%n")));
	rootLogger->setLevel(Level::getDebug());

	bool directoryCreationFailed = false;
	bool configurationFileMissing = false;

	if (!filesystem::is_directory(LOGGING_DIRECTORY)) {
		system::error_code error;
		if (filesystem::create_directory(LOGGING_DIRECTORY, error)) {
			chmod(LOGGING_DIRECTORY.c_str(), S_IRWXU);
		}
		else {
			directoryCreationFailed = true;
		}
	}

	// sadly we cannot check whether we were successful or not :(
	if (filesystem::is_regular_file(LOGGING_CONFIGURATION_FILE) || filesystem::is_symlink(LOGGING_CONFIGURATION_FILE)) {
		DOMConfigurator::configureAndWatch(LOGGING_CONFIGURATION_FILE);
	}
	else {
		configurationFileMissing = true;
	}

	LOGi(std::setfill('-') << std::setw(90) << "");
	LOGi(SOFTWARE_NAME " " SOFTWARE_VERSION " for Tibia(R) " SOFTWARE_PROTOCOL);
	LOGi(X_PROJECT_URL);
	LOGi(std::setfill('-') << std::setw(90) << "");

	if (configurationFileMissing) {
		LOGe("Cannot find logging configuration file '" << LOGGING_CONFIGURATION_FILE << "'. Using defaults.");
	}
	if (directoryCreationFailed) {
		LOGe("Cannot create logging directory '" << LOGGING_DIRECTORY << "'. No log files will be written.");
	}
}


Spells& Server::spells() const {
	assert(_ready);
	return *_spells;
}


TalkActions& Server::talkActions() const {
	assert(_ready);
	return *_talkActions;
}


Towns& Server::towns() const {
	assert(_ready);
	return *_towns;
}


Weapons& Server::weapons() const {
	assert(_ready);
	return *_weapons;
}
