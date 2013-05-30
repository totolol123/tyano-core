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

#ifndef _ACCOUNT_H
#define _ACCOUNT_H

class Account;

typedef std::shared_ptr<Account>  AccountP;


class Account {

public:

	class Character {

	public:

		const std::string& getName () const;
		const std::string& getType () const;
		void               setName (const std::string& name);
		void               setType (const std::string& type);


	private:

	#ifdef __LOGIN_SERVER__
		GameServer* _server;
	#endif

		std::string _name;
		std::string _type;

	};


	typedef std::unique_ptr<Character>  CharacterP;
	typedef std::vector<CharacterP>     Characters;


	explicit Account(uint32_t id);
	~Account();

	Characters&        getCharacters  ();
	const Characters&  getCharacters  () const;
	uint32_t           getId          () const;
	uint32_t           getLastDay     () const;
	const std::string& getName        () const;
	const std::string& getPassword    () const;
	uint16_t           getPremiumDays () const;
	const std::string& getRecoveryKey () const;
	uint32_t           getWarnings    () const;
	void               setLastDay     (uint32_t lastDay);
	void               setName        (const std::string& name);
	void               setPassword    (const std::string& password);
	void               setPremiumDays (uint16_t premiumDays);
	void               setRecoveryKey (const std::string& recoveryKey);
	void               setWarnings    (uint32_t warnings);


private:

	Account(const Account& account) = delete;
	Account(Account&& account) = delete;

	Characters     _characters;
	const uint32_t _id;
	uint32_t       _lastDay;
	std::string    _name;
	std::string    _password;
	uint16_t       _premiumDays;
	std::string    _recoveryKey;
	uint32_t       _warnings;

};

#endif // _ACCOUNT_H
