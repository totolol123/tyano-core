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

#ifndef _OTPCH_H
#define _OTPCH_H

#include <algorithm>
#include <bitset>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <sched.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <boost/thread/detail/platform.hpp>
// https://svn.boost.org/trac/boost/ticket/7089
#undef BOOST_THREAD_WAIT_BUG

#include <boost/algorithm/string.hpp>
#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/config.hpp>
#include <boost/current_function.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#include <boost/intrusive_ptr.hpp>
#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/tokenizer.hpp>
#include <boost/tr1/unordered_set.hpp>
#include <boost/utility.hpp>
#include <boost/variant.hpp>
#include <boost/version.hpp>

#include <gmp.h>
#include <libxml/parser.h>
#include <libxml/threads.h>
#include <libxml/xmlmemory.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/logger.h>
#include <log4cxx/patternlayout.h>
#include <log4cxx/xml/domconfigurator.h>
#include <mysql/mysql.h>

extern "C"
{
#	include <lua.h>
#	include <lualib.h>
#	include <lauxlib.h>
}

#include "global.h"
#include "otsystem.h"

#endif // _OTPCH_H
