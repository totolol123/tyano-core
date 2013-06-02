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
- `Item` constructor should not set `charges` to `amount`
- split `items::Kind` into subclasses
- make various `items::Kind` properties redundant:
    - `showCharges`: always show charges - remove charges where they make no sense (e.g. keys)
    - `showAttributes`: always show attributes if present
    - `runeSpellName`: derive from rune or move to subclass
    - `usable`: derive usability from actions
    - `vocations`: generate on-the-fly? store in movescript?
    - `wieldInfo`: make general, move to movescript or remove completely
    - `_grants*`: make actions or whatever - but keep it away from here
- `ConditionSpeed` should also affect speed bonuses, not just the base speed
- when entering a bed the player sometimes automatically walks over unwalkable tiles
- when leaving a bed the player is sometimes placed on an unwalkable tile
- update item defaults so that left hand, right hand and ammo slots are only available for weapons, shields and ammo respectively
- merge `(extra)attack|defense` into one variable
- split item and creature attributes in three tiers:
    - item/creature base values
    - summands (additional values from equipment, spells etc. - e.g. `+12 atk + +16 atk = +28 atk`)
    - multiplicators (additional values from equipment, spells etc. - e.g. `0.03 speed` for being paralyzed) (creatures only)
    - current value will be `(base + summands) * multiplicators`
- figure out why we have `hitChance` and `maximumHitChange` in `items::Kind`
- figure out which signed integers in `items::Kind` could be made unsigned
- figure out which integers in `items::Kind` could be made smaller
- write reusable std::hash template class for enum classes
- `items::Kind` move `_damageType`, `_floorChange0`, `_floorChange1` and `_hangableType` back to bit-fields once http://gcc.gnu.org/bugzilla/show_bug.cgi?id=51242 is fixed
- `ts::combat::getDamageTypeWithId` etc: use `std::unordered_map`
- create type trait to make looping through enum classes easier (e.g. `ts::combat::DamageType` in `Kind.cpp`)
- add proper logging to `ts::items::Class` with useful information about the context
- use `std::chrono::duration` for durations/intervals throughout the `ts::items` namespace
- find out why `ConditionDamage` needs negated damage values - that's counter-intuitive.
