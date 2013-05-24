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
#include "textlogger.h"

#include "tools.h"

void DeprecatedLogger::open()
{
	m_files[static_cast<uint8_t>(LogFile::ADMIN)] = fopen(getFilePath(FileType::LOG, "admin.log").c_str(), "a");
	m_files[static_cast<uint8_t>(LogFile::CLIENT_ASSERTION)] = fopen(getFilePath(FileType::LOG, "client_assertions.log").c_str(), "a");
}

void DeprecatedLogger::close()
{
	for(uint8_t i = 0; i <= static_cast<uint8_t>(LogFile::LAST); i++)
	{
		if(m_files[i])
			fclose(m_files[i]);
	}
}

void DeprecatedLogger::iFile(LogFile file, std::string output, bool newLine)
{
	uint8_t index = static_cast<uint8_t>(file);

	if(!m_files[index])
		return;

	internal(m_files[index], output, newLine);
	fflush(m_files[index]);
}

void DeprecatedLogger::eFile(std::string file, std::string output, bool newLine)
{
	FILE* f = fopen(getFilePath(FileType::LOG, file).c_str(), "a");
	if(!f)
		return;

	internal(f, "[" + formatDate() + "] " + output, newLine);
	fclose(f);
}

void DeprecatedLogger::internal(FILE* file, std::string output, bool newLine)
{
	if(!file)
		return;

	if(newLine)
		output += "\n";

	fprintf(file, "%s", output.c_str());
}

void DeprecatedLogger::log(const char* func, LogType type, std::string message, std::string channel/* = ""*/, bool newLine/* = true*/)
{
	std::stringstream ss;
	ss << "[" << formatDate() << "]" << " (";
	switch(type)
	{
		case LogType::EVENT:
			ss << "Event";
			break;

		case LogType::NOTICE:
			ss << "Notice";
			break;

		case LogType::WARNING:
			ss << "Warning";
			break;

		case LogType::ERROR:
			ss << "Error";
			break;

		default:
			break;
	}

	ss << " - " << func << ") ";

	if(!channel.empty())
		ss << channel << ": ";

	ss << message;
	iFile(LogFile::ADMIN, ss.str(), newLine);
}
