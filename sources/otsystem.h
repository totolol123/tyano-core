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

#ifndef _OTSYSTEM_H
#define _OTSYSTEM_H

#define CLIENT_VERSION_MIN 860
#define CLIENT_VERSION_MAX 860
#define CLIENT_VERSION_STRING "Only clients with protocol 8.60 allowed!"

#define VERSION_CHECK "http://forgottenserver.otland.net/version.xml"
#define VERSION_PATCH 1
#define VERSION_TIMESTAMP 1261647210
#define VERSION_BUILD 3429
#define VERSION_DATABASE 23

#define SCHEDULER_MINTICKS 50
#define DISPATCHER_TASK_EXPIRATION 2000

#define OTSERV_ACCESS(file, mode) access(file, mode);
inline int64_t OTSYS_TIME()
{
	timeb t;
	ftime(&t);
	return ((int64_t)t.millitm) + ((int64_t)t.time) * 1000;
}

#ifdef __GNUC__
	#define __OTSERV_FUNCTION__ __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
	#define __OTSERV_FUNCTION__ __FUNCDNAME__
#endif

#define foreach BOOST_FOREACH
#define reverse_foreach BOOST_REVERSE_FOREACH

typedef std::vector<std::pair<uint32_t, uint32_t> > IpList;

typedef std::vector<std::string> StringVector;

namespace std {
	template <>
	struct default_delete<xmlDoc> {
		void operator()(xmlDocPtr document) {
			xmlFreeDoc(document);
		}
	};
}

typedef std::unique_ptr<xmlDoc>  xmlDocP;

#endif // _OTSYSTEM_H
