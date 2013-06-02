AM_CXXFLAGS = -Wall -Wextra -Winvalid-pch -pedantic

## TODO fix these later!
AM_CXXFLAGS += -Wno-unused-parameter -Wno-strict-aliasing -Wno-vla

noinst_PROGRAMS = server

BUILT_SOURCES = sources/otpch.h.gch
CLEANFILES = sources/otpch.h.gch

include sources/$(DEPDIR)/server-otpch.Po

server_CPPFLAGS = $(LOG4CXX_CFLAGS) $(LUA_CFLAGS) $(XML_CPPFLAGS)
server_CXXFLAGS = $(AM_CXXFLAGS) -iquote "$(builddir)/sources"
server_SOURCES = sources/otpch.h.gch \
                 sources/attributes/Attribute.cpp \
                 sources/attributes/Scheme.cpp \
                 sources/attributes/Values.cpp \
                 sources/combat/Combat.cpp \
                 sources/creatures/Player.cpp \
                 sources/items/Class.cpp \
                 sources/items/Common.cpp \
                 sources/items/Key.cpp \
                 sources/items/Kind.cpp \
                 sources/items/kind/Bed.cpp \
                 sources/items/kind/Common.cpp \
                 sources/items/kind/Container.cpp \
                 sources/items/kind/Depot.cpp \
                 sources/items/kind/Door.cpp \
                 sources/items/kind/Key.cpp \
                 sources/items/kind/MagicField.cpp \
                 sources/items/kind/Mailbox.cpp \
                 sources/items/kind/Teleport.cpp \
                 sources/items/kind/TrashHolder.cpp \
                 sources/items/kind/Weapon.cpp \
                 sources/utility/Utility.cpp \
                 sources/xml/Xml.cpp \
                 sources/items.cpp \
                 sources/account.cpp \
                 sources/actions.cpp \
                 sources/admin.cpp \
                 sources/baseevents.cpp \
                 sources/beds.cpp \
                 sources/chat.cpp \
                 sources/combat.cpp \
                 sources/condition.cpp \
                 sources/configmanager.cpp \
                 sources/connection.cpp \
                 sources/container.cpp \
                 sources/creature.cpp \
                 sources/creatureevent.cpp \
                 sources/cylinder.cpp \
                 sources/database.cpp \
                 sources/databasemanager.cpp \
                 sources/databasemysql.cpp \
                 sources/dispatcher.cpp \
                 sources/depot.cpp \
                 sources/fileloader.cpp \
                 sources/game.cpp \
                 sources/gameservers.cpp \
                 sources/globalevent.cpp \
                 sources/group.cpp \
                 sources/house.cpp \
                 sources/housetile.cpp \
                 sources/ioban.cpp \
                 sources/ioguild.cpp \
                 sources/iologindata.cpp \
                 sources/iomap.cpp \
                 sources/iomapserialize.cpp \
                 sources/item.cpp \
                 sources/luascript.cpp \
                 sources/mailbox.cpp \
                 sources/map.cpp \
                 sources/md5.cpp \
                 sources/monster.cpp \
                 sources/monsters.cpp \
                 sources/movement.cpp \
                 sources/networkmessage.cpp \
                 sources/npc.cpp \
                 sources/otserv.cpp \
                 sources/outfit.cpp \
                 sources/outputmessage.cpp \
                 sources/party.cpp \
                 sources/player.cpp \
                 sources/position.cpp \
                 sources/protocol.cpp \
                 sources/protocolgame.cpp \
                 sources/protocollogin.cpp \
                 sources/protocolold.cpp \
                 sources/quests.cpp \
                 sources/raids.cpp \
                 sources/rsa.cpp \
                 sources/scheduler.cpp \
                 sources/schedulertask.cpp \
                 sources/scriptmanager.cpp \
                 sources/server.cpp \
                 sources/service.cpp \
                 sources/sha1.cpp \
                 sources/spawn.cpp \
                 sources/spells.cpp \
                 sources/status.cpp \
                 sources/talkaction.cpp \
                 sources/task.cpp \
                 sources/teleport.cpp \
                 sources/textlogger.cpp \
                 sources/thing.cpp \
                 sources/tile.cpp \
                 sources/tools.cpp \
                 sources/town.cpp \
                 sources/trashholder.cpp \
                 sources/vocation.cpp \
                 sources/waitlist.cpp \
                 sources/weapons.cpp

sources/otpch.h.gch: sources/otpch.h
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(server_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) -MT sources/otpch.h.gch -MD -MP -MF sources/$(DEPDIR)/server-otpch.Tpo -x c++-header -c -o sources/otpch.h.gch $<
	mv -f sources/$(DEPDIR)/server-otpch.Tpo sources/$(DEPDIR)/server-otpch.Po
	find "sources" -name "*.o" -delete


# automake < 1.15
ACLOCAL_AMFLAGS = -I m4
