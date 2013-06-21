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

#include "service.h"
#ifdef __LOGIN_SERVER__
#include "gameservers.h"
#endif
#include "networkmessage.h"

#include "database.h"
#include "game.h"
#include "chat.h"
#include "tools.h"
#include "rsa.h"
#include "textlogger.h"

#include "protocollogin.h"
#include "protocolgame.h"
#include "protocolold.h"
#include "status.h"
#include "admin.h"

#include "configmanager.h"
#include "scriptmanager.h"
#include "databasemanager.h"

#include "iologindata.h"
#include "ioban.h"

#include "dispatcher.h"
#include "outfit.h"
#include "vocation.h"
#include "task.h"
#include "tools.h"
#include "group.h"

#include "items.h"
#include "monsters.h"
#include "scheduler.h"
#include "server.h"

RSA g_RSA;

IpList serverIps;

static LoggerPtr logger = Logger::getLogger("tfts.main");


bool argumentsHandler(StringVector args)
{
	ConfigManager& configManager = server.configManager();

	StringVector tmp;
	for(StringVector::iterator it = args.begin(); it != args.end(); ++it)
	{
		if((*it) == "--help")
		{
			std::cout << "Usage:\n"
			"\n"
			"\t--config=$1\t\tAlternate configuration file path.\n"
			"\t--data-directory=$1\tAlternate data directory path.\n"
			"\t--ip=$1\t\t\tIP address of gameworld server.\n"
			"\t\t\t\tShould be equal to the global IP.\n"
			"\t--login-port=$1\tPort for login server to listen on.\n"
			"\t--game-port=$1\tPort for game server to listen on.\n"
			"\t--admin-port=$1\tPort for admin server to listen on.\n"
			"\t--status-port=$1\tPort for status server to listen on.\n";
			std::cout << "\t--runfile=$1\t\tSpecifies run file. Will contain the pid\n"
			"\t\t\t\tof the server process as long as it is running.\n";
			std::cout << "\t--output-log=$1\t\tAll standard output will be logged to\n"
			"\t\t\t\tthis file.\n"
			"\t--error-log=$1\t\tAll standard errors will be logged to\n"
			"\t\t\t\tthis file.\n";
			return false;
		}

		if((*it) == "--version")
		{
			return false;
		}

		tmp = explodeString((*it), "=");
		if(tmp[0] == "--config")
			configManager.setString(ConfigManager::CONFIG_FILE, tmp[1]);
		else if(tmp[0] == "--data-directory")
			configManager.setString(ConfigManager::DATA_DIRECTORY, tmp[1]);
		else if(tmp[0] == "--ip")
			configManager.setString(ConfigManager::IP, tmp[1]);
		else if(tmp[0] == "--login-port")
			configManager.setNumber(ConfigManager::LOGIN_PORT, atoi(tmp[1].c_str()));
		else if(tmp[0] == "--game-port")
			configManager.setNumber(ConfigManager::GAME_PORT, atoi(tmp[1].c_str()));
		else if(tmp[0] == "--admin-port")
			configManager.setNumber(ConfigManager::ADMIN_PORT, atoi(tmp[1].c_str()));
		else if(tmp[0] == "--status-port")
			configManager.setNumber(ConfigManager::STATUS_PORT, atoi(tmp[1].c_str()));
		else if(tmp[0] == "--runfile")
			configManager.setString(ConfigManager::RUNFILE, tmp[1]);
		else if(tmp[0] == "--output-log")
			configManager.setString(ConfigManager::OUT_LOG, tmp[1]);
		else if(tmp[0] == "--error-log")
			configManager.setString(ConfigManager::ERROR_LOG, tmp[1]);
	}

	return true;
}

void signalHandler(int32_t sig)
{
	if (!server.isReady()) {
		return;
	}

	Game& game = server.game();

	uint32_t tmp = 0;
	switch(sig)
	{
		case SIGHUP:
			server.dispatcher().addTask(Task::create(
				std::bind(&Game::saveGameState, &game, false)));
			break;

		case SIGCHLD:
			game.proceduralRefresh();
			break;

		case SIGUSR1:
			server.dispatcher().addTask(Task::create(
				std::bind(&Game::setGameState, &game, GAME_STATE_CLOSED)));
			break;

		case SIGUSR2:
			game.setGameState(GAME_STATE_NORMAL);
			break;

		case SIGCONT:
			game.reloadInfo(RELOAD_ALL);
			break;

		case SIGQUIT:
			server.dispatcher().addTask(Task::create(
				std::bind(&Game::setGameState, &game, GAME_STATE_SHUTDOWN)));
			break;

		case SIGTERM:
			server.dispatcher().addTask(Task::create(
				std::bind(&Game::shutdown, &game)));
			break;

		default:
			break;
	}
}

void runfileHandler(void)
{
	std::ofstream runfile(server.configManager().getString(ConfigManager::RUNFILE).c_str(), std::ios::trunc | std::ios::out);
	runfile.close();
}


void startupErrorMessage(const std::string& error)
{
	if(error.length() > 0) {
		LOGe(error);
	}

	getchar();
	exit(-1);
}


bool otserv(StringVector args, ServiceManager* services);


int main(int argc, char *argv[]) {
	int64_t startTime = OTSYS_TIME();

	server.setup();

	ConfigManager& configManager = server.configManager();

	StringVector args = StringVector(argv, argv + argc);
	if(argc > 1 && !argumentsHandler(args))
		return 0;

	ServiceManager servicer;
	configManager.startup();

	// ignore sigpipe...
	struct sigaction sigh;
	sigh.sa_handler = SIG_IGN;
	sigh.sa_flags = 0;
	sigemptyset(&sigh.sa_mask);
	sigaction(SIGPIPE, &sigh, nullptr);

	// register signals
	signal(SIGHUP, signalHandler); //save
	signal(SIGTRAP, signalHandler); //clean
	signal(SIGCHLD, signalHandler); //refresh
	signal(SIGUSR1, signalHandler); //close server
	signal(SIGUSR2, signalHandler); //open server
	signal(SIGCONT, signalHandler); //reload all
	signal(SIGQUIT, signalHandler); //save & shutdown
	signal(SIGTERM, signalHandler); //shutdown

	if(otserv(args, &servicer) && servicer.isRunning())
	{
		LOGi(std::setfill('-') << std::setw(90) << "");
		LOGi(configManager.getString(ConfigManager::SERVER_NAME) << " server online after " << (static_cast<double>(OTSYS_TIME() - startTime) / 1000.0) << " seconds!");
		LOGi(std::setfill('-') << std::setw(90) << "");

		// TODO: refactor!
		server.run();
		servicer.run();
	}
	else
	{
		LOGi(std::setfill('-') << std::setw(90) << "");
		LOGf(configManager.getString(ConfigManager::SERVER_NAME) << " server offline!");
		LOGi(std::setfill('-') << std::setw(90) << "");
	}

	if (server.scheduler().getState() == Scheduler::State::STARTED) {
		server.scheduler().stop();
	}
	if (server.dispatcher().getState() == Dispatcher::State::STARTED) {
		server.dispatcher().stop();
	}

	server.destroy();
	xmlCleanupParser();

	return 0;
}

bool otserv(StringVector args, ServiceManager* services) {
	Admin& admin = server.admin();
	ConfigManager& configManager = server.configManager();
	Game& game = server.game();
	Monsters& monsters = server.monsters();

	srand((uint32_t)OTSYS_TIME());

	game.setGameState(GAME_STATE_STARTUP);

	if(!getuid() || !geteuid())
	{
		LOGw(SOFTWARE_NAME " is running as root user! It is recommended to execute as a normal user.\nContinue? (y/N)");

		char buffer = getchar();
		if(buffer == 10 || (buffer != 121 && buffer != 89))
			startupErrorMessage("Aborted.");
	}

	LOGi("Loading configuration...");
	if(!configManager.load())
		startupErrorMessage("Unable to load " + configManager.getString(ConfigManager::CONFIG_FILE) + "!");

	DeprecatedLogger::getInstance()->open();

	std::string runPath = configManager.getString(ConfigManager::RUNFILE);
	if(runPath != "" && runPath.length() > 2)
	{
		std::ofstream runFile(runPath.c_str(), std::ios::trunc | std::ios::out);
		runFile << getpid();
		runFile.close();
		atexit(runfileHandler);
	}

	std::string encryptionType = asLowerCaseString(configManager.getString(ConfigManager::ENCRYPTION_TYPE));
	if(encryptionType == "md5")
	{
		configManager.setNumber(ConfigManager::ENCRYPTION, ENCRYPTION_MD5);
	}
	else if(encryptionType == "sha1")
	{
		configManager.setNumber(ConfigManager::ENCRYPTION, ENCRYPTION_SHA1);
	}
	else
	{
		configManager.setNumber(ConfigManager::ENCRYPTION, ENCRYPTION_PLAIN);
	}
	
	const char* p("14299623962416399520070177382898895550795403345466153217470516082934737582776038882967213386204600674145392845853859217990626450972452084065728686565928113");
	const char* q("7630979195970404721891201847792002125535401292779123937207447574596692788513647179235335529307251350570728407373705564708871762033017096809910315212884101");
	const char* d("46730330223584118622160180015036832148732986808519344675210555262940258739805766860224610646919605860206328024326703361630109888417839241959507572247284807035235569619173792292786907845791904955103601652822519121908367187885509270025388641700821735345222087940578381210879116823013776808975766851829020659073");
	g_RSA.setKey(p, q, d);

	LOGi("Connecting to database...");

	Database& db = server.database();
	db.start();
	if(db.isConnected())
	{
		// FIXME
		if(!server.databaseManager().isDatabaseSetup())
			startupErrorMessage("The database you specified in config.lua is empty, please import schemas/<dbengine>.sql to the database (if you are using MySQL, please read doc/MYSQL_HELP for more information).");
		else
		{
			uint32_t version = 0;
			do
			{
				version = server.databaseManager().updateDatabase();
				if(version == 0)
					break;

				LOGi("Database has been updated to version: " << version << ".");
			}
			while(version < VERSION_DATABASE);
		}

		server.databaseManager().checkTriggers();
		server.databaseManager().checkEncryption();
		if(configManager.getBool(ConfigManager::OPTIMIZE_DB_AT_STARTUP) && !server.databaseManager().optimizeTables())
			LOGt("No tables were optimized.");
	}
	else
		startupErrorMessage("Couldn't estabilish connection to SQL database!");

	LOGi("Loading items...");
	if (!server.items().reload()) {
		startupErrorMessage("Unable to load items!");
	}

	LOGi("Loading groups...");
	if(!Groups::getInstance()->loadFromXml())
		startupErrorMessage("Unable to load groups!");

	LOGi("Loading vocations...");
	if(!Vocations::getInstance()->loadFromXml())
		startupErrorMessage("Unable to load vocations!");

	LOGi("Loading scripts...");
	if(!ScriptingManager::getInstance()->load())
		startupErrorMessage("Unable to load scripts.");

	LOGi("Loading chat channels...");
	if(!server.chat().loadFromXml())
		startupErrorMessage("Unable to load chat channels!");

	LOGi("Loading outfits...");
	if(!Outfits::getInstance()->loadFromXml())
		startupErrorMessage("Unable to load outfits!");

	LOGi("Loading experience stages...");
	if(!game.loadExperienceStages())
		startupErrorMessage("Unable to load experience stages!");

	LOGi("Loading monsters...");
	if(!monsters.loadFromXml())
	{
		startupErrorMessage("Unable to load monsters!");
	}

	LOGi("Loading mods...");
	if(!ScriptingManager::getInstance()->loadMods())
		startupErrorMessage("Unable to load mods!");

	LOGi("Loading map...");
	if(!game.loadMap(configManager.getString(ConfigManager::MAP_NAME)))
		startupErrorMessage("");

	#ifdef __LOGIN_SERVER__
	LOGi("Loading game servers...");
	if(!GameServers::getInstance()->loadFromXml(true))
		startupErrorMessage("Unable to load game servers!");
	#endif

	LOGi("Loading admin configuration...");
	if(!admin.loadFromXml())
		startupErrorMessage("Unable to load administration protocol!");

	if (!services->add<ProtocolAdmin>(configManager.getNumber(ConfigManager::ADMIN_PORT))) {
		return false;
	}

	std::string worldType = asLowerCaseString(configManager.getString(ConfigManager::WORLD_TYPE));
	if(worldType == "pvp" || worldType == "2" || worldType == "normal")
	{
		game.setWorldType(WORLD_TYPE_PVP);
	}
	else if(worldType == "no-pvp" || worldType == "nopvp" || worldType == "non-pvp" || worldType == "nonpvp" || worldType == "1" || worldType == "safe")
	{
		game.setWorldType(WORLD_TYPE_NO_PVP);
	}
	else if(worldType == "pvp-enforced" || worldType == "pvpenforced" || worldType == "pvp-enfo" || worldType == "pvpenfo" || worldType == "pvpe" || worldType == "enforced" || worldType == "enfo" || worldType == "3" || worldType == "war")
	{
		game.setWorldType(WORLD_TYPE_PVP_ENFORCED);
	}
	else
	{
		startupErrorMessage("Unknown world type: " + configManager.getString(ConfigManager::WORLD_TYPE));
	}

	LOGi("Preparing game...");
	game.setGameState(GAME_STATE_INIT);

	std::string ip = configManager.getString(ConfigManager::IP);
	serverIps.push_back(std::make_pair(LOCALHOST, 0xFFFFFFFF));

	char hostName[128];
	hostent* host = nullptr;
	if(!gethostname(hostName, 128) && (host = gethostbyname(hostName)))
	{
		uint8_t** address = (uint8_t**)host->h_addr_list;
		while(address[0] != nullptr)
		{
			serverIps.push_back(std::make_pair(*(uint32_t*)(*address), 0x0000FFFF));
			address++;
		}
	}

	uint32_t resolvedIp = inet_addr(ip.c_str());
	if(resolvedIp == INADDR_NONE)
	{
		if((host = gethostbyname(ip.c_str())))
			resolvedIp = *(uint32_t*)host->h_addr;
		else
			startupErrorMessage("Cannot resolve " + ip + "!");
	}

	serverIps.push_back(std::make_pair(resolvedIp, 0));
	Status::getInstance()->setMapName(configManager.getString(ConfigManager::MAP_NAME));

	if (!services->add<ProtocolStatus>(configManager.getNumber(ConfigManager::STATUS_PORT))) {
		return false;
	}

	if(
#ifdef __LOGIN_SERVER__
	true
#else
	!configManager.getBool(ConfigManager::LOGIN_ONLY_LOGINSERVER)
#endif
	)
	{
		if (!services->add<ProtocolLogin>(configManager.getNumber(ConfigManager::LOGIN_PORT))) {
			return false;
		}
		if (!services->add<ProtocolOldLogin>(configManager.getNumber(ConfigManager::LOGIN_PORT))) {
			return false;
		}
	}

	if (!services->add<ProtocolGame>(configManager.getNumber(ConfigManager::GAME_PORT))) {
		return false;
	}
	if (!services->add<ProtocolOldGame>(configManager.getNumber(ConfigManager::LOGIN_PORT))) {
		return false;
	}

	LOGi("Starting game...");
	game.setGameState(GAME_STATE_NORMAL);

	game.start(services);

	LOGi("External server address: " << ip);

	return true;
}
