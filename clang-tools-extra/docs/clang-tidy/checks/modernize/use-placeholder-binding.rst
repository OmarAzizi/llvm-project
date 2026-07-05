.. title:: clang-tidy - modernize-use-placeholder-binding

modernize-use-placeholder-binding
==================================

Finds structured bindings where one of the bindings is only used to suppress
an "unused variable" warning via a ``(void)name;`` statement, and suggests
replacing that binding with a C++26 placeholder (``_``) instead, removing the
now unnecessary suppression statement.

.. code-block:: c++

  const auto [x, y] = compute();
  (void)y;
  use(x);

  // becomes

  const auto [x, _] = compute();
  use(x);

This also applies to structured bindings declared in the init-statement of an
``if``, ``switch``, or ``for`` statement:

.. code-block:: c++

  if (const auto [it, inserted] = set.insert(value); inserted) {
    (void)it;
    // ...
  }

  // becomes

  if (const auto [_, inserted] = set.insert(value); inserted) {
    // ...
  }

Limitations
-----------

The check only recognizes the ``(void)name;`` suppression idiom as a
standalone statement directly inside a compound statement. It does not
diagnose bindings that are simply unused with no suppression statement, nor
bindings suppressed some other way. It also does not diagnose a suppression
cast that is the sole substatement of a ``case`` or ``default`` label (for
example, inside a ``switch``), since removing it safely would require
special-casing the label rather than the statement.
