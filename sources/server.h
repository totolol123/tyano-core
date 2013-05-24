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

#ifndef _SERVER_H
#define _SERVER_H

class Actions;
class Admin;
class Chat;
class ConfigManager;
class CreatureEvents;
class Database;
class DatabaseManager;
class Dispatcher;
class Game;
class GlobalEvents;
class Items;
class Monsters;
class MoveEvents;
class Npcs;
class Scheduler;
class Server;
class Spells;
class TalkActions;
class Towns;
class Weapons;

extern Server server;


class Server {

public:

	Server(); // static class

	Actions&         actions() const;
	Admin&           admin() const;
	Chat&            chat() const;
	ConfigManager&   configManager() const;
	CreatureEvents&  creatureEvents() const;
	Database&        database() const;
	DatabaseManager& databaseManager() const;
	void             destroy();
	Dispatcher&      dispatcher() const;
	Game&            game() const;
	GlobalEvents&    globalEvents() const;
	Items&           items() const;
	Monsters&        monsters() const;
	MoveEvents&      moveEvents() const;
	Npcs&            npcs() const;
	bool             isReady() const;
	void             run();
	Scheduler&       scheduler() const;
	void             setup();
	Spells&          spells() const;
	TalkActions&     talkActions() const;
	Towns&           towns() const;
	Weapons&         weapons() const;


private:

	void setupComponents ();
	void setupLogging    ();


	LOGGER_DECLARATION;

	bool _ready;

	std::unique_ptr<Actions>         _actions;
	std::unique_ptr<Admin>           _admin;
	std::unique_ptr<Chat>            _chat;
	std::unique_ptr<ConfigManager>   _configManager;
	std::unique_ptr<CreatureEvents>  _creatureEvents;
	std::unique_ptr<DatabaseManager> _databaseManager;
	std::unique_ptr<Dispatcher>      _dispatcher;
	std::unique_ptr<Game>            _game;
	std::unique_ptr<GlobalEvents>    _globalEvents;
	std::unique_ptr<Items>           _items;
	std::unique_ptr<Monsters>        _monsters;
	std::unique_ptr<MoveEvents>      _moveEvents;
	std::unique_ptr<Npcs>            _npcs;
	std::unique_ptr<Scheduler>       _scheduler;
	std::unique_ptr<Spells>          _spells;
	std::unique_ptr<TalkActions>     _talkActions;
	std::unique_ptr<Towns>           _towns;
	std::unique_ptr<Weapons>         _weapons;

};

#endif // _SERVER_H
