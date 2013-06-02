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

#ifndef _CONST_H
#define _CONST_H


enum DistributionType_t
{
	DISTRO_UNIFORM,
	DISTRO_SQUARE,
	DISTRO_NORMAL
};


enum class FileType {
	CONFIG,
	LOG,
	MOD,
	OTHER,
	XML,
};


enum skillsid_t
{
	SKILL_LEVEL = 0,
	SKILL_TRIES = 1,
	SKILL_PERCENT = 2
};

enum playerinfo_t
{
	PLAYERINFO_LEVEL,
	PLAYERINFO_LEVELPERCENT,
	PLAYERINFO_HEALTH,
	PLAYERINFO_MAXHEALTH,
	PLAYERINFO_MANA,
	PLAYERINFO_MAXMANA,
	PLAYERINFO_MAGICLEVEL,
	PLAYERINFO_MAGICLEVELPERCENT,
	PLAYERINFO_SOUL,
};

enum freeslot_t
{
	SLOT_TYPE_NONE,
	SLOT_TYPE_INVENTORY,
	SLOT_TYPE_CONTAINER
};

enum chaseMode_t
{
	CHASEMODE_STANDSTILL,
	CHASEMODE_FOLLOW,
};

enum fightMode_t
{
	FIGHTMODE_ATTACK,
	FIGHTMODE_BALANCED,
	FIGHTMODE_DEFENSE
};

enum secureMode_t
{
	SECUREMODE_ON,
	SECUREMODE_OFF
};

enum tradestate_t
{
	TRADE_NONE,
	TRADE_INITIATED,
	TRADE_ACCEPT,
	TRADE_ACKNOWLEDGE,
	TRADE_TRANSFER
};

enum AccountManager_t
{
	MANAGER_NONE,
	MANAGER_NEW,
	MANAGER_ACCOUNT,
	MANAGER_NAMELOCK
};

enum GamemasterCondition_t
{
	GAMEMASTER_INVISIBLE = 0,
	GAMEMASTER_IGNORE = 1,
	GAMEMASTER_TELEPORT = 2
};

enum Exhaust_t
{
    EXHAUST_COMBAT = 1,
    EXHAUST_HEALING = 2
};


enum InteractType_t
{
	INTERACT_TEXT,
	INTERACT_EVENT
};

enum NpcEvent_t
{
	EVENT_NONE,
	EVENT_THINK,
	EVENT_BUSY,
	EVENT_IDLE,
	EVENT_PLAYER_ENTER,
	EVENT_PLAYER_MOVE,
	EVENT_PLAYER_LEAVE,
	EVENT_PLAYER_SHOPSELL,
	EVENT_PLAYER_SHOPBUY,
	EVENT_PLAYER_SHOPCLOSE
};

enum ResponseType_t
{
	RESPONSE_DEFAULT,
	RESPONSE_SCRIPT,
};

enum RespondParam_t
{
	RESPOND_DEFAULT = 0,
	RESPOND_MALE = 1 << 0,
	RESPOND_FEMALE = 1 << 1,
	RESPOND_PZBLOCK = 1 << 2,
	RESPOND_LOWMONEY = 1 << 3,
	RESPOND_NOAMOUNT = 1 << 4,
	RESPOND_LOWAMOUNT = 1 << 5,
	RESPOND_PREMIUM = 1 << 6,
	RESPOND_DRUID = 1 << 7,
	RESPOND_KNIGHT = 1 << 8,
	RESPOND_PALADIN = 1 << 9,
	RESPOND_SORCERER = 1 << 10,
	RESPOND_LOWLEVEL = 1 << 11
};

enum ReponseActionParam_t
{
	ACTION_NONE,
	ACTION_SETTOPIC,
	ACTION_SETLEVEL,
	ACTION_SETPRICE,
	ACTION_SETBUYPRICE,
	ACTION_SETSELLPRICE,
	ACTION_TAKEMONEY,
	ACTION_GIVEMONEY,
	ACTION_SELLITEM,
	ACTION_BUYITEM,
	ACTION_GIVEITEM,
	ACTION_TAKEITEM,
	ACTION_SETAMOUNT,
	ACTION_SETITEM,
	ACTION_SETSUBTYPE,
	ACTION_SETEFFECT,
	ACTION_SETSPELL,
	ACTION_SETLISTNAME,
	ACTION_SETLISTPNAME,
	ACTION_TEACHSPELL,
	ACTION_UNTEACHSPELL,
	ACTION_SETSTORAGE,
	ACTION_SETTELEPORT,
	ACTION_SCRIPT,
	ACTION_SCRIPTPARAM,
	ACTION_ADDQUEUE,
	ACTION_SETIDLE
};

enum ShopEvent_t
{
	SHOPEVENT_SELL,
	SHOPEVENT_BUY,
	SHOPEVENT_CLOSE
};

enum StorageComparision_t
{
	STORAGE_LESS,
	STORAGE_LESSOREQUAL,
	STORAGE_EQUAL,
	STORAGE_NOTEQUAL,
	STORAGE_GREATEROREQUAL,
	STORAGE_GREATER
};


enum CreatureEventType_t
{
	CREATURE_EVENT_NONE,
	CREATURE_EVENT_LOGIN,
	CREATURE_EVENT_LOGOUT,
	CREATURE_EVENT_CHANNEL_JOIN,
	CREATURE_EVENT_CHANNEL_LEAVE,
	CREATURE_EVENT_ADVANCE,
	CREATURE_EVENT_LOOK,
	CREATURE_EVENT_DIRECTION,
	CREATURE_EVENT_OUTFIT,
	CREATURE_EVENT_MAIL_SEND,
	CREATURE_EVENT_MAIL_RECEIVE,
	CREATURE_EVENT_TRADE_REQUEST,
	CREATURE_EVENT_TRADE_ACCEPT,
	CREATURE_EVENT_TEXTEDIT,
	CREATURE_EVENT_REPORTBUG,
	CREATURE_EVENT_THINK,
	CREATURE_EVENT_STATSCHANGE,
	CREATURE_EVENT_COMBAT_AREA,
	CREATURE_EVENT_PUSH,
	CREATURE_EVENT_TARGET,
	CREATURE_EVENT_FOLLOW,
	CREATURE_EVENT_COMBAT,
	CREATURE_EVENT_ATTACK,
	CREATURE_EVENT_CAST,
	CREATURE_EVENT_KILL,
	CREATURE_EVENT_DEATH,
	CREATURE_EVENT_PREPAREDEATH
};

enum StatsChange_t
{
	STATSCHANGE_HEALTHGAIN,
	STATSCHANGE_HEALTHLOSS,
	STATSCHANGE_MANAGAIN,
	STATSCHANGE_MANALOSS
};


enum ZoneType_t
{
	ZONE_PROTECTION,
	ZONE_NOPVP,
	ZONE_PVP,
	ZONE_NOLOGOUT,
	ZONE_NORMAL
};


enum ReturnValue
{
	RET_DONTSHOWMESSAGE = 0,
	RET_NOERROR = 1,
	RET_NOTPOSSIBLE = 2,
	RET_NOTENOUGHROOM = 3,
	RET_PLAYERISPZLOCKED = 4,
	RET_PLAYERISNOTINVITED = 5,
	RET_CANNOTTHROW = 6,
	RET_THEREISNOWAY = 7,
	RET_DESTINATIONOUTOFREACH = 8,
	RET_CREATUREBLOCK = 9,
	RET_NOTMOVEABLE = 10,
	RET_DROPTWOHANDEDITEM = 11,
	RET_BOTHHANDSNEEDTOBEFREE = 12,
	RET_CANONLYUSEONEWEAPON = 13,
	RET_NEEDEXCHANGE = 14,
	RET_CANNOTBEDRESSED = 15,
	RET_PUTTHISOBJECTINYOURHAND = 16,
	RET_PUTTHISOBJECTINBOTHHANDS = 17,
	RET_TOOFARAWAY = 18,
	RET_FIRSTGODOWNSTAIRS = 19,
	RET_FIRSTGOUPSTAIRS = 20,
	RET_CONTAINERNOTENOUGHROOM = 21,
	RET_NOTENOUGHCAPACITY = 22,
	RET_CANNOTPICKUP = 23,
	RET_THISISIMPOSSIBLE = 24,
	RET_DEPOTISFULL = 25,
	RET_CREATUREDOESNOTEXIST = 26,
	RET_CANNOTUSETHISOBJECT = 27,
	RET_PLAYERWITHTHISNAMEISNOTONLINE = 28,
	RET_NOTREQUIREDLEVELTOUSERUNE = 29,
	RET_YOUAREALREADYTRADING = 30,
	RET_THISPLAYERISALREADYTRADING = 31,
	RET_YOUMAYNOTLOGOUTDURINGAFIGHT = 32,
	RET_DIRECTPLAYERSHOOT = 33,
	RET_NOTENOUGHLEVEL = 34,
	RET_NOTENOUGHMAGICLEVEL = 35,
	RET_NOTENOUGHMANA = 36,
	RET_NOTENOUGHSOUL = 37,
	RET_YOUAREEXHAUSTED = 38,
	RET_PLAYERISNOTREACHABLE = 39,
	RET_CANONLYUSETHISRUNEONCREATURES = 40,
	RET_ACTIONNOTPERMITTEDINPROTECTIONZONE = 41,
	RET_YOUMAYNOTATTACKTHISPLAYER = 42,
	RET_YOUMAYNOTATTACKAPERSONINPROTECTIONZONE = 43,
	RET_YOUMAYNOTATTACKAPERSONWHILEINPROTECTIONZONE = 44,
	RET_YOUMAYNOTATTACKTHISCREATURE = 45,
	RET_YOUCANONLYUSEITONCREATURES = 46,
	RET_CREATUREISNOTREACHABLE = 47,
	RET_TURNSECUREMODETOATTACKUNMARKEDPLAYERS = 48,
	RET_YOUNEEDPREMIUMACCOUNT = 49,
	RET_YOUNEEDTOLEARNTHISSPELL = 50,
	RET_YOURVOCATIONCANNOTUSETHISSPELL = 51,
	RET_YOUNEEDAWEAPONTOUSETHISSPELL = 52,
	RET_PLAYERISPZLOCKEDLEAVEPVPZONE = 53,
	RET_PLAYERISPZLOCKEDENTERPVPZONE = 54,
	RET_ACTIONNOTPERMITTEDINANOPVPZONE = 55,
	RET_YOUCANNOTLOGOUTHERE = 56,
	RET_YOUNEEDAMAGICITEMTOCASTSPELL = 57,
	RET_CANNOTCONJUREITEMHERE = 58,
	RET_YOUNEEDTOSPLITYOURSPEARS = 59,
	RET_NAMEISTOOAMBIGUOUS = 60,
	RET_CANONLYUSEONESHIELD = 61,
	RET_YOUARENOTTHEOWNER = 62,
	RET_YOUMAYNOTCASTAREAONBLACKSKULL = 63,
	RET_TILEISFULL = 64
};


enum MagicEffect_t
{
	MAGIC_EFFECT_DRAW_BLOOD	= 0x00,
	MAGIC_EFFECT_LOSE_ENERGY	= 0x01,
	MAGIC_EFFECT_POFF		= 0x02,
	MAGIC_EFFECT_BLOCKHIT		= 0x03,
	MAGIC_EFFECT_EXPLOSION_AREA	= 0x04,
	MAGIC_EFFECT_EXPLOSION_DAMAGE	= 0x05,
	MAGIC_EFFECT_FIRE_AREA		= 0x06,
	MAGIC_EFFECT_YELLOW_RINGS	= 0x07,
	MAGIC_EFFECT_POISON_RINGS	= 0x08,
	MAGIC_EFFECT_HIT_AREA		= 0x09,
	MAGIC_EFFECT_TELEPORT		= 0x0A, //10
	MAGIC_EFFECT_ENERGY_DAMAGE	= 0x0B, //11
	MAGIC_EFFECT_WRAPS_BLUE	= 0x0C, //12
	MAGIC_EFFECT_WRAPS_RED	= 0x0D, //13
	MAGIC_EFFECT_WRAPS_GREEN	= 0x0E, //14
	MAGIC_EFFECT_HITBY_FIRE	= 0x0F, //15
	MAGIC_EFFECT_POISON		= 0x10, //16
	MAGIC_EFFECT_MORT_AREA		= 0x11, //17
	MAGIC_EFFECT_SOUND_GREEN	= 0x12, //18
	MAGIC_EFFECT_SOUND_RED		= 0x13, //19
	MAGIC_EFFECT_POISON_AREA	= 0x14, //20
	MAGIC_EFFECT_SOUND_YELLOW	= 0x15, //21
	MAGIC_EFFECT_SOUND_PURPLE	= 0x16, //22
	MAGIC_EFFECT_SOUND_BLUE	= 0x17, //23
	MAGIC_EFFECT_SOUND_WHITE	= 0x18, //24
	MAGIC_EFFECT_BUBBLES		= 0x19, //25
	MAGIC_EFFECT_CRAPS		= 0x1A, //26
	MAGIC_EFFECT_GIFT_WRAPS	= 0x1B, //27
	MAGIC_EFFECT_FIREWORK_YELLOW	= 0x1C, //28
	MAGIC_EFFECT_FIREWORK_RED	= 0x1D, //29
	MAGIC_EFFECT_FIREWORK_BLUE	= 0x1E, //30
	MAGIC_EFFECT_STUN		= 0x1F, //31
	MAGIC_EFFECT_SLEEP		= 0x20, //32
	MAGIC_EFFECT_WATERCREATURE	= 0x21, //33
	MAGIC_EFFECT_GROUNDSHAKER	= 0x22, //34
	MAGIC_EFFECT_HEARTS		= 0x23, //35
	MAGIC_EFFECT_FIREATTACK	= 0x24, //36
	MAGIC_EFFECT_ENERGY_AREA	= 0x25, //37
	MAGIC_EFFECT_SMALLCLOUDS	= 0x26, //38
	MAGIC_EFFECT_HOLYDAMAGE	= 0x27, //39
	MAGIC_EFFECT_BIGCLOUDS		= 0x28, //40
	MAGIC_EFFECT_ICEAREA		= 0x29, //41
	MAGIC_EFFECT_ICETORNADO	= 0x2A, //42
	MAGIC_EFFECT_ICEATTACK		= 0x2B, //43
	MAGIC_EFFECT_STONES		= 0x2C, //44
	MAGIC_EFFECT_SMALLPLANTS	= 0x2D, //45
	MAGIC_EFFECT_CARNIPHILA	= 0x2E, //46
	MAGIC_EFFECT_PURPLEENERGY	= 0x2F, //47
	MAGIC_EFFECT_YELLOWENERGY	= 0x30, //48
	MAGIC_EFFECT_HOLYAREA		= 0x31, //49
	MAGIC_EFFECT_BIGPLANTS		= 0x32, //50
	MAGIC_EFFECT_CAKE		= 0x33, //51
	MAGIC_EFFECT_GIANTICE		= 0x34, //52
	MAGIC_EFFECT_WATERSPLASH	= 0x35, //53
	MAGIC_EFFECT_PLANTATTACK	= 0x36, //54
	MAGIC_EFFECT_TUTORIALARROW	= 0x37, //55
	MAGIC_EFFECT_TUTORIALSQUARE	= 0x38, //56
	MAGIC_EFFECT_MIRRORHORIZONTAL	= 0x39, //57
	MAGIC_EFFECT_MIRRORVERTICAL	= 0x3A, //58
	MAGIC_EFFECT_SKULLHORIZONTAL	= 0x3B, //59
	MAGIC_EFFECT_SKULLVERTICAL	= 0x3C, //60
	MAGIC_EFFECT_ASSASSIN		= 0x3D, //61
	MAGIC_EFFECT_STEPSHORIZONTAL	= 0x3E, //62
	MAGIC_EFFECT_BLOODYSTEPS	= 0x3F, //63
	MAGIC_EFFECT_STEPSVERTICAL	= 0x40, //64
	MAGIC_EFFECT_YALAHARIGHOST	= 0x41, //65
	MAGIC_EFFECT_BATS		= 0x42, //66
	MAGIC_EFFECT_SMOKE		= 0x43, //67
	MAGIC_EFFECT_INSECTS		= 0x44, //68
    MAGIC_EFFECT_DRAGONHEAD		= 0x45, //69
	MAGIC_EFFECT_LAST		= MAGIC_EFFECT_DRAGONHEAD,

	//for internal use, dont send to client
	MAGIC_EFFECT_NONE    = 0x46,
	MAGIC_EFFECT_UNKNOWN = 0x47,
};

enum ShootEffect_t
{
	SHOOT_EFFECT_SPEAR		= 0x00,
	SHOOT_EFFECT_BOLT		= 0x01,
	SHOOT_EFFECT_ARROW		= 0x02,
	SHOOT_EFFECT_FIRE		= 0x03,
	SHOOT_EFFECT_ENERGY		= 0x04,
	SHOOT_EFFECT_POISONARROW	= 0x05,
	SHOOT_EFFECT_BURSTARROW	= 0x06,
	SHOOT_EFFECT_THROWINGSTAR	= 0x07,
	SHOOT_EFFECT_THROWINGKNIFE	= 0x08,
	SHOOT_EFFECT_SMALLSTONE	= 0x09,
	SHOOT_EFFECT_DEATH		= 0x0A, //10
	SHOOT_EFFECT_LARGEROCK	= 0x0B, //11
	SHOOT_EFFECT_SNOWBALL	= 0x0C, //12
	SHOOT_EFFECT_POWERBOLT	= 0x0D, //13
	SHOOT_EFFECT_POISONFIELD	= 0x0E, //14
	SHOOT_EFFECT_INFERNALBOLT	= 0x0F, //15
	SHOOT_EFFECT_HUNTINGSPEAR	= 0x10, //16
	SHOOT_EFFECT_ENCHANTEDSPEAR	= 0x11, //17
	SHOOT_EFFECT_REDSTAR	= 0x12, //18
	SHOOT_EFFECT_GREENSTAR	= 0x13, //19
	SHOOT_EFFECT_ROYALSPEAR	= 0x14, //20
	SHOOT_EFFECT_SNIPERARROW	= 0x15, //21
	SHOOT_EFFECT_ONYXARROW	= 0x16, //22
	SHOOT_EFFECT_PIERCINGBOLT	= 0x17, //23
	SHOOT_EFFECT_WHIRLWINDSWORD	= 0x18, //24
	SHOOT_EFFECT_WHIRLWINDAXE	= 0x19, //25
	SHOOT_EFFECT_WHIRLWINDCLUB	= 0x1A, //26
	SHOOT_EFFECT_ETHEREALSPEAR	= 0x1B, //27
	SHOOT_EFFECT_ICE		= 0x1C, //28
	SHOOT_EFFECT_EARTH		= 0x1D, //29
	SHOOT_EFFECT_HOLY		= 0x1E, //30
	SHOOT_EFFECT_SUDDENDEATH	= 0x1F, //31
	SHOOT_EFFECT_FLASHARROW	= 0x20, //32
	SHOOT_EFFECT_FLAMMINGARROW	= 0x21, //33
	SHOOT_EFFECT_SHIVERARROW	= 0x22, //34
	SHOOT_EFFECT_ENERGYBALL	= 0x23, //35
	SHOOT_EFFECT_SMALLICE	= 0x24, //36
	SHOOT_EFFECT_SMALLHOLY	= 0x25, //37
	SHOOT_EFFECT_SMALLEARTH	= 0x26, //38
	SHOOT_EFFECT_EARTHARROW	= 0x27, //39
	SHOOT_EFFECT_EXPLOSION	= 0x28, //40
	SHOOT_EFFECT_CAKE		= 0x29, //41
	SHOOT_EFFECT_LAST		= SHOOT_EFFECT_CAKE,

	//for internal use, dont send to client
	SHOOT_EFFECT_WEAPONTYPE	= 0xFE, //254
	SHOOT_EFFECT_NONE		= 0xFF,
	SHOOT_EFFECT_UNKNOWN	= 0xFFFF
};

enum SpeakClasses
{
	SPEAK_CLASS_NONE	= 0x00,
	SPEAK_CLASS_FIRST 	= 0x01,
	SPEAK_SAY		= SPEAK_CLASS_FIRST,
	SPEAK_WHISPER		= 0x02,
	SPEAK_YELL		= 0x03,
	SPEAK_PRIVATE_PN	= 0x04,
	SPEAK_PRIVATE_NP	= 0x05,
	SPEAK_PRIVATE		= 0x06,
	SPEAK_CHANNEL_Y		= 0x07,
	SPEAK_CHANNEL_W		= 0x08,
	SPEAK_RVR_CHANNEL	= 0x09,
	SPEAK_RVR_ANSWER	= 0x0A,
	SPEAK_RVR_CONTINUE	= 0x0B,
	SPEAK_BROADCAST		= 0x0C,
	SPEAK_CHANNEL_RN	= 0x0D, //red - #c text
	SPEAK_PRIVATE_RED	= 0x0E,	//@name@text
	SPEAK_CHANNEL_O		= 0x0F,
	//SPEAK_UNKNOWN_1		= 0x10,
	SPEAK_CHANNEL_RA	= 0x11,	//red anonymous - #d text
	//SPEAK_UNKNOWN_2		= 0x12,
	SPEAK_MONSTER_SAY	= 0x13,
	SPEAK_MONSTER_YELL	= 0x14,
	SPEAK_CLASS_LAST 	= SPEAK_MONSTER_YELL
};

enum MessageClasses
{
	MSG_CLASS_FIRST			= 0x12,
	MSG_STATUS_CONSOLE_RED		= MSG_CLASS_FIRST, /*Red message in the console*/
	MSG_EVENT_ORANGE		= 0x13, /*Orange message in the console*/
	MSG_STATUS_CONSOLE_ORANGE	= 0x14, /*Orange message in the console*/
	MSG_STATUS_WARNING		= 0x15, /*Red message in game window and in the console*/
	MSG_EVENT_ADVANCE		= 0x16, /*White message in game window and in the console*/
	MSG_EVENT_DEFAULT		= 0x17, /*White message at the bottom of the game window and in the console*/
	MSG_STATUS_DEFAULT		= 0x18, /*White message at the bottom of the game window and in the console*/
	MSG_INFO_DESCR			= 0x19, /*Green message in game window and in the console*/
	MSG_STATUS_SMALL		= 0x1A, /*White message at the bottom of the game window"*/
	MSG_STATUS_CONSOLE_BLUE		= 0x1B, /*Blue message in the console*/
	MSG_CLASS_LAST			= MSG_STATUS_CONSOLE_BLUE
};

enum MapMarks_t
{
	MAPMARK_TICK		= 0x00,
	MAPMARK_QUESTION	= 0x01,
	MAPMARK_EXCLAMATION	= 0x02,
	MAPMARK_STAR		= 0x03,
	MAPMARK_CROSS		= 0x04,
	MAPMARK_TEMPLE		= 0x05,
	MAPMARK_KISS		= 0x06,
	MAPMARK_SHOVEL		= 0x07,
	MAPMARK_SWORD		= 0x08,
	MAPMARK_FLAG		= 0x09,
	MAPMARK_LOCK		= 0x0A,
	MAPMARK_BAG		= 0x0B,
	MAPMARK_SKULL		= 0x0C,
	MAPMARK_DOLLAR		= 0x0D,
	MAPMARK_REDNORTH	= 0x0E,
	MAPMARK_REDSOUTH	= 0x0F,
	MAPMARK_REDEAST		= 0x10,
	MAPMARK_REDWEST		= 0x11,
	MAPMARK_GREENNORTH	= 0x12,
	MAPMARK_GREENSOUTH	= 0x13
};

enum FluidColors_t
{
	FLUID_EMPTY	= 0x00,
	FLUID_BLUE	= 0x01,
	FLUID_RED	= 0x02,
	FLUID_BROWN	= 0x03,
	FLUID_GREEN	= 0x04,
	FLUID_YELLOW	= 0x05,
	FLUID_WHITE	= 0x06,
	FLUID_PURPLE	= 0x07
};

enum FluidTypes_t
{
	FLUID_NONE		= FLUID_EMPTY,
	FLUID_WATER		= FLUID_BLUE,
	FLUID_BLOOD		= FLUID_RED,
	FLUID_BEER		= FLUID_BROWN,
	FLUID_SLIME		= FLUID_GREEN,
	FLUID_LEMONADE		= FLUID_YELLOW,
	FLUID_MILK		= FLUID_WHITE,
	FLUID_MANA		= FLUID_PURPLE,

	FLUID_LIFE		= FLUID_RED + 8,
	FLUID_OIL		= FLUID_BROWN + 8,
	FLUID_URINE		= FLUID_YELLOW + 8,
	FLUID_COCONUTMILK	= FLUID_WHITE + 8,
	FLUID_WINE		= FLUID_PURPLE + 8,

	FLUID_MUD		= FLUID_BROWN + 16,
	FLUID_FRUITJUICE	= FLUID_YELLOW + 16,

	FLUID_LAVA		= FLUID_RED + 24,
	FLUID_RUM		= FLUID_BROWN + 24,
	FLUID_SWAMP		= FLUID_GREEN + 24,
};

const uint8_t reverseFluidMap[] =
{
	FLUID_EMPTY,
	FLUID_WATER,
	FLUID_MANA,
	FLUID_BEER,
	FLUID_EMPTY,
	FLUID_BLOOD,
	FLUID_SLIME,
	FLUID_EMPTY,
	FLUID_LEMONADE,
	FLUID_MILK
};

enum ClientFluidTypes_t
{
	CLIENTFLUID_EMPTY	= 0x00,
	CLIENTFLUID_BLUE	= 0x01,
	CLIENTFLUID_PURPLE	= 0x02,
	CLIENTFLUID_BROWN_1	= 0x03,
	CLIENTFLUID_BROWN_2	= 0x04,
	CLIENTFLUID_RED		= 0x05,
	CLIENTFLUID_GREEN	= 0x06,
	CLIENTFLUID_BROWN	= 0x07,
	CLIENTFLUID_YELLOW	= 0x08,
	CLIENTFLUID_WHITE	= 0x09
};

const uint8_t fluidMap[] =
{
	CLIENTFLUID_EMPTY,
	CLIENTFLUID_BLUE,
	CLIENTFLUID_RED,
	CLIENTFLUID_BROWN_1,
	CLIENTFLUID_GREEN,
	CLIENTFLUID_YELLOW,
	CLIENTFLUID_WHITE,
	CLIENTFLUID_PURPLE
};

enum SquareColor_t
{
	SQ_COLOR_NONE = 256,
	SQ_COLOR_BLACK = 0,
};

enum TextColor_t
{
	TEXTCOLOR_BLUE		= 5,
	TEXTCOLOR_GREEN		= 18,
	TEXTCOLOR_TEAL		= 35,
	TEXTCOLOR_LIGHTGREEN	= 66,
	TEXTCOLOR_DARKBROWN	= 78,
	TEXTCOLOR_LIGHTBLUE	= 89,
	TEXTCOLOR_DARKPURPLE	= 112,
	TEXTCOLOR_BROWN		= 120,
	TEXTCOLOR_GREY		= 129,
	TEXTCOLOR_DARKRED	= 144,
	TEXTCOLOR_DARKPINK	= 152,
	TEXTCOLOR_PURPLE	= 154,
	TEXTCOLOR_DARKORANGE	= 156,
	TEXTCOLOR_RED		= 180,
	TEXTCOLOR_PINK		= 190,
	TEXTCOLOR_ORANGE	= 192,
	TEXTCOLOR_DARKYELLOW	= 205,
	TEXTCOLOR_YELLOW	= 210,
	TEXTCOLOR_WHITE		= 215,

	TEXTCOLOR_NONE		= 255,
	TEXTCOLOR_UNKNOWN	= 256
};

enum Icons_t
{
	ICON_NONE = 0,
	ICON_POISON = 1 << 0,
	ICON_BURN = 1 << 1,
	ICON_ENERGY = 1 << 2,
	ICON_DRUNK = 1 << 3,
	ICON_MANASHIELD = 1 << 4,
	ICON_PARALYZE = 1 << 5,
	ICON_HASTE = 1 << 6,
	ICON_SWORDS = 1 << 7,
	ICON_DROWNING = 1 << 8,
	ICON_FREEZING = 1 << 9,
	ICON_DAZZLED = 1 << 10,
	ICON_CURSED = 1 << 11,
	ICON_BUFF = 1 << 12,
	ICON_PZ = 1 << 13,
	ICON_PROTECTIONZONE = 1 << 14
};

enum WeaponType_t
{
	WEAPON_NONE = 0,
	WEAPON_SWORD = 1,
	WEAPON_CLUB = 2,
	WEAPON_AXE = 3,
	WEAPON_SHIELD = 4,
	WEAPON_DIST = 5,
	WEAPON_WAND = 6,
	WEAPON_AMMO = 7,
	WEAPON_FIST = 8
};

enum Ammo_t
{
	AMMO_NONE = 0,
	AMMO_BOLT = 1,
	AMMO_ARROW = 2,
	AMMO_SPEAR = 3,
	AMMO_THROWINGSTAR = 4,
	AMMO_THROWINGKNIFE = 5,
	AMMO_STONE = 6,
	AMMO_SNOWBALL = 7
};

enum AmmoAction_t
{
	AMMOACTION_NONE,
	AMMOACTION_REMOVECOUNT,
	AMMOACTION_REMOVECHARGE,
	AMMOACTION_MOVE,
	AMMOACTION_MOVEBACK
};

enum WieldInfo_t
{
	WIELDINFO_LEVEL = 1,
	WIELDINFO_MAGLV = 2,
	WIELDINFO_VOCREQ = 4,
	WIELDINFO_PREMIUM = 8
};

enum Skulls_t
{
	SKULL_NONE = 0,
	SKULL_YELLOW = 1,
	SKULL_GREEN = 2,
	SKULL_WHITE = 3,
	SKULL_RED = 4,
	SKULL_BLACK = 5,
	SKULL_LAST = SKULL_BLACK
};

enum PartyShields_t
{
	SHIELD_NONE = 0,
	SHIELD_WHITEYELLOW = 1,
	SHIELD_WHITEBLUE = 2,
	SHIELD_BLUE = 3,
	SHIELD_YELLOW = 4,
	SHIELD_BLUE_SHAREDEXP = 5,
	SHIELD_YELLOW_SHAREDEXP = 6,
	SHIELD_BLUE_NOSHAREDEXP_BLINK = 7,
	SHIELD_YELLOW_NOSHAREDEXP_BLINK = 8,
	SHIELD_BLUE_NOSHAREDEXP = 9,
	SHIELD_YELLOW_NOSHAREDEXP = 10,
	SHIELD_LAST = SHIELD_YELLOW_NOSHAREDEXP
};

enum item_t
{
	ITEM_FIREFIELD		= 1492,
	ITEM_FIREFIELD_SAFE	= 1500,

	ITEM_POISONFIELD	= 1496,
	ITEM_POISONFIELD_SAFE	= 1503,

	ITEM_ENERGYFIELD	= 1495,
	ITEM_ENERGYFIELD_SAFE	= 1504,

	ITEM_MAGICWALL		= 1497,
	ITEM_MAGICWALL_SAFE	= 11095,

	ITEM_WILDGROWTH		= 1499,
	ITEM_WILDGROWTH_SAFE	= 11096,

	ITEM_DEPOT		= 2594,
	ITEM_LOCKER		= 2589,
	ITEM_GLOWING_SWITCH	= 11060,

	ITEM_MALE_CORPSE	= 6080,
	ITEM_FEMALE_CORPSE	= 6081,

	ITEM_MEAT		= 2666,
	ITEM_HAM		= 2671,
	ITEM_GRAPE		= 2681,
	ITEM_APPLE		= 2674,
	ITEM_BREAD		= 2689,
	ITEM_ROLL		= 2690,
	ITEM_CHEESE		= 2696,

	ITEM_FULLSPLASH		= 2016,
	ITEM_SMALLSPLASH	= 2019,

	ITEM_PARCEL		= 2595,
	ITEM_PARCEL_STAMPED	= 2596,
	ITEM_LETTER		= 2597,
	ITEM_LETTER_STAMPED	= 2598,
	ITEM_LABEL		= 2599,

	ITEM_WATERBALL_SPLASH	= 7711,
	ITEM_WATERBALL		= 7956,

	ITEM_HOUSE_TRANSFER	= 1968 //read-only
};

enum PlayerFlags
{
	PlayerFlag_CannotUseCombat = 0,			//2^0 = 1
	PlayerFlag_CannotAttackPlayer,			//2^1 = 2
	PlayerFlag_CannotAttackMonster,			//2^2 = 4
	PlayerFlag_CannotBeAttacked,			//2^3 = 8
	PlayerFlag_CanConvinceAll,			//2^4 = 16
	PlayerFlag_CanSummonAll,			//2^5 = 32
	PlayerFlag_CanIllusionAll,			//2^6 = 64
	PlayerFlag_CanSenseInvisibility,		//2^7 = 128
	PlayerFlag_IgnoredByMonsters,			//2^8 = 256
	PlayerFlag_NotGainInFight,			//2^9 = 512
	PlayerFlag_HasInfiniteMana,			//2^10 = 1024
	PlayerFlag_HasInfiniteSoul,			//2^11 = 2048
	PlayerFlag_HasNoExhaustion,			//2^12 = 4096
	PlayerFlag_CannotUseSpells,			//2^13 = 8192
	PlayerFlag_CannotPickupItem,			//2^14 = 16384
	PlayerFlag_CanAlwaysLogin,			//2^15 = 32768
	PlayerFlag_CanBroadcast,			//2^16 = 65536
	PlayerFlag_CanEditHouses,			//2^17 = 131072
	PlayerFlag_CannotBeBanned,			//2^18 = 262144
	PlayerFlag_CannotBePushed,			//2^19 = 524288
	PlayerFlag_HasInfiniteCapacity,			//2^20 = 1048576
	PlayerFlag_CanPushAllCreatures,			//2^21 = 2097152
	PlayerFlag_CanTalkRedPrivate,			//2^22 = 4194304
	PlayerFlag_CanTalkRedChannel,			//2^23 = 8388608
	PlayerFlag_TalkOrangeHelpChannel,		//2^24 = 16777216
	PlayerFlag_NotGainExperience,			//2^25 = 33554432
	PlayerFlag_NotGainMana,				//2^26 = 67108864
	PlayerFlag_NotGainHealth,			//2^27 = 134217728
	PlayerFlag_NotGainSkill,			//2^28 = 268435456
	PlayerFlag_SetMaxSpeed,				//2^29 = 536870912
	PlayerFlag_SpecialVIP,				//2^30 = 1073741824
	PlayerFlag_NotGenerateLoot,			//2^31 = 2147483648
	PlayerFlag_CanTalkRedChannelAnonymous,		//2^32 = 4294967296
	PlayerFlag_IgnoreProtectionZone,		//2^33 = 8589934592
	PlayerFlag_IgnoreSpellCheck,			//2^34 = 17179869184
	PlayerFlag_IgnoreEquipCheck,			//2^35 = 34359738368
	PlayerFlag_CannotBeMuted,			//2^36 = 68719476736
	PlayerFlag_IsAlwaysPremium,			//2^37 = 137438953472
	PlayerFlag_CanAnswerRuleViolations,		//2^38 = 274877906944
	PlayerFlag_39,	//ignore			//2^39 = 549755813888	//not used by us
	PlayerFlag_ShowGroupNameInsteadOfVocation,	//2^40 = 1099511627776
	PlayerFlag_HasInfiniteStamina,			//2^41 = 2199023255552
	PlayerFlag_CannotMoveItems,			//2^42 = 4398046511104
	PlayerFlag_CannotMoveCreatures,			//2^43 = 8796093022208
	PlayerFlag_CanReportBugs,			//2^44 = 17592186044416
	PlayerFlag_45,	//ignore			//2^45 = 35184372088832	//not used by us
	PlayerFlag_CannotBeSeen,			//2^46 = 70368744177664

	PlayerFlag_LastFlag
};

enum PlayerCustomFlags
{
	PlayerCustomFlag_AllowIdle = 0,				//2^0 = 1
	PlayerCustomFlag_CanSeePosition,			//2^1 = 2
	PlayerCustomFlag_CanSeeItemDetails,			//2^2 = 4
	PlayerCustomFlag_CanSeeCreatureDetails,			//2^3 = 8
	PlayerCustomFlag_NotSearchable,				//2^4 = 16
	PlayerCustomFlag_GamemasterPrivileges,			//2^5 = 32
	PlayerCustomFlag_CanThrowAnywhere,			//2^6 = 64
	PlayerCustomFlag_CanPushAllItems,			//2^7 = 128
	PlayerCustomFlag_CanMoveAnywhere,			//2^8 = 256
	PlayerCustomFlag_CanMoveFromFar,			//2^9 = 512
	PlayerCustomFlag_CanLoginMultipleCharacters,		//2^10 = 1024 (account flag)
	PlayerCustomFlag_HasFullLight,				//2^11 = 2048
	PlayerCustomFlag_CanLogoutAnytime,			//2^12 = 4096 (account flag)
	PlayerCustomFlag_HideLevel,				//2^13 = 8192
	PlayerCustomFlag_IsProtected,				//2^14 = 16384
	PlayerCustomFlag_IsImmune,				//2^15 = 32768
	PlayerCustomFlag_NotGainSkull,				//2^16 = 65536
	PlayerCustomFlag_NotGainUnjustified,			//2^17 = 131072
	PlayerCustomFlag_IgnorePacification,			//2^18 = 262144
	PlayerCustomFlag_IgnoreLoginDelay,			//2^19 = 524288
	PlayerCustomFlag_CanStairhop,				//2^20 = 1048576
	PlayerCustomFlag_CanTurnhop,				//2^21 = 2097152
	PlayerCustomFlag_IgnoreHouseRent,			//2^22 = 4194304
	PlayerCustomFlag_CanWearAllAddons,			//2^23 = 8388608

	PlayerCustomFlag_LastFlag
};


enum slots_t
{
	SLOT_PRE_FIRST = 0,
	SLOT_WHEREEVER = SLOT_PRE_FIRST,
	SLOT_FIRST = 1,
	SLOT_HEAD = SLOT_FIRST,
	SLOT_NECKLACE = 2,
	SLOT_BACKPACK = 3,
	SLOT_ARMOR = 4,
	SLOT_RIGHT = 5,
	SLOT_LEFT = 6,
	SLOT_LEGS = 7,
	SLOT_FEET = 8,
	SLOT_RING = 9,
	SLOT_AMMO = 10,
	SLOT_DEPOT = 11,
	SLOT_LAST = SLOT_DEPOT,
	SLOT_HAND = 12,
	SLOT_TWO_HAND = SLOT_HAND
};


enum lootDrop_t
{
	LOOT_DROP_FULL = 0,
	LOOT_DROP_PREVENT,
	LOOT_DROP_NONE
};


enum killflags_t
{
	KILLFLAG_NONE = 0,
	KILLFLAG_LASTHIT = 1 << 0,
	KILLFLAG_JUSTIFY = 1 << 1,
	KILLFLAG_UNJUSTIFIED = 1 << 2
};


enum Visible_t
{
	VISIBLE_NONE = 0,
	VISIBLE_APPEAR = 1,
	VISIBLE_DISAPPEAR = 2,
	VISIBLE_GHOST_APPEAR = 3,
	VISIBLE_GHOST_DISAPPEAR = 4
};


enum Direction
{
	NORTH = 0,
	EAST = 1,
	SOUTH = 2,
	WEST = 3,
	SOUTHWEST = 4,
	SOUTHEAST = 5,
	NORTHWEST = 6,
	NORTHEAST = 7
};


enum ConditionType_t
{
	CONDITION_NONE = 0,
	CONDITION_POISON = 1 << 0,
	CONDITION_FIRE = 1 << 1,
	CONDITION_ENERGY = 1 << 2,
	CONDITION_PHYSICAL = 1 << 3,
	CONDITION_HASTE = 1 << 4,
	CONDITION_PARALYZE = 1 << 5,
	CONDITION_OUTFIT = 1 << 6,
	CONDITION_INVISIBLE = 1 << 7,
	CONDITION_LIGHT = 1 << 8,
	CONDITION_MANASHIELD = 1 << 9,
	CONDITION_INFIGHT = 1 << 10,
	CONDITION_DRUNK = 1 << 11,
	CONDITION_EXHAUST = 1 << 12,
	CONDITION_REGENERATION = 1 << 13,
	CONDITION_SOUL = 1 << 14,
	CONDITION_DROWN = 1 << 15,
	CONDITION_MUTED = 1 << 16,
	CONDITION_ATTRIBUTES = 1 << 17,
	CONDITION_FREEZING = 1 << 18,
	CONDITION_DAZZLED = 1 << 19,
	CONDITION_CURSED = 1 << 20,
	CONDITION_PACIFIED = 1 << 21,
	CONDITION_GAMEMASTER = 1 << 22,
	CONDITION_HUNTING = 1 << 23
};


enum ConditionEnd_t
{
	CONDITIONEND_CLEANUP,
	CONDITIONEND_DEATH,
	CONDITIONEND_TICKS,
	CONDITIONEND_ABORT
};


enum ConditionAttr_t
{
	CONDITIONATTR_TYPE = 1,
	CONDITIONATTR_ID = 2,
	CONDITIONATTR_TICKS = 3,
	CONDITIONATTR_HEALTHTICKS = 4,
	CONDITIONATTR_HEALTHGAIN = 5,
	CONDITIONATTR_MANATICKS = 6,
	CONDITIONATTR_MANAGAIN = 7,
	CONDITIONATTR_DELAYED = 8,
	CONDITIONATTR_OWNER = 9,
	CONDITIONATTR_INTERVALDATA = 10,
	CONDITIONATTR_SPEEDDELTA = 11,
	CONDITIONATTR_FORMULA_MINA = 12,
	CONDITIONATTR_FORMULA_MINB = 13,
	CONDITIONATTR_FORMULA_MAXA = 14,
	CONDITIONATTR_FORMULA_MAXB = 15,
	CONDITIONATTR_LIGHTCOLOR = 16,
	CONDITIONATTR_LIGHTLEVEL = 17,
	CONDITIONATTR_LIGHTTICKS = 18,
	CONDITIONATTR_LIGHTINTERVAL = 19,
	CONDITIONATTR_SOULTICKS = 20,
	CONDITIONATTR_SOULGAIN = 21,
	CONDITIONATTR_SKILLS = 22,
	CONDITIONATTR_STATS = 23,
	CONDITIONATTR_OUTFIT = 24,
	CONDITIONATTR_PERIODDAMAGE = 25,
	CONDITIONATTR_BUFF = 26,
	CONDITIONATTR_SUBID = 27,

	//reserved for serialization
	CONDITIONATTR_END = 254
};



enum Encryption_t
{
	ENCRYPTION_PLAIN = 0,
	ENCRYPTION_MD5,
	ENCRYPTION_SHA1
};

enum GuildLevel_t
{
	GUILDLEVEL_NONE = 0,
	GUILDLEVEL_MEMBER,
	GUILDLEVEL_VICE,
	GUILDLEVEL_LEADER
};

enum OperatingSystem_t
{
	CLIENTOS_LINUX = 0x01,
	CLIENTOS_WINDOWS = 0x02
};

enum Channels_t
{
	CHANNEL_GUILD = 0x00,
	CHANNEL_PARTY = 0x01,
	CHANNEL_RVR = 0x03,
	CHANNEL_HELP = 0x09,
	CHANNEL_DEFAULT = 0xFFFE, //internal usage only, there is no such channel
	CHANNEL_PRIVATE = 0xFFFF
};

enum ViolationAction_t
{
	ACTION_NOTATION = 0,
	ACTION_NAMEREPORT,
	ACTION_BANISHMENT,
	ACTION_BANREPORT,
	ACTION_BANFINAL,
	ACTION_BANREPORTFINAL,
	ACTION_STATEMENT,
	//internal use
	ACTION_DELETION,
	ACTION_NAMELOCK,
	ACTION_BANLOCK,
	ACTION_BANLOCKFINAL,
	ACTION_PLACEHOLDER
};

enum RaceType_t
{
	RACE_NONE = 0,
	RACE_VENOM,
	RACE_BLOOD,
	RACE_UNDEAD,
	RACE_FIRE,
	RACE_ENERGY
};

enum CombatType_t
{
	COMBAT_FIRST		= 0,
	COMBAT_NONE		= COMBAT_FIRST,
	COMBAT_PHYSICALDAMAGE	= 1 << 0,
	COMBAT_ENERGYDAMAGE	= 1 << 1,
	COMBAT_EARTHDAMAGE	= 1 << 2,
	COMBAT_FIREDAMAGE	= 1 << 3,
	COMBAT_UNDEFINEDDAMAGE	= 1 << 4,
	COMBAT_LIFEDRAIN	= 1 << 5,
	COMBAT_MANADRAIN	= 1 << 6,
	COMBAT_HEALING		= 1 << 7,
	COMBAT_DROWNDAMAGE	= 1 << 8,
	COMBAT_ICEDAMAGE	= 1 << 9,
	COMBAT_HOLYDAMAGE	= 1 << 10,
	COMBAT_DEATHDAMAGE	= 1 << 11,
	COMBAT_LAST		= COMBAT_DEATHDAMAGE
};

// TODO temporary until item refactoring is done
enum CombatTypeIndex {
	COMBATINDEX_FIRST,
	COMBATINDEX_NONE = COMBATINDEX_FIRST,
	COMBATINDEX_PHYSICALDAMAGE,
	COMBATINDEX_ENERGYDAMAGE,
	COMBATINDEX_EARTHDAMAGE,
	COMBATINDEX_FIREDAMAGE,
	COMBATINDEX_UNDEFINEDDAMAGE,
	COMBATINDEX_LIFEDRAIN,
	COMBATINDEX_MANADRAIN,
	COMBATINDEX_HEALING,
	COMBATINDEX_DROWNDAMAGE,
	COMBATINDEX_ICEDAMAGE,
	COMBATINDEX_HOLYDAMAGE,
	COMBATINDEX_DEATHDAMAGE,
	COMBATINDEX_LAST = COMBATINDEX_DEATHDAMAGE,
	COMBATINDEX_COUNT,
};

enum CombatParam_t
{
	COMBATPARAM_NONE = 0,
	COMBATPARAM_COMBATTYPE,
	COMBATPARAM_EFFECT,
	COMBATPARAM_DISTANCEEFFECT,
	COMBATPARAM_BLOCKEDBYSHIELD,
	COMBATPARAM_BLOCKEDBYARMOR,
	COMBATPARAM_TARGETCASTERORTOPMOST,
	COMBATPARAM_CREATEITEM,
	COMBATPARAM_AGGRESSIVE,
	COMBATPARAM_DISPEL,
	COMBATPARAM_USECHARGES,
	COMBATPARAM_TARGETPLAYERSORSUMMONS,
	COMBATPARAM_DIFFERENTAREADAMAGE,
	COMBATPARAM_HITEFFECT,
	COMBATPARAM_HITCOLOR
};

enum CallBackParam_t
{
	CALLBACKPARAM_NONE = 0,
	CALLBACKPARAM_LEVELMAGICVALUE,
	CALLBACKPARAM_SKILLVALUE,
	CALLBACKPARAM_TARGETTILECALLBACK,
	CALLBACKPARAM_TARGETCREATURECALLBACK
};

enum ConditionParam_t
{
	CONDITIONPARAM_OWNER = 1,
	CONDITIONPARAM_TICKS = 2,
	CONDITIONPARAM_OUTFIT = 3,
	CONDITIONPARAM_HEALTHGAIN = 4,
	CONDITIONPARAM_HEALTHTICKS = 5,
	CONDITIONPARAM_MANAGAIN = 6,
	CONDITIONPARAM_MANATICKS = 7,
	CONDITIONPARAM_DELAYED = 8,
	CONDITIONPARAM_SPEED = 9,
	CONDITIONPARAM_LIGHT_LEVEL = 10,
	CONDITIONPARAM_LIGHT_COLOR = 11,
	CONDITIONPARAM_SOULGAIN = 12,
	CONDITIONPARAM_SOULTICKS = 13,
	CONDITIONPARAM_MINVALUE = 14,
	CONDITIONPARAM_MAXVALUE = 15,
	CONDITIONPARAM_STARTVALUE = 16,
	CONDITIONPARAM_TICKINTERVAL = 17,
	CONDITIONPARAM_FORCEUPDATE = 18,
	CONDITIONPARAM_SKILL_MELEE = 19,
	CONDITIONPARAM_SKILL_FIST = 20,
	CONDITIONPARAM_SKILL_CLUB = 21,
	CONDITIONPARAM_SKILL_SWORD = 22,
	CONDITIONPARAM_SKILL_AXE = 23,
	CONDITIONPARAM_SKILL_DISTANCE = 24,
	CONDITIONPARAM_SKILL_SHIELD = 25,
	CONDITIONPARAM_SKILL_FISHING = 26,
	CONDITIONPARAM_STAT_MAXHEALTH = 27,
	CONDITIONPARAM_STAT_MAXMANA = 28,
	CONDITIONPARAM_STAT_SOUL = 29,
	CONDITIONPARAM_STAT_MAGICLEVEL = 30,
	CONDITIONPARAM_STAT_MAXHEALTHPERCENT = 31,
	CONDITIONPARAM_STAT_MAXMANAPERCENT = 32,
	CONDITIONPARAM_STAT_SOULPERCENT = 33,
	CONDITIONPARAM_STAT_MAGICLEVELPERCENT = 34,
	CONDITIONPARAM_SKILL_MELEEPERCENT = 35,
	CONDITIONPARAM_SKILL_FISTPERCENT = 36,
	CONDITIONPARAM_SKILL_CLUBPERCENT = 37,
	CONDITIONPARAM_SKILL_SWORDPERCENT = 38,
	CONDITIONPARAM_SKILL_AXEPERCENT = 39,
	CONDITIONPARAM_SKILL_DISTANCEPERCENT = 40,
	CONDITIONPARAM_SKILL_SHIELDPERCENT = 41,
	CONDITIONPARAM_SKILL_FISHINGPERCENT = 42,
	CONDITIONPARAM_PERIODICDAMAGE = 43,
	CONDITIONPARAM_BUFF = 44,
	CONDITIONPARAM_SUBID = 45
};

enum BlockType_t
{
	BLOCK_NONE = 0,
	BLOCK_DEFENSE,
	BLOCK_ARMOR,
	BLOCK_IMMUNITY
};

enum Reflect_t
{
	REFLECT_FIRST = 0,
	REFLECT_PERCENT = REFLECT_FIRST,
	REFLECT_CHANCE,
	REFLECT_LAST = REFLECT_CHANCE,
	REFLECT_COUNT
};

enum Increment_t
{
	INCREMENT_FIRST = 0,
	HEALING_VALUE = INCREMENT_FIRST,
	HEALING_PERCENT,
	MAGIC_VALUE,
	MAGIC_PERCENT,
	INCREMENT_LAST = MAGIC_PERCENT
};

enum skills_t
{
	SKILL_FIRST = 0,
	SKILL_FIST = SKILL_FIRST,
	SKILL_CLUB,
	SKILL_SWORD,
	SKILL_AXE,
	SKILL_DIST,
	SKILL_SHIELD,
	SKILL_FISH,
	SKILL__MAGLEVEL,
	SKILL__LEVEL,
	SKILL_LAST = SKILL_FISH,
	SKILL__LAST = SKILL__LEVEL
};

enum stats_t
{
	STAT_FIRST = 0,
	STAT_MAXHEALTH = STAT_FIRST,
	STAT_MAXMANA,
	STAT_SOUL,
	STAT_LEVEL,
	STAT_MAGICLEVEL,
	STAT_LAST = STAT_MAGICLEVEL
};

enum lossTypes_t
{
	LOSS_FIRST = 0,
	LOSS_EXPERIENCE = LOSS_FIRST,
	LOSS_MANA,
	LOSS_SKILLS,
	LOSS_CONTAINERS,
	LOSS_ITEMS,
	LOSS_LAST = LOSS_ITEMS
};

enum formulaType_t
{
	FORMULA_UNDEFINED = 0,
	FORMULA_LEVELMAGIC,
	FORMULA_SKILL,
	FORMULA_VALUE
};

enum ConditionId_t
{
	CONDITIONID_DEFAULT = -1,
	CONDITIONID_COMBAT = 0,
	CONDITIONID_HEAD,
	CONDITIONID_NECKLACE,
	CONDITIONID_BACKPACK,
	CONDITIONID_ARMOR,
	CONDITIONID_RIGHT,
	CONDITIONID_LEFT,
	CONDITIONID_LEGS,
	CONDITIONID_FEET,
	CONDITIONID_RING,
	CONDITIONID_AMMO,
	CONDITIONID_OUTFIT
};

enum PlayerSex_t
{
	PLAYERSEX_FEMALE = 0,
	PLAYERSEX_MALE
	// DO NOT ADD HERE! Every higher sex is only for your
	// own use- each female should be even and male odd.
};


typedef uint8_t attribute_t;
typedef uint16_t datasize_t;
typedef uint32_t flags_t;

enum itemgroup_t
{
	ITEM_GROUP_NONE = 0,
	ITEM_GROUP_GROUND,
	ITEM_GROUP_CONTAINER,
	ITEM_GROUP_WEAPON, //deprecated
	ITEM_GROUP_AMMUNITION, //deprecated
	ITEM_GROUP_ARMOR, //deprecated
	ITEM_GROUP_CHARGES,
	ITEM_GROUP_TELEPORT, //deprecated
	ITEM_GROUP_MAGICFIELD, //deprecated
	ITEM_GROUP_WRITEABLE, //deprecated
	ITEM_GROUP_KEY, //deprecated
	ITEM_GROUP_SPLASH,
	ITEM_GROUP_FLUID,
	ITEM_GROUP_DOOR, //deprecated
	ITEM_GROUP_DEPRECATED,
	ITEM_GROUP_LAST
};

enum clientVersion_t
{
	CLIENT_VERSION_750 = 1,
	CLIENT_VERSION_755 = 2,
	CLIENT_VERSION_760 = 3,
	CLIENT_VERSION_770 = 3,
	CLIENT_VERSION_780 = 4,
	CLIENT_VERSION_790 = 5,
	CLIENT_VERSION_792 = 6,
	CLIENT_VERSION_800 = 7,
	CLIENT_VERSION_810 = 8,
	CLIENT_VERSION_811 = 9,
	CLIENT_VERSION_820 = 10,
	CLIENT_VERSION_830 = 11,
	CLIENT_VERSION_840 = 12,
	CLIENT_VERSION_841 = 13,
	CLIENT_VERSION_842 = 14,
	CLIENT_VERSION_850 = 15,
	CLIENT_VERSION_854 = 16,
	CLIENT_VERSION_855 = 17,
	CLIENT_VERSION_856 = 18,
	CLIENT_VERSION_857 = 19,
	CLIENT_VERSION_860 = 20
};

enum rootattrib_
{
	ROOT_ATTR_VERSION = 0x01
};

enum itemattrib_t
{
	ITEM_ATTR_FIRST = 0x10,
	ITEM_ATTR_SERVERID = ITEM_ATTR_FIRST,
	ITEM_ATTR_CLIENTID,
	ITEM_ATTR_NAME,
	ITEM_ATTR_DESCR,
	ITEM_ATTR_SPEED,
	ITEM_ATTR_SLOT,
	ITEM_ATTR_MAXITEMS,
	ITEM_ATTR_WEIGHT,
	ITEM_ATTR_WEAPON,
	ITEM_ATTR_AMU,
	ITEM_ATTR_ARMOR,
	ITEM_ATTR_MAGLEVEL,
	ITEM_ATTR_MAGFIELDTYPE,
	ITEM_ATTR_WRITEABLE,
	ITEM_ATTR_ROTATETO,
	ITEM_ATTR_DECAY,
	ITEM_ATTR_SPRITEHASH,
	ITEM_ATTR_MINIMAPCOLOR,
	ITEM_ATTR_07,
	ITEM_ATTR_08,
	ITEM_ATTR_LIGHT,

	//1-byte aligned
	ITEM_ATTR_DECAY2,
	ITEM_ATTR_WEAPON2,
	ITEM_ATTR_AMU2,
	ITEM_ATTR_ARMOR2,
	ITEM_ATTR_WRITEABLE2,
	ITEM_ATTR_LIGHT2,

	ITEM_ATTR_TOPORDER,
	ITEM_ATTR_WRITEABLE3,
	ITEM_ATTR_LAST
};

enum itemflags_t
{
	FLAG_BLOCK_SOLID = 1 << 0,
	FLAG_BLOCK_PROJECTILE = 1 << 1,
	FLAG_BLOCK_PATHFIND = 1 << 2,
	FLAG_HAS_HEIGHT = 1 << 3,
	FLAG_USEABLE = 1 << 4,
	FLAG_PICKUPABLE = 1 << 5,
	FLAG_MOVEABLE = 1 << 6,
	FLAG_STACKABLE = 1 << 7,
	FLAG_FLOORCHANGEDOWN = 1 << 8,
	FLAG_FLOORCHANGENORTH = 1 << 9,
	FLAG_FLOORCHANGEEAST = 1 << 10,
	FLAG_FLOORCHANGESOUTH = 1 << 11,
	FLAG_FLOORCHANGEWEST = 1 << 12,
	FLAG_ALWAYSONTOP = 1 << 13,
	FLAG_READABLE = 1 << 14,
	FLAG_ROTABLE = 1 << 15,
	FLAG_HANGABLE = 1 << 16,
	FLAG_VERTICAL = 1 << 17,
	FLAG_HORIZONTAL = 1 << 18,
	FLAG_CANNOTDECAY = 1 << 19,
	FLAG_ALLOWDISTREAD = 1 << 20,
	FLAG_UNUSED = 1 << 21,
	FLAG_CLIENTCHARGES = 1 << 22,
	FLAG_LOOKTHROUGH = 1 << 23
};

#pragma pack(1)
struct VERSIONINFO
{
	uint32_t dwMajorVersion;
	uint32_t dwMinorVersion;
	uint32_t dwBuildNumber;
	uint8_t CSDVersion[128];
};

struct lightBlock2
{
	uint16_t lightLevel;
	uint16_t lightColor;
};
#pragma pack()




enum class ItemType : uint8_t {
	COMMON = 0,
	DEPOT,
	MAILBOX,
	TRASHHOLDER,
	CONTAINER,
	DOOR,
	MAGICFIELD,
	TELEPORT,
	BED,
	KEY,
	LAST = KEY
};

enum FloorChange_t
{
	CHANGE_PRE_FIRST = 0,
	CHANGE_DOWN = CHANGE_PRE_FIRST,
	CHANGE_FIRST = 1,
	CHANGE_NORTH = CHANGE_FIRST,
	CHANGE_EAST = 2,
	CHANGE_SOUTH = 3,
	CHANGE_WEST = 4,
	CHANGE_FIRST_EX = 5,
	CHANGE_NORTH_EX = CHANGE_FIRST_EX,
	CHANGE_EAST_EX = 6,
	CHANGE_SOUTH_EX = 7,
	CHANGE_WEST_EX = 8,
	CHANGE_NONE = 9,
	CHANGE_LAST = CHANGE_NONE
};


struct Outfit_t
{
	Outfit_t() {lookHead = lookBody = lookLegs = lookFeet = lookType = lookTypeEx = lookAddons = 0;}
	uint16_t lookType, lookTypeEx;
	uint8_t lookHead, lookBody, lookLegs, lookFeet, lookAddons;

	bool operator==(const Outfit_t o) const
	{
		return (o.lookAddons == lookAddons
			&& o.lookType == lookType && o.lookTypeEx == lookTypeEx
			&& o.lookHead == lookHead && o.lookBody == lookBody
			&& o.lookLegs == lookLegs && o.lookFeet == lookFeet);
	}

	bool operator!=(const Outfit_t o) const
	{
		return !(*this == o);
	}
};

struct LightInfo
{
	uint32_t level, color;

	LightInfo() {level = color = 0;}
	LightInfo(uint32_t _level, uint32_t _color):
		level(_level), color(_color) {}
};

struct ShopInfo
{
	uint32_t itemId;
	int32_t subType, buyPrice, sellPrice;
	std::string itemName;

	ShopInfo()
	{
		itemId = 0;
		subType = 1;
		buyPrice = sellPrice = -1;
		itemName = "";
	}
	ShopInfo(uint32_t _itemId, int32_t _subType = 1, int32_t _buyPrice = -1, int32_t _sellPrice = -1,
		const std::string& _itemName = ""): itemId(_itemId), subType(_subType), buyPrice(_buyPrice),
		sellPrice(_sellPrice), itemName(_itemName) {}
};

typedef std::list<ShopInfo> ShopInfoList;


//Reserved player storage key ranges
//[10000000 - 20000000]
#define PSTRG_RESERVED_RANGE_START	10000000
#define PSTRG_RESERVED_RANGE_SIZE	10000000

//[1000 - 1500]
#define PSTRG_OUTFITS_RANGE_START	(PSTRG_RESERVED_RANGE_START + 1000)
#define PSTRG_OUTFITS_RANGE_SIZE	500

//[1500 - 2000]
#define PSTRG_OUTFITSID_RANGE_START	(PSTRG_RESERVED_RANGE_START + 1500)
#define PSTRG_OUTFITSID_RANGE_SIZE	500

#define NETWORKMESSAGE_MAXSIZE 15360
#define IPBAN_FLAG 128
#define LOCALHOST 2130706433

#define IS_IN_KEYRANGE(key, range) (key >= PSTRG_##range##_START && ((key - PSTRG_##range##_START) < PSTRG_##range##_SIZE))

#define EVENT_CREATURECOUNT 1
#define EVENT_CREATURE_THINK_INTERVAL 100
#define EVENT_CHECK_CREATURE_INTERVAL (EVENT_CREATURE_THINK_INTERVAL / EVENT_CREATURECOUNT)

#endif // _CONST_H
