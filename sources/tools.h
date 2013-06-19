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

#ifndef _TOOLS_H
#define _TOOLS_H

#include "const.h"

class Position;


extern const std::string EMPTY_STRING;

typedef std::vector<int32_t> IntegerVec;

typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
typedef std::map<int32_t, bool> VocationMap;

CombatTypeIndex combatTypeIndexFromCombatType(CombatType_t combatType);

std::string transformToMD5(std::string plainText, bool upperCase);
std::string transformToSHA1(std::string plainText, bool upperCase);

void _encrypt(std::string& str, bool upperCase);
bool encryptTest(std::string plain, std::string hash);

void replaceString(std::string& text, const std::string key, const std::string value);
void trim_right(std::string& source, const std::string& t);
void trim_left(std::string& source, const std::string& t);
void toLowerCaseString(std::string& source);
void toUpperCaseString(std::string& source);
std::string asLowerCaseString(const std::string& source);
std::string asUpperCaseString(const std::string& source);
bool booleanString(std::string source);

bool readXMLInteger(xmlNodePtr node, const char* tag, int& value);
bool readXMLInteger64(xmlNodePtr node, const char* tag, int64_t& value);
bool readXMLFloat(xmlNodePtr node, const char* tag, float& value);
bool readXMLString(xmlNodePtr node, const char* tag, std::string& value);
bool readXMLContentString(xmlNodePtr node, std::string& value);
bool parseXMLContentString(xmlNodePtr node, std::string& value);
std::string getLastXMLError();
bool utf8ToLatin1(char* intext, std::string& outtext);

StringVector explodeString(const std::string& string, const std::string& separator);
IntegerVec vectorAtoi(StringVector stringVector);
bool hasBitSet(uint32_t flag, uint32_t flags);

bool isNumber(char character);
bool isLowercaseLetter(char character);
bool isUppercaseLetter(char character);
bool isPasswordCharacter(char character);

bool isValidAccountName(std::string text);
bool isValidPassword(std::string text);
bool isValidName(std::string text, bool forceUppercaseOnFirstLetter = true);
bool isNumbers(std::string text);

char upchar(char character);
bool checkText(std::string text, std::string str);
void trimString(std::string& str);
std::string parseParams(tokenizer::iterator &it, tokenizer::iterator end);

std::string generateRecoveryKey(int32_t fieldCount, int32_t fieldLength);

uint32_t rand24b();
float box_muller(float m, float s);

template<class T>
T random_range(T lowestNumber, T highestNumber, DistributionType_t type = DISTRO_UNIFORM) {
	if(highestNumber == lowestNumber)
		return lowestNumber;

	if(lowestNumber > highestNumber)
		std::swap(lowestNumber, highestNumber);

	switch(type)
	{
		case DISTRO_UNIFORM:
			return (lowestNumber + ((int32_t)rand24b() % (highestNumber - lowestNumber + 1)));
		case DISTRO_NORMAL:
			return (lowestNumber + int32_t(float(highestNumber - lowestNumber) * (float)std::min((float)1, std::max((float)0, box_muller(0.5, 0.25)))));
		default:
			break;
	}

	const float randMax = 16777216;
	return (lowestNumber + int32_t(float(highestNumber - lowestNumber) * float(1.f - sqrt((1.f * rand24b()) / randMax))));
}

int32_t round(float v);

Skulls_t getSkull(std::string strValue);
PartyShields_t getPartyShield(std::string strValue);

Direction getDirection(std::string string);
Direction getDirectionTo(Position pos1, Position pos2, bool extended = true);
Direction getReverseDirection(Direction dir);
Position getNextPosition(Direction direction, Position pos);

std::string formatDate(time_t _time = 0);
std::string formatDateShort(time_t _time = 0, bool detailed = false);
std::string formatTime(int32_t hours, int32_t minutes);
std::string convertIPAddress(uint32_t ip);

MagicEffect_t getMagicEffect(const std::string& strValue);
ShootEffect_t getShootType(const std::string& strValue);
Ammo_t getAmmoType(const std::string& strValue);
AmmoAction_t getAmmoAction(const std::string& strValue);
CombatType_t getCombatType(const std::string& strValue);
FluidTypes_t getFluidType(const std::string& strValue);
skills_t getSkillId(const std::string& strValue);

std::string getCombatName(CombatType_t combatType);
std::string getSkillName(uint16_t skillId, bool suffix = true);

std::string getReason(int32_t reasonId);
std::string getAction(ViolationAction_t actionId, bool ipBanishment);

std::string parseVocationString(StringVector vocStringVec);
bool parseVocationNode(xmlNodePtr vocationNode, VocationMap& vocationMap, StringVector& vocStringMap, std::string& errorStr);
bool parseIntegerVec(std::string str, IntegerVec& intVector);

bool fileExists(const char* filename);
uint32_t adlerChecksum(uint8_t *data, size_t length);

std::string getFilePath(FileType filetype, std::string filename);


class UnixTimestamp {

public:

	explicit UnixTimestamp(uint32_t value);

	operator RealTime() const;


private:

	static RealTime realTimeForUtcDate(int day, int month, int year);


	static const RealTime _epoch;
	static const uint32_t _epochValue;

	uint32_t _value;

};

#endif // _TOOLS_H
