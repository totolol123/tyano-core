Tasks
=====

Urgent
------

- Rework teleporters and stairs.


Important
---------

- Nothing is important right now :)


Soon
----

- Chat channels are not joined automatically when connecting while the character is already in the game.
- Add check to not serialize unsupported character directions in `ProtocolGame`.
- Monsters should spawn in alternative locations if the destination is occupied.
- Add pathfinding test to stairs and teleporters so that they don't move players to places where they aren't supposed to be.
- Add popup window which informs players about new server updates when connecting.
- Create new efficient scheduler/dispatcher system based on C++11 STL (and refactor `Item::ReleaseInfo::release` after that) (crit bits?).
- Broadcast message with pop-up because a server-wide red chat message will actually not be read by most players.
- Refactor talkaction system to unify commands & help system.
- Fix that one boost network socket leaks every time a new connection is accepted.
- Complete item structure refactoring.
- Find to-dos written directly in the code and migrate them here.
- Refactor chat messages to be more visible (e.g. gamemaster broadcasts, etc.)
- Reduce overall number of corpses and also make them disappear faster. Esp. if there are more than 7-8 corpses on a single tile it get's tricky and players may lose loot.
- Refactor networking code because the current one is unstable and unsafe.  
  E.g. what if an invisible creature becomes visible? The stack *order* of creatures would vary across clients. The current code cannot handle that!
- Swimming pools are most likely broken.


Improvements
------------

- Improve warnings about wrong house entrances.
- Support `text` and `writer` attributes in items.xml.
- Use custom class for errors.
- Don't use canSeeCreature for notifyLogIn/Out because then invisibility affects VIP lists.
- Improve spawning on log-in in cases where there is not enough room to spawn the player.
- Improve creature damage map to be limited in time and to better take of killed creatures (e.g. if a player's summon caused most damage and dies before the target dies, the player won't be attributed).
- Refine monster logic regarding magic fields - the higher the damage to the monster, the less likely it should run over it.
- Monsters should not spawn in damage fields unless they are immune.
- Rework decaying tuse Scheduler. Decaying is also buggy if it's toggeled due to item transformation (not properly removed from Game's decaying list).
- Improve stairs movement so that the player exits a the most logical position if free.
- Improve updating creature direction after diagonal movement.
- Make `Game::clearSpectatorCache` obsolete by (partial?) invalidation in `Map::onCreatureMoved`.
- Use scheduler for decaying items.
- Corpse lifecycles without difficult decaying configuration and flexible lifetime (e.g. corpses of bosses and players should last longer and be immobile for longer).
- Make monsters think in groups. E.g. a boss is attacked, the other monsters should support him. 
  And if a summon is attacked, the master should support it.
- Once there are too many corpses and/or items on a tile, merge them into a box?
- Make separate item class for item type for corpses?
- `Game::internalMoveItem` deletes and re-creates a full stack of stackable items which is inefficient. 
- Replace `boost::function` with `std::function`.
- Replace all occurrences of `std::(shared|unique|weak)_ptr` with `Shared|Unique|Weak`.
- Replace `const Time&` with just `Time` as `timepoint_t` is backed by `duration` which is a primitive type. 
- Use separate dispatching/scheduling for cross-thread and inter-game-thread avoid unnecessary locks and to speed up everything.
- Use other approach to load the map because the current approach is limited to small maps.
- Improve autowalking. Currently you lag when clicking multiple times and the walking randomly ends, e.g. when a creature walks into the path.
- Add randomization to the think intervals to distribute CPU load as much as possible.
- Use C++11 random number generators. Also check if the random number generating functions are not used concurrently.
- Update remaining documentation files located in `documentation/fixme`.
- Make login server functionality a run-time option instead of a compile-time option and get rid of `__LOGIN_SERVER__`.
- Restructure old sourcecode (e.g. using namespaces and using `.hpp` instead of `.h` etc.).
- Refactor raid system to be much more reliable and easier to use. Also make non-ref's raid monsters and items disappear when the raid ended.
- Use `Scheduler` for decaying items instead of a regular task in `Game` to better distribute CPU load.
- Validate that all client IDs for items are valid (`< 0xFF00 && (< 0x61 || > 0x63)`, i.e. not reserved by netcode) and have no duplicates.
- Revisit relationship between master and summon creatures regarding thinking-logic and automatic targeting.
- Replace `boots::intrusive_ptr` by `std::shared_ptr`.
- Make monsters attack other targets if they are unable to reach their current one.
- Set `BOOST_DISABLE_ASSERTS` and `NDEBUG` for release build.
- Obey a monster's targeting range when choosing a target.
- Find out what `Map::checkSightLine` does and why it swaps `x` and `z` coordinates which aren't type compatible.
- Revisit all types used for integral numbers. `intXY_t` vs. `int_leastXY_t` etc. for speed vs. memory vs. compatibility.
- Use `std::weak_ptr` where it makes sense (e.g. master->summon relationship and damage list) to avoid cyclic references.
- Check out LLVM as compiler, esp. static analysis.
- Use or develop a LUA framework.
- Further consolidate `Position`, `StackPosition` and `ExtendedPosition`.
- Migrate `TODO.md` to GitHub issues.
- Never directly use enum values as array index or size. 
