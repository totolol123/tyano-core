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


const RealTime Account::PREMIUM_NONE = RealTime::min();
const RealTime Account::PREMIUM_UNLIMITED = RealTime::max();



Account::Account(uint32_t id)
	: _id(id),
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


const std::string& Account::getName() const {
	return _name;
}


const std::string& Account::getPassword() const {
	return _password;
}


Days Account::getPremiumDays() const {
	if (_premiumExpiration == PREMIUM_NONE) {
		return Days::zero();
	}
	if (_premiumExpiration == PREMIUM_UNLIMITED) {
		return Days::max();
	}

	RealDuration premiumDuration = _premiumExpiration - RealClock::now();
	if (premiumDuration < RealDuration::zero()) {
		return Days::zero();
	}

	Days premiumDays = std::chrono::duration_cast<Days>(premiumDuration);
	if (premiumDays < premiumDuration) {
		// round up, not down
		++premiumDays;
	}

	if (premiumDays < Days::zero()) {
		return Days::zero();
	}

	return premiumDays;
}


RealTime Account::getPremiumExpiration() const {
	return _premiumExpiration;
}


const std::string& Account::getRecoveryKey() const {
	return _recoveryKey;
}


uint32_t Account::getWarnings() const {
	return _warnings;
}


bool Account::hasPremium() const {
	return (_premiumExpiration >= RealClock::now());
}


bool Account::hasUnlimitedPremium() const {
	return (_premiumExpiration == PREMIUM_UNLIMITED);
}


void Account::setName(const std::string& name) {
	_name = name;
}


void Account::setPassword(const std::string& password) {
	_password = password;
}


void Account::setPremiumExpiration(RealTime premiumExpiration) {
	_premiumExpiration = premiumExpiration;
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
