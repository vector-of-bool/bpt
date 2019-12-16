``dds`` Design and Rationale
############################

``dds`` has been designed from the very beginning as an extremely opinionated
hybrid *build system* and *package manager*. Unlike most build systems however,
``dds`` has a hyper-specific focus on a particular aspect of software
development: C and C++ libraries.

This may sound pointless, right? Libraries are useless unless we can use them
to build applications!

Indeed, applications *are* essential, but that is "not our job" with ``dds``.

Another design decision is that ``dds`` is built to be driven by automated
tools as well as humans. ``dds`` is not designed to entirely replace existing
build systems and package management solutions. Rather, it is designed to be
easy to integrate *with* existing systems and tools.


Background
**********

I'm going to say something somewhat controversial: C and C++ don't need
"package management." At least, not *generalized* "package management." C++
needs *library* "package management."

The C and C++ compilation model is inherently *more complex* than almost any
other language in use today. This isn't to say "bad," but rather than it is
built to meet extremely high and strange demands. It also comes with a large
burden of *legacy*. Meeting both of these requirements simultaneously presents
incredible implementation challenges.

Despite the vast amount of work put into build systems and tooling, virtually
all developers are using them *incorrectly* and/or *dangerously* without
realizing it. Despite this work, we seem to be a great distance from a unified
library package distribution and consumption mechanism.


Tabula Rasa
***********

``dds`` attempts to break from the pattern of legacy demands and strange usage
demands in a few ways. The major differences between ``dds`` and other build
systems like CMake, Meson, build2, SCons, MSBuild, etc. is that of *tradeoffs*.
If you opt-in to have your library built by ``dds``, you forgoe
*customizability* in favor of *simplicity* and *ease*.

``dds`` takes a look at what is needed to build and develop *libraries* and
hyper-optimizes for that use case. It is also built with a very strong, very
opinionated idea of *how* libraries should be constructed and used. These
prescriptions are not at all arbitrary, though. They are built upon the
observations of the strengths and weaknesses of build systems in use throughout
industry and community.

There is some ambiguity on the term "build system." It can mean one of two
things:

1. A *proper noun* "Build System," such as CMake, Meson, Autotools, or even
   Gulp, WebPack, and Mix. These are specific tools that have been developed
   for the implementation of the second definition:
2. A general noun "build system" refers to the particular start-to-finish
   process through which a specific piece of software is mapped from its raw
   *inputs* (source code, resource libraries, toolchains) to the outputs
   (applications, appliances, libraries, or web sites).

For example, LLVM and Blender both use the CMake "Build System," but their
"build system" is not the same. The "build system" for each is wildly
different, despite both using the same underlying "Build System."

``dds`` takes a massive divergence at this point. One project using ``dds`` as
their build system has a nearly identical build process to every other project
using ``dds``. Simply running :code:`dds build -t <toolchain>` should be enough
to build *any* ``dds`` project.

In order to reach this uniformity and simplicity, ``dds`` drops almost all
aspects of project-by-project customizability. Instead, ``dds`` affords the
developer a contract:

    If you play by my rules, you get to play in my space.


.. _design.rules:

The Rules
*********

We've talked an awful lot about the "rules" and "restrictions" that ``dds``
imposes, but what are they?


.. _design.rules.not-apps:

``dds`` Is not Made for Complex Applications
===============================================

Alright, this one isn't a "rule" as much as a recommendation: If you are
building an application that *needs* some build process functionality that
``dds`` does not provide, ``dds`` is only open to changes that do not
violate any of the other existing rules.

.. note::
    **However:** If you are a *library* author and you find that ``dds``
    cannot correctly build your library without violating other rules, we may
    have to take a look. This is certainly not to say it will allow arbitrary
    customization features to permit the rules to be bent arbitrarily: Read
    on.

``dds`` contains a minimal amount of functionality for building simple
applications, but it is certainly not its primary purpose.


.. _design.rules.change:

*Your* Code Should Be Changed Before ``dds`` Should Be Changed
=================================================================

The wording of this rule means that the onus is on the library developer to
meet the expectations that ``dds`` prescribes in order to make the build
work.

If your library meets all the requirements outlined in this document but you
still find trouble in making your build work, this is grounds for change in
``dds``, either in clarifying the rules or tweaking ``dds`` functionality.


.. _design.rules.layout:

Library Projects Must Meet the Layout Requirements
==================================================

This is a very concrete requirement. ``dds`` prescribes a particular project
structure layout with minimal differing options. ``dds`` prescribes the
`Pitchfork`_ layout requirements.

.. note::
    These prescriptions are not as draconian as they may sound upon first
    reading. Refer to the :doc:`guide/packages` page for more information.

.. _Pitchfork: https://api.csswg.org/bikeshed/?force=1&url=https://raw.githubusercontent.com/vector-of-bool/pitchfork/develop/data/spec.bs


.. _design.rules.no-cond-compile:

A Library Build Must Successfully Compile All Source Files
==========================================================

Almost all Build Systems have a concept of *conditionally* adding a source file
to a build. ``dds`` elides this feature in place of relying on in-source
conditional compilation.


.. _design.rules.no-lazy-code-gen:

All Code Must Be in Place Before Building
=========================================

``dds`` does not provide code-generation functionality. Instead, any
generated code should be generated and committed to the repository to be only
ever modified through such generation scripts.


.. _design.rules.one-binary-per-src:

All Compilable Files in a ``src/`` Directory Must Link Together
===============================================================

As part of the prescribed project layout, the ``src/`` project directory
contains source files. ``dds`` requires that *all* source files in a given
``src/`` directory should link together cleanly. Practically, this means that
every ``src/`` directory must correspond to *exactly* one library.


.. _design.rules.include:

No Arbitrary ``#include`` Directories
=====================================

Only ``src/`` and ``include/`` will ever be used as the basis for header
resolution while building a library, so all ``#include`` directives should be
relative to those directories. Refer to :ref:`pkg.source-root`.


.. _design.rules.uniform-compile:

All Files Compile with the Same Options
=======================================

When DDS compiles a library, every source file will be compiled with an
identical set of options. Additionally, when DDS compiles a dependency tree,
every library in that dependency tree will be compiled with an identical set of
options. Refer to the :doc:`guide/toolchains` page for more information.

Currently, the only exception to this rules is for flags that control compiler
warnings: Dependencies will be compiled without adding any warnings flags,
while the main project will be compiled with warnings enabled by default.
