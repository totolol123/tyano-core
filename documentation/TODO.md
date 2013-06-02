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
