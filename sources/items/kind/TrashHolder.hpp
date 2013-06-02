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

#ifndef _ITEMS_KIND_TRASHHOLDER_HPP
#define _ITEMS_KIND_TRASHHOLDER_HPP

#include "items/Kind.hpp"


namespace ts {
namespace items {
namespace kind {

	class TrashHolder : public Kind {

	public:

		typedef const Class<TrashHolder>  ClassT;
		typedef std::shared_ptr<ClassT>   ClassP;


		TrashHolder(const ClassP& clazz);


	protected:

		virtual bool setParameter (const std::string& name, const std::string& value, xmlNodePtr deprecatedNode);


	private:

		LOGGER_DECLARATION;

		// 4 bytes
		MagicEffect_t _trashEffect;  // XML/effect

	};

} // namespace kind
} // namespace items
} // namespace ts

#endif // _ITEMS_KIND_TRASHHOLDER_HPP
