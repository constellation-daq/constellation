# Naming Conventions & Case Sensitivity

Constellation aims to strike between a maximum convenience for users and

## User Input is Case-Insensitive

All user input should be matched against keys case-insensitively. This concerns configuration keys, but also satellite names
and types, commands etc. The goal of this is to minimize the possibility of long bug-hunts for mis-configurations.

## Public Names

* Satellite types should be `PascalCase`. Case is preserved to make them more easily readable.
* Command names should be `snake_case` for quicker typing and easy tab completion.
* Configuration parameters should best be defined in `snake_case`, i.e. all lower-case with underscores separating words.

## Code Naming Conventions

The code base of Constellation follows concise rules on naming schemes and coding conventions. This enables maintaining a
high quality of code and ensures maintainability over a longer period of time. The following naming conventions should be
adhered to when writing code which eventually should be merged into the main repository.

::::{tab-set}

:::{tab-item} C++
:sync: cxx

* **Namespace**:
  The `constellation` namespace should be used for all classes which are part of the framework, nested namespaces may be
  defined. It is encouraged to make use of `using namespace constellation;` in implementation files only for this namespace.
  Especially the namespace `std` should always be referred to directly at the function to be called, e.g. `std::string test`.

* **Class names**:
  Class names are typeset in `PascalCase`, starting with a capital letter, e.g. `class HeartbeatManager`. Every class should
  provide sensible Doxygen documentation for the class itself as well as for all member functions.

* **Member functions**:
  Naming conventions are different for public and private class members. Public member function names are typeset as
  `camelCase` names without underscores, e.g. `getCanonicalName()`. Private member functions follow `snake_case` using
  lower-case names, separating individual words by an underscore, e.g. `update_config(...)`. This allows to visually
  distinguish between public and restricted access when reading code.

  In general, public member function names should follow the `get`/`set` convention, i.e. functions which retrieve
  information and alter the state of an object should be marked accordingly. Getter functions should be made `const` where
  possible to allow usage of constant objects of the respective class.

* **Member variables**:
  Member variables of classes should always be private and accessed only via respective public member functions. This
  allows to change the class implementation and its internal members without requiring to rewrite code which accesses them.
  Member names should be typeset `snake_case`, i.e. in lower-case letters. A trailing underscore is used to mark them as
  member variables, e.g. `Logger logger_`. This immediately sets them apart from local variables declared within a function.

:::

:::{tab-item} Python
:sync: py

TODO

:::

::::
