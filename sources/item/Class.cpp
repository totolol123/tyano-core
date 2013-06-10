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
#include "item/Class.hpp"

#include "attributes/Scheme.hpp"
#include "item/Kind.hpp"
#include "xml/Xml.hpp"


namespace ts { namespace item {

LOGGER_DEFINITION(ts::item::Class);



Class::Class(const std::string& id, const std::string& name, const DynamicAttributeSchemeP& dynamicAttributeScheme)
	: _dynamicAttributeScheme(dynamicAttributeScheme),
	  _id(id),
	  _name(name)
{
}


Class::~Class() {
}


Class::DynamicAttributeSchemeP Class::getDynamicAttributeScheme() const {
	return _dynamicAttributeScheme;
}


const std::string& Class::getName() const {
	return _name;
}


Class::KindP Class::createKindFromXmlNode(xmlNode& node) const {
#define error(x)    LOGe(x << " in " << node.doc->name << ":" << node.line)
#define warning(x)  LOGw(x << " in " << node.doc->name << ":" << node.line)

	if (xmlStrcmp(node.name, reinterpret_cast<const xmlChar*>(_id.c_str())) != 0) {
		error("Expected a node named <" << _id << ">");
		return nullptr;
	}

	if (!xml::hasAttribute(node, "id")) {
		error("Missing attribute 'id'");
		return nullptr;
	}

	KindId id;
	if (!xml::attribute(node, "id", id)) {
		error("Invalid item id. Value must be a number in range 1.." << std::numeric_limits<KindId>::max());
		return nullptr;
	}

	KindP kind = createKind(id);

	for (xmlAttrPtr attribute = node.properties; attribute != nullptr; attribute = attribute->next) {
		if (xmlStrcmp(node.name, reinterpret_cast<const xmlChar*>("id")) == 0) {
			continue;
		}

		std::string name(reinterpret_cast<const char*>(attribute->name));

		std::string value;
		if (!xml::attribute(*attribute, value)) {
			error("Attribute '" << name << "' has no value");
			continue;
		}

		processKindParameter(*kind, name, value);
	}

	for (xmlNodePtr parameterNode = node.children; parameterNode != nullptr; parameterNode = parameterNode->next) {
		if (parameterNode->type != XML_ELEMENT_NODE) {
			continue;
		}

		std::string name = xml::name(*parameterNode);
		if (name == "attribute") {
			if (!xml::attribute(*parameterNode, "key", name)) {
				error("Defect <attribute> node detected");
				continue;
			}

			std::string value;
			if (!xml::attribute(*parameterNode, "value", value)) {
				error("Attribute '" << name << "' has no value");
				continue;
			}

			warning("Attribute '" << name << "' uses deprecated node-style format <attribute key=\"" << name << "\" value=\"...\"/> instead of the new format '" << name << "=\"...\"'");

			processKindParameter(*kind, name, value, parameterNode);
			continue;
		}

		kind->setParameter(name, *parameterNode);
	}

	if (!kind->assemble()) {
		return nullptr;
	}

	return kind;
/*

	for (xmlNodePtr node = root->children; node != nullptr; node = node.next) {
		if (node.type != XML_ELEMENT_NODE) {
			continue;
		}

		if(readXMLString(node, "key", strValue))
		{
			std::string tmpStrValue = asLowerCaseString(strValue);
			if(tmpStrValue == "type")
			{
				if(readXMLString(node, "value", strValue))
				{
					tmpStrValue = asLowerCaseString(strValue);
					if(tmpStrValue == "container")
					{
						kind->type = ItemType::CONTAINER;
						kind->group = ITEM_GROUP_CONTAINER;
					}
					else if(tmpStrValue == "key")
						kind->type = ItemType::KEY;
					else if(tmpStrValue == "magicfield")
						kind->type = ItemType::MAGICFIELD;
					else if(tmpStrValue == "depot")
						kind->type = ItemType::DEPOT;
					else if(tmpStrValue == "mailbox")
						kind->type = ItemType::MAILBOX;
					else if(tmpStrValue == "trashholder")
						kind->type = ItemType::TRASHHOLDER;
					else if(tmpStrValue == "teleport")
						kind->type = ItemType::TELEPORT;
					else if(tmpStrValue == "door")
						kind->type = ItemType::DOOR;
					else if(tmpStrValue == "bed")
						kind->type = ItemType::BED;
					else {
						LOGe("<item> <kind> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");
						continue;
					}

					auto clazzIt = _classes.find(kind->type);
					if (clazzIt == _classes.cend()) {
						LOGe("Cannot update item kind of unsupported type " << static_cast<uint32_t>(kind->type));
						continue;
					}

					kind->_class = clazzIt->second;
				}
			}
			else if(tmpStrValue == "field")
			{
				kind->group = ITEM_GROUP_MAGICFIELD;
				kind->type = ItemType::MAGICFIELD;
				CombatType_t combatType = COMBAT_NONE;
				ConditionDamage* conditionDamage = nullptr;

				if(readXMLString(node, "value", strValue))
				{
					tmpStrValue = asLowerCaseString(strValue);
					if(tmpStrValue == "fire")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_FIRE, false, 0);
						combatType = COMBAT_FIREDAMAGE;
					}
					else if(tmpStrValue == "energy")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_ENERGY, false, 0);
						combatType = COMBAT_ENERGYDAMAGE;
					}
					else if(tmpStrValue == "earth" || tmpStrValue == "poison")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_POISON, false, 0);
						combatType = COMBAT_EARTHDAMAGE;
					}
					else if(tmpStrValue == "ice" || tmpStrValue == "freezing")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_FREEZING, false, 0);
						combatType = COMBAT_ICEDAMAGE;
					}
					else if(tmpStrValue == "holy" || tmpStrValue == "dazzled")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_DAZZLED, false, 0);
						combatType = COMBAT_HOLYDAMAGE;
					}
					else if(tmpStrValue == "death" || tmpStrValue == "cursed")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_CURSED, false, 0);
						combatType = COMBAT_DEATHDAMAGE;
					}
					else if(tmpStrValue == "drown")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_DROWN, false, 0);
						combatType = COMBAT_DROWNDAMAGE;
					}
					else if(tmpStrValue == "physical")
					{
						conditionDamage = new ConditionDamage(CONDITIONID_COMBAT, CONDITION_PHYSICAL, false, 0);
						combatType = COMBAT_PHYSICALDAMAGE;
					}
					else
						LOGw("<item> <field> node has unknown value '" << strValue << "' for item " << kind->id << " in " << filePath << ".");

					if(combatType != COMBAT_NONE)
					{
						kind->combatType = combatType;
						kind->condition = conditionDamage;
						uint32_t ticks = 0;
						int32_t damage = 0, start = 0, count = 1;

						xmlNodePtr fieldAttributesNode = node.children;
						while(fieldAttributesNode)
						{
							if(readXMLString(fieldAttributesNode, "key", strValue))
							{
								tmpStrValue = asLowerCaseString(strValue);
								if(tmpStrValue == "ticks")
								{
									if(readXMLInteger(fieldAttributesNode, "value", intValue))
										ticks = std::max(0, intValue);
								}

								if(tmpStrValue == "count")
								{
									if(readXMLInteger(fieldAttributesNode, "value", intValue))
										count = std::max(1, intValue);
								}

								if(tmpStrValue == "start")
								{
									if(readXMLInteger(fieldAttributesNode, "value", intValue))
										start = std::max(0, intValue);
								}

								if(tmpStrValue == "damage")
								{
									if(readXMLInteger(fieldAttributesNode, "value", intValue))
									{
										damage = -intValue;
										if(start > 0)
										{
											std::list<int32_t> damageList;
											ConditionDamage::generateDamageList(damage, start, damageList);

											for(std::list<int32_t>::iterator it = damageList.begin(); it != damageList.end(); ++it)
												conditionDamage->addDamage(1, ticks, -*it);

											start = 0;
										}
										else
											conditionDamage->addDamage(count, ticks, damage);
									}
								}
							}

							fieldAttributesNode = fieldAttributesnode.next;
						}

						if(conditionDamage->getTotalDamage() > 0)
							kind->condition->setParam(CONDITIONPARAM_FORCEUPDATE, true);
					}
				}
			}
		}
	}

	if(kind->pluralName.empty() && !kind->name.empty())
	{
		kind->pluralName = (kind->showCount ? (kind->name + "s") : kind->name);
	}
}


bool Items::loadKindsFromOtb() {
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

	return true;
}


bool Items::loadKindsFromXml() {
	std::string filePath = getFilePath(FileType::OTHER, "items/items.xml");

	xmlDocP document(xmlParseFile(filePath.c_str()));
	if (!document) {
		LOGe("Cannot load items file " << filePath << ": " << getLastXMLError());
		return false;
	}

	xmlNodePtr root = xmlDocGetRootElement(document.get());
	if (xmlStrcmp(root->name, reinterpret_cast<const xmlChar*>("items")) != 0) {
		LOGe("No <items> root node in " << filePath << ".");
		return false;
	}

	IntegerVec intVector, endVector;
	std::string strValue, endValue;
	StringVector strVector;
	int32_t intValue;

	for (xmlNodePtr node = root->children; node != nullptr; node = node.next) {
		if (node.type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(node.name, reinterpret_cast<const xmlChar*>("item")) != 0) {
			LOGw("Unexpected <" << reinterpret_cast<const char*>(node.name) << "> node (expected <item>) in " << filePath << ".");
			continue;
		}

		if (readXMLInteger(node, "id", intValue)) {
			loadKindFromXmlNode(node, intValue, filePath);
		}
		else if (readXMLString(node, "fromid", strValue) && readXMLString(node, "toid", endValue)) {
			intVector = vectorAtoi(explodeString(strValue, ";"));
			endVector = vectorAtoi(explodeString(endValue, ";"));
			if (intVector[0] && endVector[0] && intVector.size() == endVector.size()) {
				size_t size = intVector.size();
				for (size_t i = 0; i < size; ++i) {
					loadKindFromXmlNode(node, intVector[i], filePath);
					while (intVector[i] < endVector[i]) {
						loadKindFromXmlNode(node, ++intVector[i], filePath);
					}
				}
			}
			else {
				LOGw("Malformed <item> node with ID range from '" << strValue << "' to '" << endValue << "' in " << filePath << ".");
			}
		}
		else {
			LOGw("Cannot parse <item> node without ID in " << filePath << ".");
		}
	}

	for (ItemKindVector::const_iterator it = _kinds.begin(); it != _kinds.end(); ++it) {
		const ItemKindP& type = *it;
		if (!type) {
			continue;
		}

		if ((type->transformToFree || type->transformUseTo[PLAYERSEX_FEMALE] || type->transformUseTo[PLAYERSEX_MALE]) && type->type != ItemType::BED) {
			LOGw("Item " << type->id << " is not set as a bed-type in " << filePath << ".");
		}
	}

	return true;*/
}


void Class::processKindParameter(KindT& kind, const std::string& name, const std::string& value, xmlNodePtr deprecatedNode) const {
	if (!kind.setParameter(name, value, deprecatedNode)) {
		std::string lowercaseName(name);
		std::transform(lowercaseName.begin(), lowercaseName.end(), lowercaseName.begin(), ::tolower);
		if (!kind.setParameter(lowercaseName, value, deprecatedNode)) {
			LOGw("Unknown attribute '" << name << "'");
		}
	}
}

}} // namespace ts::item
