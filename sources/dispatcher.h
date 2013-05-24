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

#ifndef _DISPATCHER_H
#define _DISPATCHER_H

class Game;
class OutputMessagePool;
class Task;


class Dispatcher {

public:

	enum class State {
		STOPPED,
		STARTED,
		STOPPING,
	};

	typedef std::unique_ptr<Task>  TaskP;


	Dispatcher();
	~Dispatcher();

	void  addTask         (TaskP task, bool urgent = false);
	State getState        ();
	void  start           ();
	void  stop            ();
	void  waitUntilStopped();


private:

	typedef std::deque<TaskP>  TaskDeque;


	void runTask (const Task& task, Game& game, OutputMessagePool* messagePool) const;
	void runTasks(const TaskDeque& tasks) const;
	void thread  ();


	LOGGER_DECLARATION;

	std::mutex              _mutex;
	std::condition_variable _signal;
	volatile State          _state;
	TaskDeque               _tasks;
	std::thread             _thread;

};

#endif // _DISPATCHER_H
