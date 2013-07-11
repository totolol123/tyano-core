Tyano Core
==========

This is a game server built for [Tibia](http://www.tibia.com/)®, a massively multiplayer online role-playing game developed and
maintained by [CipSoft](http://www.cipsoft.com/).

This server is derived from one of the most popular open-source servers currently available for Tibia®:
[The Forgotten Server](http://otland.net/project.php?projectid=2) (Version 0.3.6).
This server is in turn derived from an older version of [OpenTibia server](https://github.com/opentibia/server) (aka OTServ).

Tyano Core is targeted towards the **Tibia® 8.60** game client.

In order to connect to a custom Tibia® server you need a custom loader or an IP changer.
I recommend [Tibia® Loader](http://otservlist.org/ipc) by [OTServList.org](http://otservlist.org/).


Changes
--------

Although based on The Forgotten Server, Tyano Core received plenty of changes so far. My goal is not to stay compatible with
other custom Tibia® servers but to improve the server as fast as possible in three key areas: performance, stability and features.

To be honest when I (fluidsonic) started modifying The Forgotten Server to fix and change only a few things I didn't expect me to
change it so deeply and so fast. So I actually cannot list all changes which happened since the initial fork. But I'll do my best to
complete this list over time :)

- PvP servers can have designated PvPE areas.
- Reworked thinking logic of all creatures to consume less CPU and be more reliable. It's also much more precise now.
- Reworked movement logic of all creatures to consume less CPU and be more reliable. It's also much more precise now.
- Reworked premium logic with precision up to seconds. Beside offering various LUA methods to grant, revoke and check premium status
  having premium does not affect the game play in any way yet.
- Reworked the logging with a beautiful log format, various configurable log levels and additional disk-logging.
- Reworked map loading which uses a 2-dimensional array per floor now. Much faster to load, to use and uses much less memory.
  Problem is that it only works for small maps. I'll figure out something new when I've got time for that.
- Reworked scheduler and dispatcher.
- Reworked how persistable item attributes are saved so that they consume less memory. They'll also enfore their type to catch programming and
  scripting errors earlier.
- Added an admin protocol command to execute LUA scripts and get the result.
- Monsters and NPCs also wander diagonal.
- Support for much more than 2,147,483,647 money.
- Fixed plenty of memory leaks. Constantly trying to reduce the server's memory footprint.
- Added a cheap 'firewall' to the networking code which watches outgoing creature movement events and checks if everything works as expected.
- Limited number of items per tile to 9 and items + creatures per tile to 10. The Tibia® client doesn't support more than 10.
  If not, then it'll show a big warning in the log and try to alter the movement to not crash the Tibia® client.  
  Note: Tibia®'s networking code is really bad and fragile - every fault immediately leads to a (controlled) crash of the client.
- Upgrading more and more code to C++11.
- Reworked compilation process to be much faster (e.g. using precompiled headers) and to significantly increase the optimization level
  of the g++ compiled (e.g. link-time optimization - maps load in half the time as before, yay!).
- **No support** for compiling natively on Windows. If you really need to run the server on Windows (e.g. for testing locally) then use Cygwin
  (see [Compiling](documentation/compiling.md)).  
  In general I only support recent Linux distributions and recommend Ubuntu 12.10 or newer.
- **No support** for databases other than MySQL. MySQL is free and easy to use for everyone. Supporting more than one database is lots of work
  and just prevents you from developing as fast as you'd like to.
  Note: *Maybe* I might migrate the server to MongoDB some day, which is even lighter ;)

**If you notice any changes or features in Tyano Core which are not part of The Forgotten Server then please tell me so I can update this list :)**


Setup
-----

Once the remaining bigger issues (e.g. missing data folder) are solved I'll set up a step-by-step guide to get your first
Tyano Core server up and running :)


Issues
------

I'm aware that you cannot use the server as it is right now out of the box.

Especially the data folder is missing completely. You can use the
[data folder of The Forgotten Server 0.3.6pl1](http://otland.net/subversion.php?svn=public&file=dl.php&repname=forgottenserver&path=%2Ftags%2F0.3.6pl1%2Fdata%2F&rev=102&peg=102&isdir=1)
as a starting point for now. I'll provide an own data folder later.

Also a few features of The Forgotten Server are (at least parially) broken for now:

- Account Manager (didn't test that though)  
  I'll remove the Account Manager in the future. You should always provide a website with your server and allow the complete
  account management there.

- Standalone login server (did anyone actually need that yet?)

- Maps with large dimensions (no matter if there are a lot of holes or not) take more memory than they should.

- There aren't more than 9 items or 10 items+creatures per tile allowed. That currently leads to some problems e.g. if a creature dies on a tile
  with 9 items on it. There will be no corpse and thus the loot is lost.
  
- Swimming pools might be broken.


Support
-------

If you find any bugs or encounter any problems then [check out the issues section](https://github.com/fluidsonic/tyano-core/issues).
Feel free to open a new issue if your problem is not yet known to us.

You can find great help around Tibia® and custom Tibia® servers in general in the [OtLand forums](http://otland.net/).
You can also get help around Tyano Core there as I keep an eye on it ;)


Resources
---------

- [Authors](documentation/authors.md)
- [Compiling](documentation/compiling.md)
- [License](documentation/license.md)
- [Signals](documentation/signals.md)
- [Tasks](documentation/tasks.md)


Branches
--------
Branches not listed below are for working on a single new feature or bigger change (e.g. refactoring something) and will be merged into `cutting-edge` once completed.

### master [![Build Status](https://travis-ci.org/fluidsonic/tyano-core.png?branch=master)](https://travis-ci.org/fluidsonic/tyano-core)
Primary branch with the latest set of completed features and fixes.

### stable [![Build Status](https://travis-ci.org/fluidsonic/tyano-core.png?branch=stable)](https://travis-ci.org/fluidsonic/tyano-core)
Latest version known to be as stable as possible.

### cutting-edge [![Build Status](https://travis-ci.org/fluidsonic/tyano-core.png?branch=stable)](https://travis-ci.org/fluidsonic/tyano-core)
Most-recent changes and new features which are still in development. Once completed and tested, changes here will be merged into `master`.


Notes
-----

Tibia is a registered trademark of [CipSoft GmbH](http://www.cipsoft.com/).
