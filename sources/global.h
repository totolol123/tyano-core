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


#define SOFTWARE_NAME       PACKAGE_NAME
#define SOFTWARE_VERSION    PACKAGE_VERSION
#define SOFTWARE_DEVELOPERS "Elf, Talaturen, Dalkon, BeniS, Tryller, Kornholijo and fluidsonic"
#define SOFTWARE_PROTOCOL   "8.60"

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

#endif // GLOBAL_H
