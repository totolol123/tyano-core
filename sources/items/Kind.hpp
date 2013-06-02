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

#ifndef _ITEMS_KIND_HPP
#define _ITEMS_KIND_HPP

#include "const.h"
#include "combat/Combat.hpp"
#include "creatures/Player.hpp"


namespace ts {
namespace items {

	template<typename _Kind>
	class Class;


	enum class FloorChange : uint8_t {
		NONE = 0,
		DOWN,
		NORTH,
		EAST,
		SOUTH,
		WEST,
		NORTH_EX,
		EAST_EX,
		SOUTH_EX,
		WEST_EX,
	};


	enum class HangableType : uint8_t {
		NONE = 0,
		HANGING,
		HORIZONTAL_WALL,
		VERTICAL_WALL,
	};



	class Kind {

	public:

		typedef const Class<Kind>       ClassT;
		typedef std::shared_ptr<ClassT> ClassP;


		virtual ~Kind();

		void setId (uint16_t id);


	protected:

		typedef creatures::Attribute  Attribute;
		typedef combat::DamageType    DamageType;


		Kind(const ClassP& clazz);

		virtual bool assemble                  ();
		        void _checkDeprecatedParameter (const std::string& name, const char* newName) const;
		virtual bool setParameter              (const std::string& name, const std::string& value, xmlNodePtr deprecatedNode);
		virtual bool setParameter              (const std::string& name, xmlNode& node);

		template<typename T>
		typename std::enable_if<std::is_same<T,bool>::value,bool>::type
		_setParameter (const std::string& name, const std::string& value, T& target, const char* newName = nullptr);

		template<typename T>
		typename std::enable_if<std::is_same<T,std::string>::value,bool>::type
		_setParameter (const std::string& name, const std::string& value, T& target, const char* newName = nullptr);

		template<typename T>
		typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T,bool>::value,bool>::type
		_setParameter (const std::string& name, const std::string& value, T& target, const char* newName = nullptr);

		template<typename T>
		typename std::enable_if<std::is_arithmetic<T>::value,bool>::type
		_setParameter (const std::string& name, const std::string& value, T& target, T min, T max, const char* newName = nullptr);


	private:

		typedef std::unordered_map<DamageType,uint8_t>     Absorptions;
		typedef std::unordered_map<Attribute,uint16_t>     Attributes;
		typedef std::pair<uint8_t,uint8_t>                 Reflection;  // <probability,reflection%>
		typedef std::unordered_map<DamageType,Reflection>  Reflections;


		Kind(const Kind&) = delete;
		Kind(Kind&&) = delete;

		bool _setAttributeParameter        (const std::string& name, const std::string& value, Attribute attribute, const char* newName = nullptr);
        void _setAbsorptions               (const std::string& name, xmlNode& node);
		void _setAttributes                (const std::string& name, xmlNode& node);
		void _setFloorChanges              (const std::string& name, xmlNode& node);
		void _setProtectsAgainstConditions (const std::string& name, xmlNode& node);
		void _setReflections               (const std::string& name, xmlNode& node);


		LOGGER_DECLARATION;

		// 4+ bytes
		Absorptions  _absorptions;                // XML/absorb*
		Attributes   _attributes;                 // XML/skill*, XML/maxhealthinc, XML/maxmanainc, etc.
		int32_t      _armor;                      // XML/armor
		const ClassP _class;                      // OTB/<node-type>, XML/<type>
		int32_t      _defense;                    // XML/defense
		int32_t      _defenseBonus;               // XML/extradefense
		std::string  _description;                // XML/description
		uint32_t     _healthRegeneration;         // XML/healthgain
		uint32_t     _healthRegenerationInterval; // XML/healthticks
		uint32_t     _lifetime;                   // XML/duration
		int32_t      _lightColor;                 // OTB/ITEM_ATTR_LIGHT2
		int32_t      _lightLevel;                 // OTB/ITEM_ATTR_LIGHT2
		uint32_t     _manaRegeneration;           // XML/healthgain
		uint32_t     _manaRegenerationInterval;   // XML/healthticks
		std::string  _name;                       // XML/name
		std::string  _nameArticle;                // XML/article
		std::string  _pluralName;                 // XML/name
		Reflections  _reflections;                // XML/reflect*
		int32_t      _requiredLevel;              // rune XML/maglvl
		int32_t      _requiredMagicLevel;         // rune XML/lvl
		std::string  _runeSpellName;              // XML/runespellname
		int32_t      _speed;                      // XML/speed
		std::string  _vocations;                  // movement XML
		float        _weight;                     // XML/weight
		uint32_t     _wieldInfo;                  // movement XML
		uint32_t     _worth;                      // XML/worth

		// 2 bytes
		uint16_t _charges;                     // XML/charges
		uint16_t _clientId;                    // OTB/ITEM_ATTR_CLIENTID
		uint16_t _decayedItemId;               // XML/decayto
		uint16_t _equipmentCounterpartItemId;  // XML/transformEquipTo, XML/transformDeEquipTo
		uint16_t _groundSpeed;                 // OTB/ITEM_ATTR_SPEED
		uint16_t _id;                          // OTH/ITEM_ATTR_SERVERID, XML/id
		uint16_t _maximumTextLength;           // XML/maxtextlen
		uint16_t _rotatedItemId;               // XML/rotateto
		uint16_t _writtenCounterpartItemId;    // XML/writeonceitemid

		// 1 byte
		FloorChange  _floorChange0;  // XML/floorchange
		FloorChange  _floorChange1;  // XML/floorchange
		HangableType _hangableType;  // OTB/FLAG_HANGABLE, OTB/FLAG_HORIZONTAL, OTB/FLAG_VERTICAL

		// < 1 byte or packed
		bool            _blocksPaths;                    // OTB/FLAG_BLOCK_PATHFIND
		bool            _blocksProjectiles;              // OTB/FLAG_BLOCK_PROJECTILE
		bool            _blocksSolids;                   // OTB/FLAG_BLOCK_SOLID
		bool            _blocksSolidsExceptPocketables;  // XML/allowpickupable
		FluidTypes_t    _containedFluid;                 // XML/fluidsource
		bool            _descriptionContainsAttributes;  // XML/showattributes
		bool            _descriptionContainsCharges;     // XML/showcharges
		bool            _descriptionContainsLifetime;    // XML/showduration
		bool            _descriptionContainsText;        // OTB/FLAG_ALLOWDISTREAD
		bool            _elevated;                       // OTB/FLAG_HAS_HEIGHT
		slots_t         _equipmentSlot;                  // XML/slottype
		bool            _grantsInvisibility;             // XML/invisibe
		bool            _grantsManaShield;               // XML/manashield
		bool            _grantsRegeneration;             // XML/regeneration
		bool            _movable;                        // OTB/FLAG_MOVEABLE, XML/movable
		bool            _opaque;                         // !OTB/FLAG_LOOKTHROUGH
		uint8_t         _pileOrder;                      // OTB/ITEM_ATTR_TOPORDER
		bool            _pocketable;                     // OTB/FLAG_PICKUPABLE
		bool            _persistent;                     // XML/forceserialize
		bool            _preventsEquipmentLossOnDeath;   // XML/preventdrop
		bool            _preventsSkillLossOnDeath;       // XML/preventloss
		ConditionType_t _protectsAgainstConditions;      // XML/suppress*
		RaceType_t      _race;                           // XML/corpsetype
		bool            _readable;                       // OTB/FLAG_READABLE, XML/readable
		bool            _stackable;                      // OTB/FLAG_STACKABLE
		bool            _usable;                         // OTB/FLAG_USEABLE
		bool            _writable;                       // OTB/FLAG_WRITABLE, XML/writable


		friend class Class<Kind>;

	};

//
//	template<>
//	bool Kind::_setParameter<bool> (const std::string& name, const std::string& value, bool& target, const char* newName);
//
//	template<>
//	bool Kind::_setParameter<std::string> (const std::string& name, const std::string& value, std::string& target, const char* newName);

} // namespace items
} // namespace ts

#endif // _ITEMS_KIND_HPP
