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

#ifndef _TASK_H
#define _TASK_H


class Task {

public:

	typedef std::chrono::milliseconds             Duration;
	typedef boost::function<void(void)>           Function;
	typedef std::chrono::steady_clock::time_point TimePoint;
	typedef std::unique_ptr<Task>                 UniquePointer;


	static UniquePointer create(const Function& function);
	static UniquePointer create(Duration duration, const Function& function);

	virtual ~Task();

	TimePoint getExpiration() const;
	void      setExpiration(TimePoint expiration);

	void operator()() const;


protected:

	Task(const Function& function);
	Task(Duration duration, const Function& function);


private:

	TimePoint _expiration;
	Function  _function;

};

#endif // _TASK_H
