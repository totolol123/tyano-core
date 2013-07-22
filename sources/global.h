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

#ifndef _GLOBAL_H
#define _GLOBAL_H

#include "config.h"


#define SOFTWARE_NAME      PACKAGE_NAME
#define SOFTWARE_VERSION   PACKAGE_VERSION
#define SOFTWARE_PROTOCOL  "8.60"

#define X_PROJECT_URL         "https://github.com/fluidsonic/tyano-core"
#define X_PROJECT_ISSUES_URL  X_PROJECT_URL "/issues"

// we disable the sendbuffer because as OutputMessagePool works right now it forgets to send small packets
// unless there is activity on the server causing another send and detecting the timeout!
#define __NO_PLAYER_SENDBUFFER__  1

// TODO remove dependencies on this
#define __ENABLE_SERVER_DIAGNOSTIC__  1

#define LOGd_(logger, message)  do { LOG4CXX_DEBUG(logger, message) } while(false)
#define LOGe_(logger, message)  do { LOG4CXX_ERROR(logger, message) } while(false)
#define LOGf_(logger, message)  do { LOG4CXX_FATAL(logger, message) } while(false)
#define LOGi_(logger, message)  do { LOG4CXX_INFO(logger, message) } while(false)
#define LOGt_(logger, message)  do { LOG4CXX_TRACE(logger, message) } while(false)
#define LOGw_(logger, message)  do { LOG4CXX_WARN(logger, message) } while(false)
#define LOGd(message)           LOGd_(logger, message)
#define LOGe(message)           LOGe_(logger, message)
#define LOGf(message)           LOGf_(logger, message)
#define LOGi(message)           LOGi_(logger, message)
#define LOGt(message)           LOGt_(logger, message)
#define LOGw(message)           LOGw_(logger, message)

#define LOGGER_DECLARATION            static const LoggerPtr logger
#define LOGGER_DEFINITION(className)  const LoggerPtr className::logger = Logger::getLogger("tfts." #className)
#define LOGGER_INLINE(className)      static const LoggerPtr logger = Logger::getLogger("tfts." #className)

using log4cxx::Logger;
using log4cxx::LoggerPtr;


namespace std {

#ifdef __CYGWIN__

	// Cygwin development is a bit behind and doesn't propagate all math functions to the std namespace.

	using ::ceil;
	using ::floor;
	using ::round;

#endif


	template<>
	struct default_delete<xmlDoc> {
		void operator()(xmlDocPtr document) const {
			xmlFreeDoc(document);
		}
	};


	template<>
	struct default_delete<lua_State> {
		void operator()(lua_State* state) const {
			lua_close(state);
		}
	};


	template<typename T>
	struct equal_to<reference_wrapper<const T>> : public function<bool(const reference_wrapper<const T>&, const reference_wrapper<const T>&)> {
		bool operator()(const reference_wrapper<const T>& a, const reference_wrapper<const T>& b) const {
			return equal_to<T>()(a, b);
		}
	};


	template<typename T>
	struct hash<boost::intrusive_ptr<T>> : public function<size_t(const boost::intrusive_ptr<T>&)> {
		size_t operator()(const boost::intrusive_ptr<T>& pointer) const {
			return boost::hash<boost::intrusive_ptr<T>>()(pointer);
		}
	};


	template<typename T>
	struct hash<const T> {
		size_t operator()(const T& reference) const {
			return hash<T>()(reference);
		}
	};


	template<typename T>
	struct hash<reference_wrapper<T>> {
		size_t operator()(const reference_wrapper<T>& reference) const {
			return hash<T>()(reference);
		}
	};
}

typedef std::chrono::steady_clock                      Clock;
typedef std::chrono::duration<int, std::ratio<86400>>  Days;
typedef Clock::duration                                Duration;
typedef std::chrono::hours                             Hours;
typedef std::chrono::milliseconds                      Milliseconds;
typedef std::chrono::minutes                           Minutes;
typedef std::chrono::system_clock                      RealClock;
typedef RealClock::duration                            RealDuration;
typedef RealClock::time_point                          RealTime;
typedef std::chrono::seconds                           Seconds;
typedef Clock::time_point                              Time;
typedef std::unique_ptr<xmlDoc>                        xmlDocP;

using Function = boost::function<void(void)>;

template<class T> using Shared = std::shared_ptr<T>;
template<class T> using Unique = std::unique_ptr<T>;
template<class T> using Weak   = std::weak_ptr<T>;



template<typename T>
inline constexpr auto operator + (T value) -> typename std::underlying_type<T>::type {
	return static_cast<typename std::underlying_type<T>::type>(value);
}


inline std::ostream& operator << (std::ostream& stream, int8_t value) {
	return stream << static_cast<int_least16_t>(value);
}


inline std::ostream& operator << (std::ostream& stream, uint8_t value) {
	return stream << static_cast<uint_least16_t>(value);
}


inline std::ostream& operator << (log4cxx::helpers::CharMessageBuffer& stream, int8_t value) {
	return stream << static_cast<int_least16_t>(value);
}


inline std::ostream& operator << (log4cxx::helpers::CharMessageBuffer& stream, uint8_t value) {
	return stream << static_cast<uint_least16_t>(value);
}


inline std::ostream& operator << (log4cxx::helpers::MessageBuffer& stream, int8_t value) {
	return stream << static_cast<int_least16_t>(value);
}


inline std::ostream& operator << (log4cxx::helpers::MessageBuffer& stream, uint8_t value) {
	return stream << static_cast<uint_least16_t>(value);
}

#endif // GLOBAL_H
