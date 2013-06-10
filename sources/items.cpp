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
#include "items.h"

#include "beds.h"
#include "configmanager.h"
#include "condition.h"
#include "depot.h"
#include "fileloader.h"
#include "house.h"
#include "item/BedKind.hpp"
#include "item/DepotKind.hpp"
#include "item/DoorKind.hpp"
#include "item/OtherKind.hpp"
#include "item/KeyKind.hpp"
#include "item/MagicFieldKind.hpp"
#include "item/MailboxKind.hpp"
#include "item/TeleportKind.hpp"
#include "item/TrashHolderKind.hpp"
#include "item/WeaponKind.hpp"
#include "xml/Xml.hpp"
#include "mailbox.h"
#include "server.h"
#include "spells.h"
#include "teleport.h"
#include "trashholder.h"
#include "weapons.h"

using namespace ts;
using namespace ts::item;


LOGGER_DEFINITION(Items);


Items::Items()
	: _defaultRandomizationChance(50),
	  _versionBuild(0),
	  _versionMajor(0),
	  _versionMinor(0)
{}


void Items::addClass(const ClassP& clazz) {
	assert(clazz != nullptr);

	_classes[clazz->getId()] = clazz;
}


void Items::addKind(KindP kind) {
	assert(kind != nullptr);

	KindId id = kind->getId();
	if (_kinds.size() <= id) {
		if (_kinds.capacity() <= id) {
			_kinds.reserve(id + 5000);
		}

		_kinds.resize(id + 1);
	}

	if (_kinds[id] != nullptr) {
		LOGw("Item " << id << " is defined multiple times.");
		return;
	}

	uint32_t clientId = kind->getClientId();
	if (clientId > 0) {
		if (_kindIdsByClientId.find(clientId) != _kindIdsByClientId.end()) {
//			LOGw("Multiple items have the same client ID " << kind->clientId << " in " << fileName << ".");
		}

		_kindIdsByClientId[clientId] = id;
	}

	_kinds[id] = kind;
}


void Items::addRandomization(KindId kindId, KindId fromId, KindId toId, uint8_t chance) {
	if (_randomizations.find(kindId) != _randomizations.end()) {
		LOGw("Randomization for item " << kindId << " is defined multiple times.");
	}

	RandomizationBlock& block = _randomizations[kindId];
	block.chance = chance;
	block.fromRange = fromId;
	block.toRange = toId;
}


Items::Kinds::const_iterator Items::begin() const {
	return _kinds.begin();
}


void Items::clear() {
	_classes.clear();
	_kindIdsByClientId.clear();
	_kinds.clear();
	_randomizations.clear();
	_worth.clear();

	_defaultRandomizationChance = 50;
	_versionBuild = 0;
	_versionMajor = 0;
	_versionMinor = 0;
}


Items::Kinds::const_iterator Items::end() const {
	return _kinds.end();
}


KindPC Items::getKind(KindId kindId) const {
	return getMutableKind(kindId);
}


KindPC Items::getKindByClientId(ClientKindId clientKindId) const {
	auto i = _kindIdsByClientId.find(clientKindId);
	if (i == _kindIdsByClientId.end()) {
		return nullptr;
	}

	return getKind(i->second);
}


KindPC Items::getKindByName(const std::string& name) const {
	if (name.empty()) {
		return nullptr;
	}

	for (auto kind : _kinds) {
		if (kind == nullptr) {
			continue;
		}

		if (boost::iequals(kind->getName(), name)) {
			return kind;
		}
	}

	return nullptr;
}


KindId Items::getKindIdByName(const std::string& name) const {
	auto kind = getKindByName(name);
	if (kind == nullptr) {
		return 0;
	}

	return kind->getId();
}


KindP Items::getMutableKind(KindId kindId) const {
	if (kindId == 0 || kindId >= _kinds.size()) {
		return nullptr;
	}

	return _kinds[kindId];
}


KindId Items::getRandomizedKindId(KindId kindId) const {
	if (!server.configManager().getBool(ConfigManager::RANDOMIZE_TILES)) {
		return kindId;
	}

	RandomizationMap::const_iterator it = _randomizations.find(kindId);
	if (it == _randomizations.end()) {
		return kindId;
	}

	const RandomizationBlock& block = it->second;
	if (block.chance > 0 && random_range(0, 100) <= block.chance) {
		kindId = random_range(block.fromRange, block.toRange);
	}

	return kindId;
}


uint32_t Items::getVersionBuild() const {
	return _versionBuild;
}


uint32_t Items::getVersionMajor() const {
	return _versionMajor;
}


uint32_t Items::getVersionMinor() const {
	return _versionMinor;
}


const Items::WorthMap& Items::getWorth() const {
	return _worth;
}


void Items::loadKindFromXmlNode(xmlNode& node) {
	std::string classId = xml::name(node);

	auto i = _classes.find(classId);
	if (i == _classes.end()) {
		LOGe("Unknown node <" << classId << ">.");
		return;
	}

	KindP kind = i->second->createKindFromXmlNode(node);
	if (kind == nullptr) {
		return;
	}

	addKind(kind);
}


bool Items::loadKindsFromOtb() {
	/*
	std::string filePath = getFilePath(FileType::OTHER, "items/items.otb");

	FileLoader loader;
	if (!loader.openFile(filePath.c_str(), false, true)) {
		return loader.getError();
	}

	PropStream properties;

	uint32_t nodeType;
	const NodeStruct* node = loader.getChildNode(NO_NODE, nodeType);

	if (loader.getProps(node, properties)) {
		uint32_t flags;
		if (!properties.GET_ULONG(flags)) {
			LOGe("Cannot load flags from " << filePath << ".");
			return false;
		}

		attribute_t attribute;
		if (!properties.GET_VALUE(attribute)) {
			LOGe("Cannot load attribute from " << filePath << ".");
			return false;
		}

		if (attribute == ROOT_ATTR_VERSION) {
			datasize_t length = 0;
			if (!properties.GET_VALUE(length)) {
				LOGe("Cannot load version attribute length from " << filePath << ".");
				return false;
			}
			if (length != sizeof(VERSIONINFO)) {
				LOGe("Invalid version attribute in " << filePath << ".");
				return false;
			}

			VERSIONINFO* version;
			if (!properties.GET_STRUCT(version)) {
				LOGe("Cannot load version attribute from " << filePath << ".");
				return false;
			}

			_versionBuild = version->dwBuildNumber;
			_versionMajor = version->dwMajorVersion; // otb format file version
			_versionMinor = version->dwMinorVersion; // client version
		}
	}

	if (_versionMajor == 0xFFFFFFFF) {
		LOGw("Generic file version used by " << filePath << ".");
	}
	else if (_versionMajor < 3) {
		LOGe("File format used by " << filePath << " is too old.");
		return false;
	}
	else if (_versionMajor > 3) {
		LOGe("File format used by " << filePath << " is too old.");
		return false;
	}

	_kinds.reserve(std::numeric_limits<uint16_t>::max());

	for (node = loader.getChildNode(node, nodeType); node != NO_NODE; node = loader.getNextNode(node, nodeType)) {
		if (!loader.getProps(node, properties)) {
			LOGe("Cannot load item node properties from " << filePath << ".");
			return false;
		}

		ItemKindP kind = std::make_shared<ItemKind>();
		kind->group = static_cast<itemgroup_t>(nodeType);

		flags_t flags;
		switch (nodeType) {
			case ITEM_GROUP_CONTAINER:
				kind->type = ItemType::CONTAINER;
				break;

			case ITEM_GROUP_DOOR: // unused
				kind->type = ItemType::DOOR;
				break;

			case ITEM_GROUP_MAGICFIELD: // unused
				kind->type = ItemType::MAGICFIELD;
				break;

			case ITEM_GROUP_TELEPORT: // unused
				kind->type = ItemType::TELEPORT;
				break;

			case ITEM_GROUP_NONE:
			case ITEM_GROUP_GROUND:
			case ITEM_GROUP_SPLASH:
			case ITEM_GROUP_FLUID:
			case ITEM_GROUP_CHARGES:
			case ITEM_GROUP_DEPRECATED:
				break;

			default:
				LOGe("Item node has unknown type " << nodeType << " in " << filePath << ".");
				return false;
		}

		if (!properties.GET_VALUE(flags)) {
			LOGe("Cannot load item flags from " << filePath << ".");
			return false;
		}

		kind->blockSolid = hasBitSet(FLAG_BLOCK_SOLID, flags);
		kind->blockProjectile = hasBitSet(FLAG_BLOCK_PROJECTILE, flags);
		kind->blockPathFind = hasBitSet(FLAG_BLOCK_PATHFIND, flags);
		kind->hasHeight = hasBitSet(FLAG_HAS_HEIGHT, flags);
		kind->useable = hasBitSet(FLAG_USEABLE, flags);
		kind->pickupable = hasBitSet(FLAG_PICKUPABLE, flags);
		kind->moveable = hasBitSet(FLAG_MOVEABLE, flags);
		kind->stackable = hasBitSet(FLAG_STACKABLE, flags);

		kind->alwaysOnTop = hasBitSet(FLAG_ALWAYSONTOP, flags);
		kind->isVertical = hasBitSet(FLAG_VERTICAL, flags);
		kind->isHorizontal = hasBitSet(FLAG_HORIZONTAL, flags);
		kind->isHangable = hasBitSet(FLAG_HANGABLE, flags);
		kind->allowDistRead = hasBitSet(FLAG_ALLOWDISTREAD, flags);
		kind->rotable = hasBitSet(FLAG_ROTABLE, flags);
		kind->canReadText = hasBitSet(FLAG_READABLE, flags);
		kind->clientCharges = hasBitSet(FLAG_CLIENTCHARGES, flags);
		kind->lookThrough = hasBitSet(FLAG_LOOKTHROUGH, flags);

		attribute_t attribute;
		while (properties.GET_VALUE(attribute)) {
			datasize_t length = 0;
			if (!properties.GET_VALUE(length)) {
				LOGe("Cannot load item attribute length from " << filePath << ".");
				return false;
			}

			switch (attribute) {
				case ITEM_ATTR_SERVERID: {
					if (length != sizeof(uint16_t)) {
						LOGe("Invalid item server ID attribute length in " << filePath << ".");
						return false;
					}

					uint16_t serverId;
					if (!properties.GET_USHORT(serverId)) {
						LOGe("Cannot load item server ID from " << filePath << ".");
						return false;
					}

					if (serverId > 20000 && serverId < 20100) {
						// TODO check this
						serverId -= 20000;
					}

					kind->id = serverId;
					break;
				}

				case ITEM_ATTR_CLIENTID: {
					if (length != sizeof(uint16_t)) {
						LOGe("Invalid item client ID attribute length in " << filePath << ".");
						return false;
					}

					uint16_t clientId;
					if (!properties.GET_USHORT(clientId)) {
						LOGe("Cannot load item client ID from " << filePath << ".");
						return false;
					}

					kind->clientId = clientId;
					break;
				}

				case ITEM_ATTR_SPEED: {
					if (length != sizeof(uint16_t)) {
						LOGe("Invalid item speed attribute length in " << filePath << ".");
						return false;
					}

					uint16_t speed;
					if (!properties.GET_USHORT(speed)) {
						LOGe("Cannot load item speed from " << filePath << ".");
						return false;
					}

					kind->speed = speed;
					break;
				}

				case ITEM_ATTR_LIGHT2: {
					if (length != sizeof(lightBlock2)) {
						LOGe("Invalid item light attribute length in " << filePath << ".");
						return false;
					}

					lightBlock2* block;
					if (!properties.GET_STRUCT(block)) {
						LOGe("Cannot load item light from " << filePath << ".");
						return false;
					}

					kind->lightLevel = block->lightLevel;
					kind->lightColor = block->lightColor;
					break;
				}

				case ITEM_ATTR_TOPORDER: {
					if (length != sizeof(uint8_t)) {
						LOGe("Invalid item always-on-top order attribute length in " << filePath << ".");
						return false;
					}

					uint8_t topOrder;
					if (!properties.GET_UCHAR(topOrder)) {
						LOGe("Cannot load item always-on-top order from " << filePath << ".");
						return false;
					}

					kind->alwaysOnTopOrder = topOrder;
					break;
				}

				case ITEM_ATTR_SPRITEHASH:
				case ITEM_ATTR_MINIMAPCOLOR:
				case ITEM_ATTR_07:
				case ITEM_ATTR_08:
					if (!properties.SKIP_N(length)) {
						LOGe("Cannot skip useless item attribute in " << filePath << ".");
						return false;
					}
					break;

				default: {
					if (!properties.SKIP_N(length)) {
						LOGe("Cannot skip unknown item attribute in " << filePath << ".");
						return false;
					}

					LOGw("Skipped unknown item attribute " << static_cast<uint32_t>(attribute) << " in " << filePath << ".");
					break;
				}
			}
		}

		auto clazzIt = _classes.find(kind->type);
		if (clazzIt == _classes.cend()) {
			LOGe("Cannot add item kind of unsupported type " << static_cast<uint32_t>(kind->type));
			continue;
		}

		kind->_class = clazzIt->second;

		addKind(kind, filePath);
	}

	_kinds.shrink_to_fit();

	*/
	return true;
}


bool Items::loadKindsFromXml() {
	_kinds.clear();
	_kinds.reserve(std::numeric_limits<KindId>::max());

	std::string filePath = getFilePath(FileType::OTHER, "items/items.xml");

	xmlDocP document(xmlParseFile(filePath.c_str()));
	if (document == nullptr) {
		LOGe("Cannot load items file " << filePath << ": " << getLastXMLError());
		return false;
	}

	xmlNode& root = *xmlDocGetRootElement(document.get());
	if (xmlStrcmp(root.name, reinterpret_cast<const xmlChar*>("items")) != 0) {
		LOGe("<items> must be the root node in " << filePath << ".");
		return false;
	}

	for (xmlNodePtr node = root.children; node != nullptr; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		loadKindFromXmlNode(*node);
	}

	_kinds.shrink_to_fit();

	return true;
}


bool Items::loadRandomizationFromXml() {
	std::string filePath = getFilePath(FileType::OTHER, "items/randomization.xml");

	xmlDocP document(xmlParseFile(filePath.c_str()));
	if (!document) {
		LOGe("Cannot load items file " << filePath << ": " << getLastXMLError());
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(document.get());
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("randomization")) != 0) {
		LOGe("No <randomization> root node in " << filePath << ".");
		return false;
	}

	IntegerVec intVector, endVector;
	std::string strValue, endValue;
	StringVector strVector;

	KindId kindId = 0;
	KindId endId = 0;
	KindId fromId = 0;
	KindId toId = 0;


	int32_t intValue, intValue2;

	for (xmlNodePtr node = root->children; node != nullptr; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("config")) == 0) {
			if (readXMLInteger(node, "chance", intValue) || readXMLInteger(node, "defaultChance", intValue)) {
				if (intValue > 100) {
					intValue = 100;
					LOGw("<config> node 'defaultChance' cannot be higher than 100 in " << filePath << ".");
				}

				_defaultRandomizationChance = intValue;
			}
		}
		else if (xmlStrcmp(node->name, reinterpret_cast<const xmlChar*>("palette")) == 0) {
			if (readXMLString(node, "randomize", strValue)) {
				std::vector<int32_t> itemList = vectorAtoi(explodeString(strValue, ";"));
				if (itemList.size() == 2 && itemList[0] < itemList[1]) {
					fromId = itemList[0];
					toId = itemList[1];
				}
				else {
					LOGw("<palette> node randomize value '" << strValue << "' must be in format 'fromId;toId' where fromId is less than toId in " << filePath << ".");
					continue;
				}

				int32_t chance = _defaultRandomizationChance;
				if (readXMLInteger(node, "chance", intValue)) {
					if (intValue > 100) {
						intValue = 100;
						LOGw("<palette> node 'chance' cannot be higher than 100 in " << filePath << ".");
					}

					chance = intValue;
				}

				if (readXMLInteger(node, "itemid", intValue)) {
					kindId = intValue;
					addRandomization(kindId, fromId, toId, chance);
				}
				else if (readXMLInteger(node, "fromid", intValue) && readXMLInteger(node, "toid", intValue2)) {
					fromId = intValue;
					toId = intValue2;

					addRandomization(kindId, fromId, toId, chance);
					while (kindId < endId) {
						addRandomization(++kindId, fromId, toId, chance);
					}
				}
				else {
					LOGw("<palette> nodes must specify either attribute 'itemid' or attributes 'fromid' and 'toid' in " << filePath << ".");
				}
			}
		}
		else {
			LOGw("Unexpected <" << reinterpret_cast<const char*>(node->name) << "> node (expected <config> or <palette>) in " << filePath << ".");
		}
	}

	return true;
}


bool Items::reload() {
	// TODO proper error handling
	// TODO reload move events & weapons?
	// TODO reuse previous ItemKinds

	clear();

	setupClasses();

	if (!loadKindsFromOtb()) {
		return false;
	}
	if (!loadKindsFromXml()) {
		return false;
	}
	if (!loadRandomizationFromXml()) {
		return false;
	}

	return true;
}


void Items::setupClasses() {
	addClass(ClassDescriptor<BedKind>::create());
	addClass(ClassDescriptor<ContainerKind>::create());
	addClass(ClassDescriptor<DepotKind>::create());
	addClass(ClassDescriptor<DoorKind>::create());
	addClass(ClassDescriptor<KeyKind>::create());
	addClass(ClassDescriptor<MagicFieldKind>::create());
	addClass(ClassDescriptor<MailboxKind>::create());
	addClass(ClassDescriptor<OtherKind>::create());
	addClass(ClassDescriptor<TeleportKind>::create());
	addClass(ClassDescriptor<TrashHolderKind>::create());
	addClass(ClassDescriptor<WeaponKind>::create());
}


KindPC Items::operator [] (KindId kindId) const {
	return getKind(kindId);
}
