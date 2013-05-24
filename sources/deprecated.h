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

#ifndef _DEPRECATED_H
#define _DEPRECATED_H


class ReferenceCounted {

public:

    friend void intrusive_ptr_add_ref(const ReferenceCounted* object) {
        ++object->_referenceCount;
    }

    friend void intrusive_ptr_release(const ReferenceCounted* object) {
        if (--object->_referenceCount == 0) {
            delete object;
        }
    }

protected:

    ReferenceCounted(): _referenceCount(0) {}
    ReferenceCounted(const ReferenceCounted&) : _referenceCount(0) {}
    ReferenceCounted& operator =(const ReferenceCounted&) { return *this; }
    virtual ~ReferenceCounted() {};

private:

    mutable uint32_t _referenceCount;

};

#endif // _DEPRECATED_H
