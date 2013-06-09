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

#include "dispatcher.h"

#include "game.h"
#include "outputmessage.h"
#include "server.h"
#include "task.h"


LOGGER_DEFINITION(Dispatcher);



Dispatcher::Dispatcher()
	: _state(State::STOPPED)
{
}


Dispatcher::~Dispatcher() {
	auto locked = _mutex.try_lock();
	if (!locked) {
		LOGe("Dispatcher deleted while in use.");
		assert(locked);

		_mutex.lock();
	}

	if (_state != State::STOPPED) {
		LOGe("Dispatcher deleted but not yet stopped. Forgot to call waitUntilStopped()?");
		assert(_state == State::STOPPED);

		if (std::this_thread::get_id() != _thread.get_id()) {
			if (_state == State::STARTED) {
				stop();
			}

			waitUntilStopped();
		}
		else {
			LOGe("Cannot delete the dispatcher from within the dispatcher thread.");
		}
	}

	_mutex.unlock();
}


void Dispatcher::addTask(TaskP task, bool urgent /*= false*/) {
	if (task == nullptr) {
		assert(task != nullptr);
		return;
	}

	std::lock_guard<std::mutex> lock(_mutex);

	switch (_state) {
	case State::STOPPED:
	case State::STARTED:
		if (urgent) {
			_tasks.push_front(std::move(task));
		}
		else {
			_tasks.push_back(std::move(task));
		}

		if (_tasks.size() == 1) {
			_signal.notify_one();
		}

		break;

	case State::STOPPING:
		LOGt("Cannot add task to dispatcher which about to stop.");
		break;
	}
}


Dispatcher::State Dispatcher::getState() {
	std::lock_guard<std::mutex> guard(_mutex);

	return _state;
}


void Dispatcher::runTask(const Task& task, Game& game, OutputMessagePool* messagePool) const {
	if (task.getExpiration() < Clock::now()) {
		return;
	}

	if (messagePool != nullptr) {
		messagePool->startExecutionFrame();
	}

	task();

	// TODO run all(most?) outstanding tasks before sending all messages
	if (messagePool != nullptr) {
		messagePool->sendAll();
	}

	game.clearSpectatorCache();
}


void Dispatcher::runTasks(const TaskDeque& tasks) const {
	auto& game = server.game();
	auto messagePool = OutputMessagePool::getInstance();

	for (auto task = tasks.begin(); task != tasks.end(); ++task) {
		runTask(**task, game, messagePool);
	}
}


void Dispatcher::start() {
	std::lock_guard<std::mutex> guard(_mutex);

	if (_state == State::STOPPED) {
		_state = State::STARTED;
		_thread = std::thread(&Dispatcher::thread, this);
	}
	else {
		LOGe("Cannot start dispatcher unless it is stopped.");
		assert(_state == State::STOPPED);
	}
}


void Dispatcher::stop() {
	std::lock_guard<std::mutex> guard(_mutex);

	if (_state == State::STARTED) {
		_state = State::STOPPING;

		if (_tasks.empty()) {
			_signal.notify_one();
		}
	}
	else {
		LOGe("Cannot stop dispatcher unless it is running.");
		assert(_state == State::STARTED);
	}
}


void Dispatcher::thread() {
	std::unique_lock<std::mutex> uniqueLock(_mutex, std::defer_lock);

	while (_state == State::STARTED) {
		uniqueLock.lock();

		if (_tasks.empty()) {
			_signal.wait(uniqueLock);
		}

		if (_state != State::STARTED || _tasks.empty()) {
			uniqueLock.unlock();
			continue;
		}

		auto task = std::move(_tasks.front());
		_tasks.pop_front();

		uniqueLock.unlock();

		auto& game = server.game();
		auto messagePool = OutputMessagePool::getInstance();

		runTask(*task, game, messagePool);
	}

	_mutex.lock();
	auto tasks = std::move(_tasks);
	_mutex.unlock();

	runTasks(tasks);

	_mutex.lock();
	_state = State::STOPPED;
	_mutex.unlock();
}


void Dispatcher::waitUntilStopped() {
	_mutex.lock();

	if (_state == State::STARTED) {
		LOGe("Cannot wait for dispatcher to stop because stopping wasn't requested.");
		assert(_state != State::STARTED);

		_mutex.unlock();
		return;
	}

	if (_thread.get_id() == std::this_thread::get_id()) {
		auto tasks = std::move(_tasks);
		_mutex.unlock();

		runTasks(tasks);
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
