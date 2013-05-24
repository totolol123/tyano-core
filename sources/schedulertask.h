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

#ifndef _SCHEDULERTASK_H
#define _SCHEDULERTASK_H

#include "task.h"


class SchedulerTask : public Task {

public:

	typedef uint32_t                        Id;
	typedef std::unique_ptr<SchedulerTask>  UniquePointer;


	static UniquePointer create(Duration delay, const Function& function);

	Id        getId   () const;
	TimePoint getTime () const;
	void      setId   (Id id);


private:

	SchedulerTask(Duration delay, const Function& function);


	Id        _id;
	TimePoint _time;

};

#endif // _SCHEDULERTASK_H
