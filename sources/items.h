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

#ifndef _ITEMS_H
#define _ITEMS_H

#include "const.h"
#include "item/Class.hpp"
#include "item/Kind.hpp"

#define SLOTP_WHEREEVER 0xFFFFFFFF
#define SLOTP_HEAD 1 << 0
#define	SLOTP_NECKLACE 1 << 1
#define	SLOTP_BACKPACK 1 << 2
#define	SLOTP_ARMOR 1 << 3
#define	SLOTP_RIGHT 1 << 4
#define	SLOTP_LEFT 1 << 5
#define	SLOTP_LEGS 1 << 6
#define	SLOTP_FEET 1 << 7
#define	SLOTP_RING 1 << 8
#define	SLOTP_AMMO 1 << 9
#define	SLOTP_DEPOT 1 << 10
#define	SLOTP_TWO_HAND 1 << 11
#define SLOTP_HAND SLOTP_LEFT | SLOTP_RIGHT


struct RandomizationBlock {
	ts::item::KindId fromRange;
	ts::item::KindId toRange;
	uint8_t          chance;
};


class Items {

public:

	typedef ts::item::ClientKindId    ClientKindId;
	typedef ts::item::Kind            Kind;
	typedef ts::item::KindId          KindId;
	typedef ts::item::KindP           KindP;
	typedef ts::item::KindPC          KindPC;
	typedef std::vector<KindP>        Kinds;
	typedef Kinds::const_iterator     const_iterator;
	typedef std::map<int32_t,KindId>  WorthMap;  // <worth,kindId>


	Items();

	const_iterator  begin               () const;
	const_iterator  end                 () const;
	KindPC          getKind             (KindId kindId) const;
	KindPC          getKindByClientId   (ClientKindId clientId) const;
	KindPC          getKindByName       (const std::string& name) const;
	KindId          getKindIdByName     (const std::string& name) const;
	KindP           getMutableKind      (KindId kindId) const;
	KindId          getRandomizedKindId (KindId kindId) const;
	uint32_t        getVersionBuild     () const;
	uint32_t        getVersionMajor     () const;
	uint32_t        getVersionMinor     () const;
	const WorthMap& getWorth            () const;
	void            loadKindFromXmlNode (xmlNode& node);
	bool            reload              ();

	KindPC operator [] (KindId kindId) const;


private:



	typedef ts::item::ClassP                        ClassP;
	typedef std::unordered_map<std::string,ClassP>  Classes;
	typedef std::map<ClientKindId,KindId>           KindIdsByClientIds;
	typedef std::map<KindId,RandomizationBlock>     RandomizationMap;


	void addClass                 (const ClassP& clazz);
	void addKind                  (KindP kind);
	void addRandomization         (KindId kindId, KindId fromId, KindId toId, uint8_t chance);
	void clear                    ();
	bool loadKindsFromOtb         ();
	bool loadKindsFromXml         ();
	bool loadRandomizationFromXml ();
	void setupClasses             ();


	LOGGER_DECLARATION;

	Classes            _classes;
	uint8_t            _defaultRandomizationChance;
	KindIdsByClientIds _kindIdsByClientId;
	Kinds              _kinds;
	RandomizationMap   _randomizations;
	uint32_t           _versionBuild;
	uint32_t           _versionMajor;
	uint32_t           _versionMinor;
	WorthMap           _worth;

};

#endif // _ITEMS_H
