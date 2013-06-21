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


SchedulerTask::SchedulerTask(Time time, const Function& function)
	: Task(function),
	  _id(0),
	  _time(time)
{}


auto SchedulerTask::create(Duration delay, const Function& function) -> SchedulerTaskP {
	if (delay <= Duration::zero()) {
		assert(delay > Duration::zero());
		delay = Duration::zero();
	}

	return std::make_shared<SchedulerTask>(Clock::now() + delay, function);
}


auto SchedulerTask::create(Time time, const Function& function) -> SchedulerTaskP {
	return std::make_shared<SchedulerTask>(time, function);
}


SchedulerTask::Id SchedulerTask::getId() const {
	return _id;
}


Time SchedulerTask::getTime() const {
	return _time;
}


void SchedulerTask::setId(Id id) {
	_id = id;
}
