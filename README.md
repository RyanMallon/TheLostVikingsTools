Lost Vikings Tools
==================

This is a library and a set of tools based on a partial reverse
engineering of the DOS game The Lost Vikings. The Lost Vikings is
copyright Silicon and Synapse (now Blizzard) and was published by
Interplay in 1992.

License
-------

These tools are public domain. The code may be freely incorporated
into other projects, either source or binary.

Library
-------

The library code is in ```liblv``` and provides APIs for handling the
various data formats used by The Lost Vikings.

Tools
-----

The tools are:

 * pack_tool: Tool for extracting, decompressing and repacking the DATA.DAT
   file.

 * sprite_view: A viewer for the various sprites formats used by The Lost
   Vikings.

 * level_view: A very basic, incomplete level viewer.

The tools have been tested with a version of The Lost Vikings with the
following MD5 sums:

 * ```8807a952565c13e428988d87021095c8  VIKINGS.EXE```
 * ```d0f8a04202070a54d1909758ecd962b5  DATA.DAT```

Run each of the tools with ```--help``` to see the command line
options. The source for the pack and sprite viewer tools also contain
comments with some example usages.

Level Viewer
------------

Run the level viewer with the path to DATA.DAT and the level number
(numbering starts at 1). For example:

```
./level_view DATA.DAT 1
```

The arrow keys move the view, and clicking a tile or object will show
information about it. The following keys may also be used:

| Key | Effect                                |
|-----|---------------------------------------|
| F   | Toggles drawing the foreground        |
| B   | Toggles drawing the background        |
| O   | Toggles drawing objects               |
| G   | Toggles grid                          |
| R   | Toggles drawing object bounding boxes |

Building
--------

The level and sprite viewers require the SDL library. The tools have
only been tested on Linux, but should require minimal porting effort
to other operating systems. To build the library and the tools:

```
make
```

Documentation
-------------

The library is documented using Doxygen. To build and view the
documentation:

```
make docs
google-chrome docs/html/index.html
```

Known Bugs/Missing Features
---------------------------

There are lots:

 * The source contains almost no error checking (I am lazy). The tools will
   generally crash if passed incorrect arguments or invalid data.

 * Some levels, notably most of the special levels, will crash the level
   viewer.

 * Many sprites are either not rendered, or rendered incorrectly by the level
   viewer. This is due to the following problems:

   - Packed 32x32 object sprites are not drawn. I currently do not know how
     the game specifies which sprite set to draw for this format.

   - Many unpacked sprites are drawn with the wrong size, resulting in a
     corrupted image. Objects have a width and height which appears to be
     a bounding box (for example the two doors on level one, which are the
     same type, have different bounding boxes). The object database entry
     for the type (basically the object's class) also has a width and height,
     but these do not correspond directly to the sprite size.

   - Some objects, such as doors, are comprised of multiple sprites. The
     level viewer will only draw the first sprite in the set.

   - Some objects, such as collectibles, use a single sprite set for multiple
     different objects. The level viewer will only draw the first sprite from
     the set. It is not fully understood how the correct frame is chosen by
     the game.

   - The Vikings are not always drawn in their correct start positions.

   - Palette animations are not supported. The information about them is
     decoded, but not fully understood.

 * The object database contains programs for a virtual machine. It appears
   that this is used to control the behaviour of the game objects. It may
   also be used to handle rendering of some objects like doors that consist
   of multiple sprites. The virtual machine is complex and I have only
   reversed a small amount of it (not enough to be useful).

 * I have not bothered to reverse the music or sound formats. The music
   format appears to be Extended Multiple Instrument Digital Interface (XMI).
