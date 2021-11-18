# Walkgame

A very simple game for MS-DOS+VGA PCs (or Dosbox :=)) that demonstrates the use of an offscreen
drawing buffer (and supporting drawing library), collision detection (rectangular & circular),
and game timing via BIOS clock ticks.

A 32-bit version (see below) supports SVGA video modes.

## Building

### 16-bit with TurboC

Ensure you have the TurboC sub-project (`git submodule udpate --init --recursive`), configure
your Dosbox (other emulators are possible) to make this project folder drive `C:`, then in
MS-DOS, run:
```bash
C:\> setup
C:\> make
```
which should result in an executable `BIN\WALKGAME.EXE`, which can be executed from the root
folder (so it is able to locate and load resources).

### 32-bit with DJGPP

This builds outside MS-DOS using a modern(ish) GCC toolchain. Currently tested on Linux but
_should_ build similarly on OSX or Windows via MinGW. Reports and PRs welcome!

Download the latest DJGPP pre-built release from:
https://github.com/andrewwutw/build-djgpp/releases
and unpack into a folder beside this one, eg: `../djgpp`

Download the latest CWSDPMI pre-built release from:
http://sandmann.dotster.com/cwsdpmi/csdpmi7b.zip
and unpack the `bin` folder into `../djgpp` from above.

```bash
% make -f Makefile.djgpp
```
should produce a binary `djgpp/walkgame.exe` that is a self-contained 32-bit DOS protected mode
application, with SuperVGA mode support (and it's waay faster in Dosbox, possibly because
Dosbox switches to unrestricted clocking once an application enters protected mode?)

Fire up your Dosbox or hardware, and run the application from the root folder (as above).

## Playing

Hopefully self-evident!
