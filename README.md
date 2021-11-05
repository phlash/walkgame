# Walkgame

A very simple game for MS-DOS+VGA PCs (or Dosbox :=)) that demonstrates the use of an offscreen
drawing buffer (and supporting drawing library), collision detection (rectangular & circular),
and game timing via BIOS clock ticks.

## Building

Ensure you have the TurboC sub-project (`git submodule udpate --init --recursive`), configure
your Dosbox (other emulators are possible) to make this project folder drive `C:`, then in
MS-DOS, run:
```bash
C:\> setup
C:\> make
```
which should result in an executable `BIN\WALKGAME.EXE`, which can be executed from the root
folder (so it is able to locate and load resources).

## Playing

Hopefully self-evident!
