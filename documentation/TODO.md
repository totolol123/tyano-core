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
