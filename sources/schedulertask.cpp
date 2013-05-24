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

#include "schedulertask.h"


SchedulerTask::SchedulerTask(Duration delay, const Function& function)
	: Task(function),
	  _id(0),
	  _time(TimePoint::clock::now() + delay)
{}


SchedulerTask::UniquePointer SchedulerTask::create(Duration delay, const Function& function) {
	if (delay <= Duration::zero()) {
		assert(delay > Duration::zero());
		delay = Duration::zero();
	}

	return UniquePointer(new SchedulerTask(delay, function));
}


SchedulerTask::Id SchedulerTask::getId() const {
	return _id;
}


SchedulerTask::TimePoint SchedulerTask::getTime() const {
	return _time;
}


void SchedulerTask::setId(Id id) {
	_id = id;
}
