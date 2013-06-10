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
#include "item/TrashHolderKind.hpp"

#include "item/Class.hpp"
#include "tools.h"


namespace ts { namespace item {

LOGGER_DEFINITION(ts::item::TrashHolderKind);



TrashHolderKind::TrashHolderKind(const ClassPC& clazz, KindId id)
	: Kind(std::static_pointer_cast<const Kind::ClassT>(clazz), id),
	  _trashEffect(MAGIC_EFFECT_NONE)
{}


bool TrashHolderKind::setParameter(const std::string& name, const std::string& value, xmlNodePtr deprecatedNode) {
	if (Kind::setParameter(name, value, deprecatedNode)) {
		return true;
	}

	if (name == "trashEffect" || name == "effect") {
		_checkDeprecatedParameter(name, "trashEffect");

		MagicEffect_t effect = getMagicEffect(value);
		if (effect != MAGIC_EFFECT_UNKNOWN) {
			_trashEffect = effect;
		}
		else {
			LOGe("Attribute '" << name << "' must be a magic effect (" << getMagicEffectNames() << ")");
		}
	}
	else {
		return false;
	}

	return true;
}

}} // namespace ts::item
