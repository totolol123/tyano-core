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

#include "tools.h"
#include "md5.h"
#include "sha1.h"

#include "vocation.h"
#include "configmanager.h"
#include "position.h"
#include "server.h"


const std::string EMPTY_STRING;


std::string transformToSHA1(std::string plainText, bool upperCase)
{
	SHA1 sha1;
	unsigned sha1Hash[5];
	std::stringstream hexStream;

	sha1.Input((const uint8_t*)plainText.c_str(), plainText.length());
	sha1.Result(sha1Hash);

	hexStream.flags(std::ios::hex | std::ios::uppercase);
	for(uint32_t i = 0; i < 5; ++i)
		hexStream << std::setw(8) << std::setfill('0') << (uint32_t)sha1Hash[i];

	std::string hexStr = hexStream.str();
	if(!upperCase)
		toLowerCaseString(hexStr);

	return hexStr;
}

std::string transformToMD5(std::string plainText, bool upperCase)
{
	MD5_CTX m_md5;
	std::stringstream hexStream;

	MD5Init(&m_md5, 0);
	MD5Update(&m_md5, (const uint8_t*)plainText.c_str(), plainText.length());
	MD5Final(&m_md5);

	hexStream.flags(std::ios::hex | std::ios::uppercase);
	for(uint32_t i = 0; i < 16; ++i)
		hexStream << std::setw(2) << std::setfill('0') << (uint32_t)m_md5.digest[i];

	std::string hexStr = hexStream.str();
	if(!upperCase)
		toLowerCaseString(hexStr);

	return hexStr;
}

void _encrypt(std::string& str, bool upperCase)
{
	switch(server.configManager().getNumber(ConfigManager::ENCRYPTION))
	{
		case ENCRYPTION_MD5:
			str = transformToMD5(str, upperCase);
			break;
		case ENCRYPTION_SHA1:
			str = transformToSHA1(str, upperCase);
			break;
		default:
		{
			if(upperCase)
				std::transform(str.begin(), str.end(), str.begin(), upchar);

			break;
		}
	}
}

bool encryptTest(std::string plain, std::string hash)
{
	std::transform(hash.begin(), hash.end(), hash.begin(), upchar);
	_encrypt(plain, true);
	return plain == hash;
}

void replaceString(std::string& text, const std::string key, const std::string value)
{
	if(value.find(key) != std::string::npos) //don't allow infinite loops
		return;

	for(std::string::size_type keyStart = text.find(key); keyStart
		!= std::string::npos; keyStart = text.find(key))
		text.replace(keyStart, key.size(), value);
}

void trim_right(std::string& source, const std::string& t)
{
	source.erase(source.find_last_not_of(t)+1);
}

void trim_left(std::string& source, const std::string& t)
{
	source.erase(0, source.find_first_not_of(t));
}

void toLowerCaseString(std::string& source)
{
	std::transform(source.begin(), source.end(), source.begin(), tolower);
}

void toUpperCaseString(std::string& source)
{
	std::transform(source.begin(), source.end(), source.begin(), upchar);
}

std::string asLowerCaseString(const std::string& source)
{
	std::string s = source;
	toLowerCaseString(s);
	return s;
}

std::string asUpperCaseString(const std::string& source)
{
	std::string s = source;
	toUpperCaseString(s);
	return s;
}

bool booleanString(std::string source)
{
	toLowerCaseString(source);
	return (source == "yes" || source == "true" || atoi(source.c_str()) > 0);
}

bool readXMLInteger(xmlNodePtr node, const char* tag, int& value)
{
	char* nodeValue = (char*)xmlGetProp(node, (xmlChar*)tag);
	if(!nodeValue)
		return false;

	value = atoi(nodeValue);
	xmlFree(nodeValue);
	return true;
}


bool readXMLInteger64(xmlNodePtr node, const char* tag, int64_t& value)
{
	char* nodeValue = (char*)xmlGetProp(node, (xmlChar*)tag);
	if(!nodeValue)
		return false;

	// atoll sometimes isn't defined by GCC's stdlib
	value = strtoll(nodeValue, nullptr, 10);
	xmlFree(nodeValue);
	return true;
}

bool readXMLFloat(xmlNodePtr node, const char* tag, float& value)
{
	char* nodeValue = (char*)xmlGetProp(node, (xmlChar*)tag);
	if(!nodeValue)
		return false;

	value = atof(nodeValue);
	xmlFree(nodeValue);
	return true;
}

bool readXMLString(xmlNodePtr node, const char* tag, std::string& value)
{
	char* nodeValue = (char*)xmlGetProp(node, (xmlChar*)tag);
	if(!nodeValue)
		return false;

	if(!utf8ToLatin1(nodeValue, value))
		value = nodeValue;

	xmlFree(nodeValue);
	return true;
}

bool readXMLContentString(xmlNodePtr node, std::string& value)
{
	char* nodeValue = (char*)xmlNodeGetContent(node);
	if(!nodeValue)
		return false;

	if(!utf8ToLatin1(nodeValue, value))
		value = nodeValue;

	xmlFree(nodeValue);
	return true;
}

bool parseXMLContentString(xmlNodePtr node, std::string& value)
{
	bool result = false;
	std::string compareValue;
	while(node)
	{
		if(xmlStrcmp(node->name, (const xmlChar*)"text") && node->type != XML_CDATA_SECTION_NODE)
		{
			node = node->next;
			continue;
		}

		if(!readXMLContentString(node, compareValue))
		{
			node = node->next;
			continue;
		}

		trim_left(compareValue, "\r");
		trim_left(compareValue, "\n");
		trim_left(compareValue, " ");
		if(compareValue.length() > value.length())
		{
			value = compareValue;
			if(!result)
				result = true;
		}

		node = node->next;
	}

	return result;
}

std::string getLastXMLError()
{
	std::stringstream ss;
	xmlErrorPtr lastError = xmlGetLastError();
	if(lastError->line)
		ss << "Line: " << lastError->line << ", ";

	ss << "Info: " << lastError->message << std::endl;
	return ss.str();
}

bool utf8ToLatin1(char* intext, std::string& outtext)
{
	outtext = "";
	if(!intext)
		return false;

	int32_t inlen = strlen(intext);
	if(!inlen)
		return false;

	int32_t outlen = inlen * 2;
	uint8_t* outbuf = new uint8_t[outlen];

	int32_t res = UTF8Toisolat1(outbuf, &outlen, (uint8_t*)intext, &inlen);
	if(res < 0)
	{
		delete[] outbuf;
		return false;
	}

	outbuf[outlen] = '\0';
	outtext = (char*)outbuf;

	delete[] outbuf;
	return true;
}

StringVector explodeString(const std::string& string, const std::string& separator)
{
	StringVector returnVector;
	size_t start = 0, end = 0;
	while((end = string.find(separator, start)) != std::string::npos)
	{
		returnVector.push_back(string.substr(start, end - start));
		start = end + separator.size();
	}

	returnVector.push_back(string.substr(start));
	return returnVector;
}

IntegerVec vectorAtoi(StringVector stringVector)
{
	IntegerVec returnVector;
	for(StringVector::iterator it = stringVector.begin(); it != stringVector.end(); ++it)
		returnVector.push_back(atoi((*it).c_str()));

	return returnVector;
}

bool hasBitSet(uint32_t flag, uint32_t flags)
{
	return ((flags & flag) == flag);
}

int32_t round(float v)
{
	int32_t t = (int32_t)std::floor(v);
	if((v - t) > 0.5)
		return t + 1;

	return t;
}

uint32_t rand24b()
{
	return ((rand() << 12) ^ (rand())) & (0xFFFFFF);
}

float box_muller(float m, float s)
{
	// normal random variate generator
	// mean m, standard deviation s
	float x1, x2, w, y1;
	static float y2;

	static bool useLast = false;
	if(useLast) // use value from previous call
	{
		y1 = y2;
		useLast = false;
		return (m + y1 * s);
	}

	do
	{
		double r1 = (((float)(rand()) / RAND_MAX));
		double r2 = (((float)(rand()) / RAND_MAX));

		x1 = 2.0 * r1 - 1.0;
		x2 = 2.0 * r2 - 1.0;
		w = x1 * x1 + x2 * x2;
	}
	while(w >= 1.0);
	w = sqrt((-2.0 * log(w)) / w);

	y1 = x1 * w;
	y2 = x2 * w;

	useLast = true;
	return (m + y1 * s);
}


char upchar(char character)
{
	if((character >= 97 && character <= 122) || (character <= -1 && character >= -32))
		character -= 32;

	return character;
}

bool isNumber(char character)
{
	return (character >= 48 && character <= 57);
}

bool isLowercaseLetter(char character)
{
	return (character >= 97 && character <= 122);
}

bool isUppercaseLetter(char character)
{
	return (character >= 65 && character <= 90);
}

bool isPasswordCharacter(char character)
{
	return ((character >= 33 && character <= 47) || (character >= 58 && character <= 64) || (character >= 91 && character <= 96) || (character >= 123 && character <= 126));
}

bool isValidAccountName(std::string text)
{
	toLowerCaseString(text);

	uint32_t textLength = text.length();
	for(uint32_t size = 0; size < textLength; size++)
	{
		if(!isLowercaseLetter(text[size]) && !isNumber(text[size]))
			return false;
	}

	return true;
}

bool isValidPassword(std::string text)
{
	toLowerCaseString(text);

	uint32_t textLength = text.length();
	for(uint32_t size = 0; size < textLength; size++)
	{
		if(!isLowercaseLetter(text[size]) && !isNumber(text[size]) && !isPasswordCharacter(text[size]))
			return false;
	}

	return true;
}

bool isValidName(std::string text, bool forceUppercaseOnFirstLetter/* = true*/)
{
	uint32_t textLength = text.length(), lenBeforeSpace = 1, lenBeforeQuote = 1, lenBeforeDash = 1, repeatedCharacter = 0;
	char lastChar = 32;
	if(forceUppercaseOnFirstLetter)
	{
		if(!isUppercaseLetter(text[0]))
			return false;
	}
	else if(!isLowercaseLetter(text[0]) && !isUppercaseLetter(text[0]))
		return false;

	for(uint32_t size = 1; size < textLength; size++)
	{
		if(text[size] != 32)
		{
			lenBeforeSpace++;

			if(text[size] != 39)
				lenBeforeQuote++;
			else
			{
				if(lenBeforeQuote <= 1 || size == textLength - 1 || text[size + 1] == 32)
					return false;

				lenBeforeQuote = 0;
			}

			if(text[size] != 45)
				lenBeforeDash++;
			else
			{
				if(lenBeforeDash <= 1 || size == textLength - 1 || text[size + 1] == 32)
					return false;

				lenBeforeDash = 0;
			}

			if(text[size] == lastChar)
			{
				repeatedCharacter++;
				if(repeatedCharacter > 2)
					return false;
			}
			else
				repeatedCharacter = 0;

			lastChar = text[size];
		}
		else
		{
			if(lenBeforeSpace <= 1 || size == textLength - 1 || text[size + 1] == 32)
				return false;

			lenBeforeSpace = lenBeforeQuote = lenBeforeDash = 0;
		}

		if(!(isLowercaseLetter(text[size]) || text[size] == 32 || text[size] == 39 || text[size] == 45
			|| (isUppercaseLetter(text[size]) && text[size - 1] == 32)))
			return false;
	}

	return true;
}

bool isNumbers(std::string text)
{
	uint32_t textLength = text.length();
	for(uint32_t size = 0; size < textLength; size++)
	{
		if(!isNumber(text[size]))
			return false;
	}

	return true;
}

bool checkText(std::string text, std::string str)
{
	trimString(text);
	return asLowerCaseString(text) == str;
}

std::string generateRecoveryKey(int32_t fieldCount, int32_t fieldLenght)
{
	std::stringstream key;
	int32_t i = 0, j = 0, lastNumber = 99, number = 0;

	char character = 0, lastCharacter = 0;
	bool madeNumber = false, madeCharacter = false;
	do
	{
		do
		{
			madeNumber = madeCharacter = false;
			if((bool)random_range(0, 1))
			{
				number = random_range(2, 9);
				if(number != lastNumber)
				{
					key << number;
					lastNumber = number;
					madeNumber = true;
				}
			}
			else
			{
				character = (char)random_range(65, 90);
				if(character != lastCharacter)
				{
					key << character;
					lastCharacter = character;
					madeCharacter = true;
				}
			}
		}
		while((!madeCharacter && !madeNumber) ? true : (++j && j < fieldLenght));
		lastCharacter = character = number = j = 0;

		lastNumber = 99;
		if(i < fieldCount - 1)
			key << "-";
	}
	while(++i && i < fieldCount);
	return key.str();
}

void trimString(std::string& str)
{
	str.erase(str.find_last_not_of(" ") + 1);
	str.erase(0, str.find_first_not_of(" "));
}

std::string parseParams(tokenizer::iterator &it, tokenizer::iterator end)
{
	if(it == end)
		return "";

	std::string tmp = (*it);
	++it;
	if(tmp[0] == '"')
	{
		tmp.erase(0, 1);
		while(it != end && tmp[tmp.length() - 1] != '"')
		{
			tmp += " " + (*it);
			++it;
		}

		if(tmp.length() > 0 && tmp[tmp.length() - 1] == '"')
			tmp.erase(tmp.length() - 1);
	}

	return tmp;
}

std::string formatDate(time_t _time/* = 0*/)
{
	char buffer[21];
	if(!_time)
		_time = time(nullptr);

	const tm* tms = localtime(&_time);
	if(tms)
		sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", tms->tm_mday, tms->tm_mon + 1, tms->tm_year + 1900, tms->tm_hour, tms->tm_min, tms->tm_sec);
	else
		sprintf(buffer, "UNIX Time: %d", (int32_t)_time);

	return buffer;
}

std::string formatDateShort(time_t _time, bool detailed/* = false*/)
{
	char buffer[21];
	if(!_time)
		_time = time(nullptr);

	const tm* tms = localtime(&_time);
	if(tms)
	{
		std::string format = "%d %b %Y";
		if(detailed)
			format += " %H:%M:%S";

		strftime(buffer, 25, format.c_str(), tms);
	}
	else
		sprintf(buffer, "UNIX Time: %d", (int32_t)_time);

	return buffer;
}

std::string formatTime(int32_t hours, int32_t minutes)
{
	std::stringstream time;
	if(hours)
		time << hours << " " << (hours > 1 ? "hours" : "hour") << (minutes ? " and " : "");

	if(minutes)
		time << minutes << " " << (minutes > 1 ? "minutes" : "minute");

	return time.str();
}

std::string convertIPAddress(uint32_t ip)
{
	char buffer[17];
	sprintf(buffer, "%d.%d.%d.%d", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24));
	return buffer;
}

Skulls_t getSkull(std::string strValue)
{
	std::string tmpStrValue = asLowerCaseString(strValue);
	if(tmpStrValue == "black" || tmpStrValue == "5")
		return SKULL_BLACK;
	else if(tmpStrValue == "red" || tmpStrValue == "4")
		return SKULL_RED;
	else if(tmpStrValue == "white" || tmpStrValue == "3")
		return SKULL_WHITE;
	else if(tmpStrValue == "green" || tmpStrValue == "2")
		return SKULL_GREEN;
	else if(tmpStrValue == "yellow" || tmpStrValue == "1")
		return SKULL_YELLOW;

	return SKULL_NONE;
}

PartyShields_t getPartyShield(std::string strValue)
{
	std::string tmpStrValue = asLowerCaseString(strValue);
	if(tmpStrValue == "whitenoshareoff" || tmpStrValue == "10")
		return SHIELD_YELLOW_NOSHAREDEXP;
	else if(tmpStrValue == "blueshareoff" || tmpStrValue == "9")
		return SHIELD_BLUE_NOSHAREDEXP;
	else if(tmpStrValue == "yellowshareblink" || tmpStrValue == "8")
		return SHIELD_YELLOW_NOSHAREDEXP_BLINK;
	else if(tmpStrValue == "blueshareblink" || tmpStrValue == "7")
		return SHIELD_BLUE_NOSHAREDEXP_BLINK;
	else if(tmpStrValue == "yellowshareon" || tmpStrValue == "6")
		return SHIELD_YELLOW_SHAREDEXP;
	else if(tmpStrValue == "blueshareon" || tmpStrValue == "5")
		return SHIELD_BLUE_SHAREDEXP;
	else if(tmpStrValue == "yellow" || tmpStrValue == "4")
		return SHIELD_YELLOW;
	else if(tmpStrValue == "blue" || tmpStrValue == "3")
		return SHIELD_BLUE;
	else if(tmpStrValue == "whiteyellow" || tmpStrValue == "2")
		return SHIELD_WHITEYELLOW;
	else if(tmpStrValue == "whiteblue" || tmpStrValue == "1")
		return SHIELD_WHITEBLUE;

	return SHIELD_NONE;
}


bool getDirection(const std::string& string, Direction& target) {
	if (string == "east") {
		target = Direction::EAST;
		return true;
	}
	if (string == "north") {
		target = Direction::NORTH;
		return true;
	}
	if (string == "northEast") {
		target = Direction::NORTH_EAST;
		return true;
	}
	if (string == "northWest") {
		target = Direction::NORTH_WEST;
		return true;
	}
	if (string == "south") {
		target = Direction::SOUTH;
		return true;
	}
	if (string == "southEast") {
		target = Direction::SOUTH_EAST;
		return true;
	}
	if (string == "southWest") {
		target = Direction::SOUTH_WEST;
		return true;
	}
	if (string == "west") {
		target = Direction::WEST;
		return true;
	}

	return false;
}


bool getDeprecatedDirection(const std::string& string, Direction& target) {
	if (string == "e" || string == "1") {
		target = Direction::EAST;
		return true;
	}
	if (string == "n" || string == "0") {
		target = Direction::NORTH;
		return true;
	}
	if (string == "north east" || string == "north-east" || string == "ne" || string == "7") {
		target = Direction::NORTH_EAST;
		return true;
	}
	if (string == "north west" || string == "north-west" || string == "nw" || string == "6") {
		target = Direction::NORTH_WEST;
		return true;
	}
	if (string == "s" || string == "2") {
		target = Direction::SOUTH;
		return true;
	}
	if (string == "south east" || string == "south-east" || string == "se" || string == "5") {
		target = Direction::SOUTH_EAST;
		return true;
	}
	if (string == "south west" || string == "south-west" || string == "sw" || string == "4") {
		target = Direction::SOUTH_WEST;
		return true;
	}
	if (string == "w" || string == "3") {
		target = Direction::WEST;
		return true;
	}

	return false;
}


std::string getDirectionNames() {
	return "east, north, northEast, northWest, south, southEast, southWest or west";
}


Direction getDirectionTo(Position pos1, Position pos2, bool extended/* = true*/)
{
	Direction direction = Direction::NORTH;
	if(pos1.x > pos2.x)
	{
		direction = Direction::WEST;
		if(extended)
		{
			if(pos1.y > pos2.y)
				direction = Direction::NORTH_WEST;
			else if(pos1.y < pos2.y)
				direction = Direction::SOUTH_WEST;
		}
	}
	else if(pos1.x < pos2.x)
	{
		direction = Direction::EAST;
		if(extended)
		{
			if(pos1.y > pos2.y)
				direction = Direction::NORTH_EAST;
			else if(pos1.y < pos2.y)
				direction = Direction::SOUTH_EAST;
		}
	}
	else
	{
		if(pos1.y > pos2.y)
			direction = Direction::NORTH;
		else if(pos1.y < pos2.y)
			direction = Direction::SOUTH;
	}

	return direction;
}

Direction getReverseDirection(Direction dir)
{
	switch(dir)
	{
		case Direction::NORTH:
			return Direction::SOUTH;
		case Direction::SOUTH:
			return Direction::NORTH;
		case Direction::WEST:
			return Direction::EAST;
		case Direction::EAST:
			return Direction::WEST;
		case Direction::SOUTH_WEST:
			return Direction::NORTH_EAST;
		case Direction::NORTH_WEST:
			return Direction::SOUTH_EAST;
		case Direction::NORTH_EAST:
			return Direction::SOUTH_WEST;
		case Direction::SOUTH_EAST:
			return Direction::NORTH_WEST;
	}

	return Direction::SOUTH;
}

Position getNextPosition(Direction direction, Position pos)
{
	switch(direction)
	{
		case Direction::NORTH:
			pos.y--;
			break;
		case Direction::SOUTH:
			pos.y++;
			break;
		case Direction::WEST:
			pos.x--;
			break;
		case Direction::EAST:
			pos.x++;
			break;
		case Direction::SOUTH_WEST:
			pos.x--;
			pos.y++;
			break;
		case Direction::NORTH_WEST:
			pos.x--;
			pos.y--;
			break;
		case Direction::SOUTH_EAST:
			pos.x++;
			pos.y++;
			break;
		case Direction::NORTH_EAST:
			pos.x++;
			pos.y--;
			break;
	}

	return pos;
}

struct AmmoTypeNames
{
	const char* name;
	Ammo_t ammoType;
};

struct MagicEffectNames
{
	const char* name;
	MagicEffect_t magicEffect;
};

struct ShootTypeNames
{
	const char* name;
	ShootEffect_t shootType;
};

struct CombatTypeNames
{
	const char* name;
	CombatType_t combatType;
};

struct AmmoActionNames
{
	const char* name;
	AmmoAction_t ammoAction;
};

struct FluidTypeNames
{
	const char* name;
	FluidTypes_t fluidType;
};

struct SkillIdNames
{
	const char* name;
	skills_t skillId;
};

MagicEffectNames magicEffectNames[] =
{
	{"assassin",		MAGIC_EFFECT_ASSASSIN},
	{"bats",			MAGIC_EFFECT_BATS},
	{"bigclouds",		MAGIC_EFFECT_BIGCLOUDS},
	{"bigplants",		MAGIC_EFFECT_BIGPLANTS},
	{"blackspark",		MAGIC_EFFECT_HIT_AREA},
	{"bloodysteps",		MAGIC_EFFECT_BLOODYSTEPS},
	{"bluebubble",		MAGIC_EFFECT_LOSE_ENERGY},
	{"bluefirework",	MAGIC_EFFECT_FIREWORK_BLUE},
	{"bluenote",		MAGIC_EFFECT_SOUND_BLUE},
	{"blueshimmer",		MAGIC_EFFECT_WRAPS_BLUE},
	{"bubbles",			MAGIC_EFFECT_BUBBLES},
	{"cake",			MAGIC_EFFECT_CAKE},
	{"carniphila",		MAGIC_EFFECT_CARNIPHILA},
	{"dice",			MAGIC_EFFECT_CRAPS},
	{"dragonhead",		MAGIC_EFFECT_DRAGONHEAD},
	{"energy",			MAGIC_EFFECT_ENERGY_DAMAGE},
	{"energyarea",		MAGIC_EFFECT_ENERGY_AREA},
	{"explosion",		MAGIC_EFFECT_EXPLOSION_DAMAGE},
	{"explosionarea",	MAGIC_EFFECT_EXPLOSION_AREA},
	{"fire",			MAGIC_EFFECT_HITBY_FIRE},
	{"firearea",		MAGIC_EFFECT_FIRE_AREA},
	{"fireattack",		MAGIC_EFFECT_FIREATTACK},
	{"giantice",		MAGIC_EFFECT_GIANTICE},
	{"giftwraps",		MAGIC_EFFECT_GIFT_WRAPS},
	{"greenbubble",		MAGIC_EFFECT_POISON_RINGS},
	{"greennote",		MAGIC_EFFECT_SOUND_GREEN},
	{"greenshimmer",	MAGIC_EFFECT_WRAPS_GREEN},
	{"greenspark",		MAGIC_EFFECT_POISON},
	{"groundshaker",	MAGIC_EFFECT_GROUNDSHAKER},
	{"hearts",			MAGIC_EFFECT_HEARTS},
	{"holyarea",		MAGIC_EFFECT_HOLYAREA},
	{"holydamage",		MAGIC_EFFECT_HOLYDAMAGE},
	{"icearea",			MAGIC_EFFECT_ICEAREA},
	{"iceattack",		MAGIC_EFFECT_ICEATTACK},
	{"icetornado",		MAGIC_EFFECT_ICETORNADO},
	{"insects",			MAGIC_EFFECT_INSECTS},
	{"mirrorhorizontal",MAGIC_EFFECT_MIRRORHORIZONTAL},
	{"mirrorvertical",	MAGIC_EFFECT_MIRRORVERTICAL},
	{"mortarea",		MAGIC_EFFECT_MORT_AREA},
	{"plantattack",		MAGIC_EFFECT_PLANTATTACK},
	{"poff",			MAGIC_EFFECT_POFF},
	{"poison",			MAGIC_EFFECT_POISON_AREA},
	{"purpleenergy",	MAGIC_EFFECT_PURPLEENERGY},
	{"purplenote",		MAGIC_EFFECT_SOUND_PURPLE},
	{"redfirework",		MAGIC_EFFECT_FIREWORK_RED},
	{"rednote",			MAGIC_EFFECT_SOUND_RED},
	{"redspark",		MAGIC_EFFECT_DRAW_BLOOD},
	{"redshimmer",		MAGIC_EFFECT_WRAPS_RED},
	{"skullhorizontal",	MAGIC_EFFECT_SKULLHORIZONTAL},
	{"skullvertical",	MAGIC_EFFECT_SKULLVERTICAL},
	{"sleep",			MAGIC_EFFECT_SLEEP},
	{"smallclouds",		MAGIC_EFFECT_SMALLCLOUDS},
	{"smallplants",		MAGIC_EFFECT_SMALLPLANTS},
	{"smoke",			MAGIC_EFFECT_SMOKE},
	{"stepshorizontal",	MAGIC_EFFECT_STEPSHORIZONTAL},
	{"stepsvertical",	MAGIC_EFFECT_STEPSVERTICAL},
	{"stones",			MAGIC_EFFECT_STONES},
	{"stun",			MAGIC_EFFECT_STUN},
	{"teleport",		MAGIC_EFFECT_TELEPORT},
	{"tutorialarrow",	MAGIC_EFFECT_TUTORIALARROW},
	{"tutorialsquare",	MAGIC_EFFECT_TUTORIALSQUARE},
	{"watercreature",	MAGIC_EFFECT_WATERCREATURE},
	{"watersplash",		MAGIC_EFFECT_WATERSPLASH},
	{"whitenote",		MAGIC_EFFECT_SOUND_WHITE},
	{"yalaharighost",	MAGIC_EFFECT_YALAHARIGHOST},
	{"yellowbubble",	MAGIC_EFFECT_YELLOW_RINGS},
	{"yellowenergy",	MAGIC_EFFECT_YELLOWENERGY},
	{"yellowfirework",	MAGIC_EFFECT_FIREWORK_YELLOW},
	{"yellownote",		MAGIC_EFFECT_SOUND_YELLOW},
	{"yellowspark",		MAGIC_EFFECT_BLOCKHIT},
};

ShootTypeNames shootTypeNames[] =
{
	{"arrow",		SHOOT_EFFECT_ARROW},
	{"bolt",		SHOOT_EFFECT_BOLT},
	{"burstarrow",		SHOOT_EFFECT_BURSTARROW},
	{"cake",		SHOOT_EFFECT_CAKE},
	{"death",		SHOOT_EFFECT_DEATH},
	{"earth",		SHOOT_EFFECT_EARTH},
	{"eartharrow",		SHOOT_EFFECT_EARTHARROW},
	{"enchantedspear",	SHOOT_EFFECT_ENCHANTEDSPEAR},
	{"energy",		SHOOT_EFFECT_ENERGY},
	{"energyball",		SHOOT_EFFECT_ENERGYBALL},
	{"etherealspear",	SHOOT_EFFECT_ETHEREALSPEAR},
	{"explosion",		SHOOT_EFFECT_EXPLOSION},
	{"flamingarrow",	SHOOT_EFFECT_FLAMMINGARROW},
	{"flasharrow",		SHOOT_EFFECT_FLASHARROW},
	{"fire",		SHOOT_EFFECT_FIRE},
	{"greenstar",		SHOOT_EFFECT_GREENSTAR},
	{"holy",		SHOOT_EFFECT_HOLY},
	{"huntingspear",	SHOOT_EFFECT_HUNTINGSPEAR},
	{"ice",			SHOOT_EFFECT_ICE},
	{"infernalbolt",	SHOOT_EFFECT_INFERNALBOLT},
	{"largerock",		SHOOT_EFFECT_LARGEROCK},
	{"onyxarrow",		SHOOT_EFFECT_ONYXARROW},
	{"piercingbolt",	SHOOT_EFFECT_PIERCINGBOLT},
	{"poison",		SHOOT_EFFECT_POISONFIELD},
	{"poisonarrow",		SHOOT_EFFECT_POISONARROW},
	{"powerbolt",		SHOOT_EFFECT_POWERBOLT},
	{"redstar",		SHOOT_EFFECT_REDSTAR},
	{"royalspear",		SHOOT_EFFECT_ROYALSPEAR},
	{"shiverarrow",		SHOOT_EFFECT_SHIVERARROW},
	{"smallearth",		SHOOT_EFFECT_SMALLEARTH},
	{"smallholy",		SHOOT_EFFECT_SMALLHOLY},
	{"smallice",		SHOOT_EFFECT_SMALLICE},
	{"smallstone",		SHOOT_EFFECT_SMALLSTONE},
	{"sniperarrow",		SHOOT_EFFECT_SNIPERARROW},
	{"snowball",		SHOOT_EFFECT_SNOWBALL},
	{"spear",		SHOOT_EFFECT_SPEAR},
	{"suddendeath",		SHOOT_EFFECT_SUDDENDEATH},
	{"throwingknife",	SHOOT_EFFECT_THROWINGKNIFE},
	{"throwingstar",	SHOOT_EFFECT_THROWINGSTAR},
	{"whirlwindaxe",	SHOOT_EFFECT_WHIRLWINDAXE},
	{"whirlwindclub",	SHOOT_EFFECT_WHIRLWINDCLUB},
	{"whirlwindsword",	SHOOT_EFFECT_WHIRLWINDSWORD},
};

CombatTypeNames combatTypeNames[] =
{
	{"physical",		COMBAT_PHYSICALDAMAGE},
	{"energy",		COMBAT_ENERGYDAMAGE},
	{"earth",		COMBAT_EARTHDAMAGE},
	{"fire",		COMBAT_FIREDAMAGE},
	{"undefined",		COMBAT_UNDEFINEDDAMAGE},
	{"lifedrain",		COMBAT_LIFEDRAIN},
	{"life drain",		COMBAT_LIFEDRAIN},
	{"manadrain",		COMBAT_MANADRAIN},
	{"mana drain",		COMBAT_MANADRAIN},
	{"healing",		COMBAT_HEALING},
	{"drown",		COMBAT_DROWNDAMAGE},
	{"ice",			COMBAT_ICEDAMAGE},
	{"holy",		COMBAT_HOLYDAMAGE},
	{"death",		COMBAT_DEATHDAMAGE}
};

AmmoTypeNames ammoTypeNames[] =
{
	{"arrow",         AMMO_ARROW},
	{"bolt",          AMMO_BOLT},
	{"snowball",      AMMO_SNOWBALL},
	{"spear",         AMMO_SPEAR},
	{"stone",         AMMO_STONE},
	{"throwingknife", AMMO_THROWINGKNIFE},
	{"throwingstar",  AMMO_THROWINGSTAR},
};

AmmoActionNames ammoActionNames[] =
{
	{"move",         AMMOACTION_MOVE},
	{"moveback",     AMMOACTION_MOVEBACK},
	{"removecharge", AMMOACTION_REMOVECHARGE},
	{"removecount",  AMMOACTION_REMOVECOUNT},
};

FluidTypeNames fluidTypeNames[] =
{
	{"beer",		FLUID_BEER},
	{"blood",		FLUID_BLOOD},
	{"coconutmilk",	FLUID_COCONUTMILK},
	{"fruitjuice",	FLUID_FRUITJUICE},
	{"lemonade",	FLUID_LEMONADE},
	{"lava",		FLUID_LAVA},
	{"life",		FLUID_LIFE},
	{"mana",		FLUID_MANA},
	{"milk",		FLUID_MILK},
	{"mud",			FLUID_MUD},
	{"oil",			FLUID_OIL},
	{"rum",			FLUID_RUM},
	{"slime",		FLUID_SLIME},
	{"swamp",		FLUID_SWAMP},
	{"urine",		FLUID_URINE},
	{"water",		FLUID_WATER},
	{"wine",		FLUID_WINE},
};

SkillIdNames skillIdNames[] =
{
	{"fist",		SKILL_FIST},
	{"club",		SKILL_CLUB},
	{"sword",		SKILL_SWORD},
	{"axe",			SKILL_AXE},
	{"distance",		SKILL_DIST},
	{"dist",		SKILL_DIST},
	{"shielding",		SKILL_SHIELD},
	{"shield",		SKILL_SHIELD},
	{"fishing",		SKILL_FISH},
	{"fish",		SKILL_FISH},
	{"level",		SKILL__LEVEL},
	{"magiclevel",		SKILL__MAGLEVEL},
	{"magic level",		SKILL__MAGLEVEL}
};

MagicEffect_t getMagicEffect(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(magicEffectNames) / sizeof(MagicEffectNames); ++i)
	{
		if(strValue == magicEffectNames[i].name)
			return magicEffectNames[i].magicEffect;
	}

	return MAGIC_EFFECT_UNKNOWN;
}

std::string getMagicEffectNames() {
	std::ostringstream stream;

	uint32_t last = (sizeof(magicEffectNames) / sizeof(MagicEffectNames)) - 1;
	for(uint32_t i = 0; i <= last; ++i)
	{
		if (i != 0) {
			if (last) {
				stream << " or ";
			}
			else {
				stream << ", ";
			}
		}

		stream << shootTypeNames[i].name;
	}

	return stream.str();
}

ShootEffect_t getShootType(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(shootTypeNames) / sizeof(ShootTypeNames); ++i)
	{
		if(strValue == shootTypeNames[i].name)
			return shootTypeNames[i].shootType;
	}

	return SHOOT_EFFECT_UNKNOWN;
}

std::string getShootEffectNames() {
	std::ostringstream stream;

	uint32_t last = (sizeof(shootTypeNames) / sizeof(ShootTypeNames)) - 1;
	for(uint32_t i = 0; i <= last; ++i)
	{
		if (i != 0) {
			if (last) {
				stream << " or ";
			}
			else {
				stream << ", ";
			}
		}

		stream << shootTypeNames[i].name;
	}

	return stream.str();
}

CombatType_t getCombatType(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(combatTypeNames) / sizeof(CombatTypeNames); ++i)
	{
		if(!strcasecmp(strValue.c_str(), combatTypeNames[i].name))
			return combatTypeNames[i].combatType;
	}

	return COMBAT_NONE;
}

Ammo_t getAmmoType(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(ammoTypeNames) / sizeof(AmmoTypeNames); ++i)
	{
		if(strValue == ammoTypeNames[i].name)
			return ammoTypeNames[i].ammoType;
	}

	return AMMO_NONE;
}

std::string getAmmoTypeNames() {
	std::ostringstream stream;

	uint32_t last = (sizeof(ammoTypeNames) / sizeof(AmmoTypeNames)) - 1;
	for(uint32_t i = 0; i <= last; ++i)
	{
		if (i != 0) {
			if (last) {
				stream << " or ";
			}
			else {
				stream << ", ";
			}
		}

		stream << ammoTypeNames[i].name;
	}

	return stream.str();
}

AmmoAction_t getAmmoAction(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(ammoActionNames) / sizeof(AmmoActionNames); ++i)
	{
		if(strValue == ammoActionNames[i].name)
			return ammoActionNames[i].ammoAction;
	}

	return AMMOACTION_NONE;
}

std::string getAmmoConsumptionNames() {
	std::ostringstream stream;

	uint32_t last = (sizeof(ammoActionNames) / sizeof(AmmoActionNames)) - 1;
	for(uint32_t i = 0; i <= last; ++i)
	{
		if (i != 0) {
			if (last) {
				stream << " or ";
			}
			else {
				stream << ", ";
			}
		}

		stream << ammoActionNames[i].name;
	}

	return stream.str();
}

FluidTypes_t getFluidType(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(fluidTypeNames) / sizeof(FluidTypeNames); ++i)
	{
		if(strValue == fluidTypeNames[i].name)
			return fluidTypeNames[i].fluidType;
	}

	return FLUID_NONE;
}

std::string getFluidNames() {
	std::ostringstream stream;

	uint32_t last = (sizeof(fluidTypeNames) / sizeof(FluidTypeNames)) - 1;
	for(uint32_t i = 0; i <= last; ++i)
	{
		if (i != 0) {
			if (last) {
				stream << " or ";
			}
			else {
				stream << ", ";
			}
		}

		stream << fluidTypeNames[i].name;
	}

	return stream.str();
}

skills_t getSkillId(const std::string& strValue)
{
	for(uint32_t i = 0; i < sizeof(skillIdNames) / sizeof(SkillIdNames); ++i)
	{
		if(!strcasecmp(strValue.c_str(), skillIdNames[i].name))
			return skillIdNames[i].skillId;
	}

	return SKILL_FIST;
}

std::string getCombatName(CombatType_t combatType)
{
	switch(combatType)
	{
		case COMBAT_PHYSICALDAMAGE:
			return "physical";
		case COMBAT_ENERGYDAMAGE:
			return "energy";
		case COMBAT_EARTHDAMAGE:
			return "earth";
		case COMBAT_FIREDAMAGE:
			return "fire";
		case COMBAT_UNDEFINEDDAMAGE:
			return "undefined";
		case COMBAT_LIFEDRAIN:
			return "life drain";
		case COMBAT_MANADRAIN:
			return "mana drain";
		case COMBAT_HEALING:
			return "healing";
		case COMBAT_DROWNDAMAGE:
			return "drown";
		case COMBAT_ICEDAMAGE:
			return "ice";
		case COMBAT_HOLYDAMAGE:
			return "holy";
		case COMBAT_DEATHDAMAGE:
			return "death";
		default:
			break;
	}

	return "unknown";
}

std::string getSkillName(uint16_t skillId, bool suffix/* = true*/)
{
	switch(skillId)
	{
		case SKILL_FIST:
		{
			std::string tmp = "fist";
			if(suffix)
				tmp += " fighting";

			return tmp;
		}
		case SKILL_CLUB:
		{
			std::string tmp = "club";
			if(suffix)
				tmp += " fighting";

			return tmp;
		}
		case SKILL_SWORD:
		{
			std::string tmp = "sword";
			if(suffix)
				tmp += " fighting";

			return tmp;
		}
		case SKILL_AXE:
		{
			std::string tmp = "axe";
			if(suffix)
				tmp += " fighting";

			return tmp;
		}
		case SKILL_DIST:
		{
			std::string tmp = "distance";
			if(suffix)
				tmp += " fighting";

			return tmp;
		}
		case SKILL_SHIELD:
			return "shielding";
		case SKILL_FISH:
			return "fishing";
		case SKILL__MAGLEVEL:
			return "magic level";
		case SKILL__LEVEL:
			return "level";
		default:
			break;
	}

	return "unknown";
}

std::string getReason(int32_t reasonId)
{
	switch(reasonId)
	{
		case 0:
			return "Offensive Name";
		case 1:
			return "Invalid Name Format";
		case 2:
			return "Unsuitable Name";
		case 3:
			return "Name Inciting Rule Violation";
		case 4:
			return "Offensive Statement";
		case 5:
			return "Spamming";
		case 6:
			return "Illegal Advertising";
		case 7:
			return "Off-Topic Public Statement";
		case 8:
			return "Non-English Public Statement";
		case 9:
			return "Inciting Rule Violation";
		case 10:
			return "Bug Abuse";
		case 11:
			return "Game Weakness Abuse";
		case 12:
			return "Using Unofficial Software to Play";
		case 13:
			return "Hacking";
		case 14:
			return "Multi-Clienting";
		case 15:
			return "Account Trading or Sharing";
		case 16:
			return "Threatening Gamemaster";
		case 17:
			return "Pretending to Have Influence on Rule Enforcement";
		case 18:
			return "False Report to Gamemaster";
		case 19:
			return "Destructive Behaviour";
		case 20:
			return "Excessive Unjustified Player Killing";
		default:
			break;
	}

	return "Unknown Reason";
}

std::string getAction(ViolationAction_t actionId, bool ipBanishment)
{
	std::string action = "Unknown";
	switch(actionId)
	{
		case ACTION_NOTATION:
			action = "Notation";
			break;
		case ACTION_NAMEREPORT:
			action = "Name Report";
			break;
		case ACTION_BANISHMENT:
			action = "Banishment";
			break;
		case ACTION_BANREPORT:
			action = "Name Report + Banishment";
			break;
		case ACTION_BANFINAL:
			action = "Banishment + Final Warning";
			break;
		case ACTION_BANREPORTFINAL:
			action = "Name Report + Banishment + Final Warning";
			break;
		case ACTION_STATEMENT:
			action = "Statement Report";
			break;
		//internal use
		case ACTION_DELETION:
			action = "Deletion";
			break;
		case ACTION_NAMELOCK:
			action = "Name Lock";
			break;
		case ACTION_BANLOCK:
			action = "Name Lock + Banishment";
			break;
		case ACTION_BANLOCKFINAL:
			action = "Name Lock + Banishment + Final Warning";
			break;
		default:
			break;
	}

	if(ipBanishment)
		action += " + IP Banishment";

	return action;
}

std::string parseVocationString(StringVector vocStringVec)
{
	std::string str = "";
	if(!vocStringVec.empty())
	{
		for(StringVector::iterator it = vocStringVec.begin(); it != vocStringVec.end(); ++it)
		{
			if((*it) != vocStringVec.front())
			{
				if((*it) != vocStringVec.back())
					str += ", ";
				else
					str += " and ";
			}

			str += (*it);
			str += "s";
		}
	}

	return str;
}

bool parseVocationNode(xmlNodePtr vocationNode, VocationMap& vocationMap, StringVector& vocStringVec, std::string& errorStr)
{
	if(xmlStrcmp(vocationNode->name,(const xmlChar*)"vocation"))
		return true;

	int32_t vocationId = -1;
	std::string strValue, tmpStrValue;
	if(readXMLString(vocationNode, "name", strValue))
	{
		vocationId = Vocations::getInstance()->getVocationId(strValue);
		if(vocationId != -1)
		{
			vocationMap[vocationId] = true;
			int32_t promotedVocation = Vocations::getInstance()->getPromotedVocation(vocationId);
			if(promotedVocation != -1)
				vocationMap[promotedVocation] = true;
		}
		else
		{
			errorStr = "Wrong vocation name: " + strValue;
			return false;
		}
	}
	else if(readXMLString(vocationNode, "id", strValue))
	{
		IntegerVec intVector;
		if(!parseIntegerVec(strValue, intVector))
		{
			errorStr = "Invalid vocation id - '" + strValue + "'";
			return false;
		}

		size_t size = intVector.size();
		for(size_t i = 0; i < size; ++i)
		{
			Vocation* vocation = Vocations::getInstance()->getVocation(intVector[i]);
			if(vocation && vocation->getName() != "")
			{
				vocationId = vocation->getId();
				strValue = vocation->getName();

				vocationMap[vocationId] = true;
				int32_t promotedVocation = Vocations::getInstance()->getPromotedVocation(vocationId);
				if(promotedVocation != -1)
					vocationMap[promotedVocation] = true;
			}
			else
			{
				std::stringstream ss;
				ss << "Wrong vocation id: " << intVector[i];

				errorStr = ss.str();
				return false;
			}
		}
	}

	if(vocationId != -1 && (!readXMLString(vocationNode, "showInDescription", tmpStrValue) || booleanString(tmpStrValue)))
		vocStringVec.push_back(asLowerCaseString(strValue));

	return true;
}

bool parseIntegerVec(std::string str, IntegerVec& intVector)
{
	StringVector strVector = explodeString(str, ";");
	IntegerVec tmpIntVector;
	for(StringVector::iterator it = strVector.begin(); it != strVector.end(); ++it)
	{
		tmpIntVector = vectorAtoi(explodeString((*it), "-"));
		if(!tmpIntVector[0] && it->substr(0, 1) != "0")
			continue;

		intVector.push_back(tmpIntVector[0]);
		if(tmpIntVector.size() > 1)
		{
			while(tmpIntVector[0] < tmpIntVector[1])
				intVector.push_back(++tmpIntVector[0]);
		}
	}

	return true;
}

bool fileExists(const char* filename)
{
	FILE* f = fopen(filename, "rb");
	if(!f)
		return false;

	fclose(f);
	return true;
}

uint32_t adlerChecksum(uint8_t *data, size_t length)
{
	if(length > NETWORKMESSAGE_MAXSIZE)
		return 0;

	const uint16_t adler = 65521;
	uint32_t a = 1, b = 0;
	while(length > 0)
	{
		size_t tmp = length > 5552 ? 5552 : length;
		length -= tmp;
		do
		{
			a += *data++;
			b += a;
		}
		while(--tmp);

		a %= adler;
		b %= adler;
	}

	return (b << 16) | a;
}

std::string getFilePath(FileType filetype, std::string filename)
{
	std::string path = server.configManager().getString(ConfigManager::DATA_DIRECTORY);
	switch(filetype)
	{
		case FileType::OTHER:
			path += filename;
			break;
		case FileType::XML:
			path += "XML/" + filename;
			break;
		case FileType::LOG:
			path += "logs/" + filename;
			break;
		case FileType::MOD:
		{
			path = "mods/" + filename;
			break;
		}
		case FileType::CONFIG:
		{
			path = filename;
			break;
		}
	}

	return path;
}
