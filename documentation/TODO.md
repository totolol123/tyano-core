TODO
====

- remove old debug logging macros
- check use of `rand()` C functions as they are not thread-safe
- fix leaking boost sockets
- update remaining documentation located in `documentation/fixme`
- get rid of `__LOGIN_SERVER__` preprocessor macro and allow changing the login behavior by configuration
- re-order all class and struct members to minimize padding
- complete item attribute refactoring (make it easier to use and use more templating)
- refactor item structure:
    - `Class`: describes a class of items, e.g. Bed, Container, Key, etc.
    - `Kind`: describes a single item, e.g. Bed #543, Container #140, Key #364, etc.  
              One sub-class per `Class` to save memory and improve structure!
    - `Item` (i.e. an instance of Bed 543, Container 150, Key 364, etc.)  
              One sub-class per `Kind`.
- restructure source code (use namespaces, use .hpp instead of .h, etc.)
- make raids.xml easier to understand and document how it works
- allow non-ref'd raid monsters and items to disappear when the raid ended
- instead of regulary cleaning the map - decay the items (including corpses) after a while relative to the time they dropped
- item client id must not be >= 0xFF00 || (>= 0x61 && <= 0x63)  -- all reserved for netcode
- cannot move creature in ProtocolGame::GetTileDescription - fix that!
- figure out what to do if the stackpos of a creature is invalid
- check whether summons/master should start thinking depending on what the other one does
- check which creature/monster/player methods need `startThinking()`
- replace boost::intrusive\_ptr by boost::shared\_ptr
- make monster attack other target if they cannot attack their current one
- set `BOOST_DISABLE_ASSERTS` and `NDEBUG` for release
- update `hasToThinkAboutCreature` to take a monster's targeting distance into account
- find out what `Map::checkSightLine` does and why it swaps x and z coordinates, which aren't type compatible
- use least-size integers instead of fixed-size ones
- fix TODOs written in code comments
- use `weak_ptr` where it makes sense (e.g. master->summon relationship and damage list to avoid cyclic references)
- check out LLVM as compiler (esp. static analysis)
- check useful optimization flags (fast math, LTO, etc.)

Architecture
------------
* Use or develop new LUA framework.
* Further consolidate `Position`, `StackPosition` and `ExtendedPosition`.
