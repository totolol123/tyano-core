Tasks
=====

Urgent
------

- Nothing is urgent right now! :)


Important
---------

- Make items automatically disappear after a while on the ground instead of regulary cleaning the map to distribute CPU load.


Soon
----

- Create new effficient scheduler/dispatcher system based on C++11 STL (and refactor `Item::ReleaseInfo::release` after that).
- Broadcast message with pop-up because a server-wide red chat message will actually not be read by most players.
- Refactor talkaction system to unify commands & help system.
- Fix that one boost network socket leaks every time a new connection is accepted.
- Complete item structure refactoring.
- Find to-dos written directly in the code and migrate them here.
- Refactor chat messages to be more visible (e.g. gamemaster broadcasts, etc.)
- Reduce overall number of corpses and also make them disappear faster. Esp. if there are more than 7-8 corpses on a single tile it get's tricky and players may lose loot.
- Refactor networking code because the current one is unstable and unsafe.  
  E.g. what if an invisible creature becomes visible? The stack *order* of creatures would vary across clients. The current code cannot handle that!


Improvements
------------

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
