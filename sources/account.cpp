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
#include "account.h"


Account::Account(uint32_t id)
	: _id(id),
	  _lastDay(0),
	  _premiumDays(0),
	  _warnings(0)
{}


Account::~Account() {
}


Account::Characters& Account::getCharacters() {
	return _characters;
}


const Account::Characters& Account::getCharacters() const {
	return _characters;
}


uint32_t Account::getId() const {
	return _id;
}


uint32_t Account::getLastDay() const {
	return _lastDay;
}


const std::string& Account::getName() const {
	return _name;
}


const std::string& Account::getPassword() const {
	return _password;
}


uint16_t Account::getPremiumDays() const {
	return _premiumDays;
}


const std::string& Account::getRecoveryKey() const {
	return _recoveryKey;
}


uint32_t Account::getWarnings() const {
	return _warnings;
}


void Account::setLastDay(uint32_t lastDay) {
	_lastDay = lastDay;
}


void Account::setName(const std::string& name) {
	_name = name;
}


void Account::setPassword(const std::string& password) {
	_password = password;
}


void Account::setPremiumDays(uint16_t premiumDays) {
	_premiumDays = premiumDays;
}


void Account::setRecoveryKey(const std::string& recoveryKey) {
	_recoveryKey = recoveryKey;
}


void Account::setWarnings(uint32_t warnings) {
	_warnings = warnings;
}



const std::string& Account::Character::getName() const {
	return _name;
}


const std::string& Account::Character::getType() const {
	return _type;
}


void Account::Character::setName(const std::string& name) {
	_name = name;
}


void Account::Character::setType(const std::string& type) {
	_type = type;
}
