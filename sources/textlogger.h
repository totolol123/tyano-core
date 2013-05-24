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

#ifndef _TEXTLOGGER_H
#define _TEXTLOGGER_H

enum class LogFile : uint8_t
{
	FIRST = 0,
	ADMIN = FIRST,
	CLIENT_ASSERTION = 1,
	LAST = CLIENT_ASSERTION
};

enum class LogType : uint8_t
{
	EVENT,
	NOTICE,
	WARNING,
	ERROR,
};

class DeprecatedLogger
{
	public:
		~DeprecatedLogger() {close();}
		static DeprecatedLogger* getInstance()
		{
			static DeprecatedLogger instance;
			return &instance;
		}

		void open();
		void close();

		void iFile(LogFile file, std::string output, bool newLine);
		void eFile(std::string file, std::string output, bool newLine);

		void log(const char* func, LogType type, std::string message, std::string channel = "", bool newLine = true);

	private:
		DeprecatedLogger() {}
		void internal(FILE* file, std::string output, bool newLine);

		FILE* m_files[static_cast<uint8_t>(LogFile::LAST) + 1];
};

#define LOG_MESSAGE(type, message, channel) \
	DeprecatedLogger::getInstance()->log(__OTSERV_FUNCTION__, type, message, channel);

#endif // _TEXTLOGGER_H
