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

#ifndef _SCHEDULER_H
#define _SCHEDULER_H

class SchedulerTask;


class Scheduler {

public:

	enum class State {
		STOPPED,
		STARTED,
		STOPPING,
	};

	typedef uint32_t                        TaskId;
	typedef std::unique_ptr<SchedulerTask>  TaskP;


	Scheduler();
	virtual ~Scheduler();

	TaskId addTask         (TaskP task);
	bool   cancelTask      (TaskId taskId);
	State  getState        ();
	void   start           ();
	void   stop            ();
	void   waitUntilStopped();


private:

	class TaskQueueCompare : public std::binary_function<const TaskP&, const TaskP&, bool> {
	public:
		inline bool operator() (const TaskP& a, const TaskP& b);
	};

	typedef std::unordered_set<TaskId>                              TaskIdSet;
	typedef std::vector<TaskP>                                      TaskVector;
	typedef std::priority_queue<TaskP,TaskVector,TaskQueueCompare>  TaskQueue;


	void thread();


	LOGGER_DECLARATION;

	TaskId                  _lastUsedTaskId;
	std::mutex              _mutex;
	TaskIdSet               _pendingTaskIds;
	std::condition_variable _signal;
	volatile State          _state;
	TaskQueue               _tasks;
	std::thread             _thread;

};

#endif // _SCHEDULER_H
