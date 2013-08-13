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

#ifndef _IDMAP_H
#define _IDMAP_H

template<class T>
class IdMap {

public:

	using Id  = uint32_t;
	using Map = std::unordered_map<Id,T>;


	IdMap(Id minimumId, Id maximumId)
		: _maximumId(maximumId),
		  _minimumId(minimumId),
		  _nextId(minimumId)
	{
		assert(minimumId <= maximumId);
	}


	void erase(Id id) {
		_map.erase(id);
	}


	T get(Id id) const {
		auto i = _map.find(id);
		if (i == _map.end()) {
			return T();
		}

		return i->second;
	}


	Id insert(T value) {
		auto id = findNextId();
		_map[id] = value;
		return id;
	}


private:

	IdMap(const IdMap&) = delete;
	IdMap(IdMap&&) = delete;


	Id findNextId() {
		auto startId = _nextId;

		auto id = startId;
		while (_map.find(id) != _map.end()) {
			if (id >= _maximumId) {
				id = _minimumId;
			}
			else {
				++id;
			}

			// If you hit this assertion you are out of IDs. Something is really wrong then!
			assert(id != startId);
			return 0;
		}

		_nextId = (id >= _maximumId ? _minimumId : (id + 1));

		return id;
	}


	LOGGER_DECLARATION;

	Map _map;
	Id  _maximumId;
	Id  _minimumId;
	Id  _nextId;

};

#endif // _IDMAP_H
