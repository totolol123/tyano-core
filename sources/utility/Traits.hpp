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

#ifndef _UTILITY_TRAITS_HPP
#define _UTILITY_TRAITS_HPP

namespace ts {

	template <typename T>
	using static_negate = std::integral_constant<bool, !T::value>;


	template<typename Base, typename... Args>
	struct first_convertible;

	template<typename Base>
	struct first_convertible<Base> {
		typedef std::true_type  empty;
	};

	template<typename Base, typename Class>
	struct first_convertible<Base, Class> {
		typedef std::is_convertible<Class*, Base*>                   _matches;
		typedef static_negate<_matches>                              empty;
		typedef typename std::enable_if<!empty::value, Class>::type  type;
	};

	template<typename Base, typename Class, typename... Args>
	struct first_convertible<Base, Class, Args...> {
		typedef std::is_convertible<Class*, Base*>                                             _matches;
		typedef first_convertible<Base, Args...>                                               _next;
		typedef typename _next::empty                                                          _nextEmpty;
		typedef typename _next::type                                                           _nextType;
		typedef typename std::conditional<_matches::value, std::false_type, _nextEmpty>::type  empty;
		typedef typename std::conditional<_matches::value, Class, _nextType>::type             type;
	};

	template<typename Base, typename... Args>
	struct first_convertible<Base, std::tr2::__reflection_typelist<Args...>> : first_convertible<Base, Args...> {
	};


	template<typename Class, typename Root>
	struct parallel_hierarchy {
		typedef typename std::tr2::direct_bases<Class>::type                                           _subclasses;
		typedef first_convertible<Root, _subclasses>                                                   _possibleBase;
		typedef typename _possibleBase::type                                                           _possibleBaseType;
		typedef typename std::conditional<_possibleBase::empty::value, Root, _possibleBaseType>::type  type;
	};

} // namespace ts

#endif // _UTILITY_TRAITS_HPP
