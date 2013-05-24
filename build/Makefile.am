SOURCES_DIRECTORY = $(srcdir)/sources

AM_CXXFLAGS = -Wall -Wextra -Winvalid-pch -pedantic

## TODO fix these later!
AM_CXXFLAGS += -Wno-unused-parameter -Wno-strict-aliasing -Wno-vla

noinst_PROGRAMS = server

BUILT_SOURCES = otpch.h.gch
CLEANFILES = otpch.h.gch

include ./$(DEPDIR)/server-otpch.Po

server_CPPFLAGS = $(LOG4CXX_CFLAGS) $(LUA_CFLAGS) $(XML_CPPFLAGS)
server_CXXFLAGS = $(AM_CXXFLAGS) -include "$(builddir)/otpch.h"
server_SOURCES = $(SOURCES_DIRECTORY)/otpch.h.gch \
                 $(SOURCES_DIRECTORY)/actions.cpp \
                 $(SOURCES_DIRECTORY)/admin.cpp \
                 $(SOURCES_DIRECTORY)/baseevents.cpp \
                 $(SOURCES_DIRECTORY)/beds.cpp \
                 $(SOURCES_DIRECTORY)/chat.cpp \
                 $(SOURCES_DIRECTORY)/combat.cpp \
                 $(SOURCES_DIRECTORY)/condition.cpp \
                 $(SOURCES_DIRECTORY)/configmanager.cpp \
                 $(SOURCES_DIRECTORY)/connection.cpp \
                 $(SOURCES_DIRECTORY)/container.cpp \
                 $(SOURCES_DIRECTORY)/creature.cpp \
                 $(SOURCES_DIRECTORY)/creatureevent.cpp \
                 $(SOURCES_DIRECTORY)/cylinder.cpp \
                 $(SOURCES_DIRECTORY)/database.cpp \
                 $(SOURCES_DIRECTORY)/databasemanager.cpp \
                 $(SOURCES_DIRECTORY)/databasemysql.cpp \
                 $(SOURCES_DIRECTORY)/dispatcher.cpp \
                 $(SOURCES_DIRECTORY)/depot.cpp \
                 $(SOURCES_DIRECTORY)/fileloader.cpp \
                 $(SOURCES_DIRECTORY)/game.cpp \
                 $(SOURCES_DIRECTORY)/gameservers.cpp \
                 $(SOURCES_DIRECTORY)/globalevent.cpp \
                 $(SOURCES_DIRECTORY)/group.cpp \
                 $(SOURCES_DIRECTORY)/house.cpp \
                 $(SOURCES_DIRECTORY)/housetile.cpp \
                 $(SOURCES_DIRECTORY)/ioban.cpp \
                 $(SOURCES_DIRECTORY)/ioguild.cpp \
                 $(SOURCES_DIRECTORY)/iologindata.cpp \
                 $(SOURCES_DIRECTORY)/iomap.cpp \
                 $(SOURCES_DIRECTORY)/iomapserialize.cpp \
                 $(SOURCES_DIRECTORY)/item.cpp \
                 $(SOURCES_DIRECTORY)/itemattributes.cpp \
                 $(SOURCES_DIRECTORY)/items.cpp \
                 $(SOURCES_DIRECTORY)/luascript.cpp \
                 $(SOURCES_DIRECTORY)/mailbox.cpp \
                 $(SOURCES_DIRECTORY)/map.cpp \
                 $(SOURCES_DIRECTORY)/md5.cpp \
                 $(SOURCES_DIRECTORY)/monster.cpp \
                 $(SOURCES_DIRECTORY)/monsters.cpp \
                 $(SOURCES_DIRECTORY)/movement.cpp \
                 $(SOURCES_DIRECTORY)/networkmessage.cpp \
                 $(SOURCES_DIRECTORY)/npc.cpp \
                 $(SOURCES_DIRECTORY)/otserv.cpp \
                 $(SOURCES_DIRECTORY)/outfit.cpp \
                 $(SOURCES_DIRECTORY)/outputmessage.cpp \
                 $(SOURCES_DIRECTORY)/party.cpp \
                 $(SOURCES_DIRECTORY)/player.cpp \
                 $(SOURCES_DIRECTORY)/position.cpp \
                 $(SOURCES_DIRECTORY)/protocol.cpp \
                 $(SOURCES_DIRECTORY)/protocolgame.cpp \
                 $(SOURCES_DIRECTORY)/protocollogin.cpp \
                 $(SOURCES_DIRECTORY)/protocolold.cpp \
                 $(SOURCES_DIRECTORY)/quests.cpp \
                 $(SOURCES_DIRECTORY)/raids.cpp \
                 $(SOURCES_DIRECTORY)/rsa.cpp \
                 $(SOURCES_DIRECTORY)/scheduler.cpp \
                 $(SOURCES_DIRECTORY)/schedulertask.cpp \
                 $(SOURCES_DIRECTORY)/scriptmanager.cpp \
                 $(SOURCES_DIRECTORY)/server.cpp \
                 $(SOURCES_DIRECTORY)/service.cpp \
                 $(SOURCES_DIRECTORY)/sha1.cpp \
                 $(SOURCES_DIRECTORY)/spawn.cpp \
                 $(SOURCES_DIRECTORY)/spells.cpp \
                 $(SOURCES_DIRECTORY)/status.cpp \
                 $(SOURCES_DIRECTORY)/talkaction.cpp \
                 $(SOURCES_DIRECTORY)/task.cpp \
                 $(SOURCES_DIRECTORY)/teleport.cpp \
                 $(SOURCES_DIRECTORY)/textlogger.cpp \
                 $(SOURCES_DIRECTORY)/thing.cpp \
                 $(SOURCES_DIRECTORY)/tile.cpp \
                 $(SOURCES_DIRECTORY)/tools.cpp \
                 $(SOURCES_DIRECTORY)/town.cpp \
                 $(SOURCES_DIRECTORY)/trashholder.cpp \
                 $(SOURCES_DIRECTORY)/vocation.cpp \
                 $(SOURCES_DIRECTORY)/waitlist.cpp \
                 $(SOURCES_DIRECTORY)/weapons.cpp

otpch.h.gch: $(SOURCES_DIRECTORY)/otpch.h
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(server_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) -MT otpch.h.gch -MD -MP -MF $(DEPDIR)/server-otpch.Tpo -x c++-header -c -o otpch.h.gch $<
	mv -f $(DEPDIR)/server-otpch.Tpo $(DEPDIR)/server-otpch.Po
	rm -f "$(builddir)/"*.o


# automake < 1.15
ACLOCAL_AMFLAGS = -I m4
