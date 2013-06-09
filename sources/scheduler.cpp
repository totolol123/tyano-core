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

#include "scheduler.h"

#include "dispatcher.h"
#include "schedulertask.h"
#include "server.h"


LOGGER_DEFINITION(Scheduler);


Scheduler::Scheduler()
	: _lastUsedTaskId(0),
	  _state(State::STOPPED)
{}


Scheduler::~Scheduler() {
	auto locked = _mutex.try_lock();
	if (!locked) {
		LOGe("Scheduler deleted while in use.");
		assert(locked);

		_mutex.lock();
	}

	if (_state != State::STOPPED) {
		LOGe("Scheduler deleted but not yet stopped. Forgot to call waitUntilStopped()?");
		assert(_state == State::STOPPED);

		if (std::this_thread::get_id() != _thread.get_id()) {
			if (_state == State::STARTED) {
				stop();
			}

			waitUntilStopped();
		}
		else {
			LOGe("Cannot delete the scheduler from within the scheduler thread.");
		}
	}

	_mutex.unlock();
}


Scheduler::TaskId Scheduler::addTask(TaskP task) {
	if (task == nullptr) {
		assert(task != nullptr);
		return 0;
	}

	std::lock_guard<std::mutex> lock(_mutex);

	switch (_state) {
	case State::STOPPED:
	case State::STARTED: {
		TaskId taskId = task->getId();
		if (taskId == 0) {
			if (_lastUsedTaskId == std::numeric_limits<TaskId>::max()) {
				taskId = _lastUsedTaskId = 1;
			}
			else {
				taskId = ++_lastUsedTaskId;
			}

			task->setId(taskId);
		}

		_pendingTaskIds.insert(taskId);
		_tasks.push(std::move(task));

		if (_tasks.top()->getId() == taskId) {
			_signal.notify_one();
		}

		return taskId;
	}

	case State::STOPPING:
		LOGt("Cannot add task to scheduler which about to stop.");
		return 0;
	}

	return 0;
}


bool Scheduler::cancelTask(TaskId taskId) {
	std::lock_guard<std::mutex> lock(_mutex);

	if (_state != State::STARTED) {
		LOGt("Cannot cancel task in scheduler which is not started.");
		return false;
	}

	if (taskId == 0) {
		return false;
	}

	auto i = _pendingTaskIds.find(taskId);
	if (i == _pendingTaskIds.end()) {
		return false;
	}

	_pendingTaskIds.erase(i);
	return true;
}


Scheduler::State Scheduler::getState() {
	std::lock_guard<std::mutex> guard(_mutex);

	return _state;
}


void Scheduler::start() {
	std::lock_guard<std::mutex> guard(_mutex);

	if (_state == State::STOPPED) {
		_state = State::STARTED;
		_thread = std::thread(&Scheduler::thread, this);
	}
	else {
		LOGe("Cannot start scheduler unless it is stopped.");
		assert(_state == State::STOPPED);
	}
}


void Scheduler::stop() {
	std::lock_guard<std::mutex> guard(_mutex);

	if (_state == State::STARTED) {
		_state = State::STOPPING;
		_signal.notify_one();
	}
	else {
		LOGe("Cannot stop scheduler unless it is running.");
		assert(_state == State::STARTED);
	}
}


void Scheduler::thread() {
	std::unique_lock<std::mutex> uniqueLock(_mutex, std::defer_lock);

	while (_state == State::STARTED) {
		uniqueLock.lock();

		if (_tasks.empty()) {
			_signal.wait(uniqueLock);
		}
		else {
			_signal.wait_until(uniqueLock, _tasks.top()->getTime());
		}

		if (_state != State::STARTED || _tasks.empty() || _tasks.top()->getTime() > Clock::now()) {
			uniqueLock.unlock();
			continue;
		}

		// we use const_cast because of problem in C++11x: https://groups.google.com/d/topic/comp.lang.c++.moderated/rXa3SEgy0Vw/discussion
		auto task = std::move(const_cast<TaskP&>(_tasks.top()));
		_tasks.pop();

		auto i = _pendingTaskIds.find(task->getId());
		if (i == _pendingTaskIds.end()) {
			// task was cancelled
			uniqueLock.unlock();
			continue;
		}

		_pendingTaskIds.erase(i);
		uniqueLock.unlock();

		task->setExpiration(Time::max());
		server.dispatcher().addTask(std::move(task));
	}

	_mutex.lock();
	_lastUsedTaskId = 0;
	_pendingTaskIds.clear();
	_tasks = TaskQueue();
	_state = State::STOPPED;
	_mutex.unlock();
}


void Scheduler::waitUntilStopped() {
	_mutex.lock();

	if (_state == State::STARTED) {
		LOGe("Cannot wait for scheduler to stop because stopping wasn't requested.");
		assert(_state != State::STARTED);

		_mutex.unlock();
		return;
	}

	if (_thread.get_id() == std::this_thread::get_id()) {
		_mutex.unlock();
	}
	else {
		if (_thread.joinable()) {
			// TODO: race condition possible here?
			_mutex.unlock();
			_thread.join();
		}
		else {
			_mutex.unlock();
		}
	}
}



bool Scheduler::TaskQueueCompare::operator() (const TaskP& a, const TaskP& b) {
	return (a->getTime() > b->getTime());
}
