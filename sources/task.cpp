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

#include "task.h"


Task::Task(const Function& function)
	: _expiration(TimePoint::max()),
	  _function(function)
{}


Task::Task(Duration duration, const Function& function)
	: _expiration(TimePoint::clock::now() + duration),
	  _function(function)
{}


Task::~Task() {
}


Task::UniquePointer Task::create(const Function& function) {
	return UniquePointer(new Task(function));
}


Task::UniquePointer Task::create(Duration duration, const Function& function) {
	return UniquePointer(new Task(duration, function));
}


Task::TimePoint Task::getExpiration() const {
	return _expiration;
}


void Task::setExpiration(TimePoint expiration) {
	_expiration = expiration;
}


void Task::operator()() const {
	_function();
}
