all: bin bin\walkgame.exe bin\svgablit.exe

clean:
	-del bin\*.*
	-rmdir bin

{src}.c{bin}.obj:
	tcc -G -O2 -Iturboc3\tc\include -Isrc -ml -c -o$@ $<

{src}.asm{bin}.obj:
	tasm /dc $<, $@

# - prefix ignores exit code
bin:
	-mkdir bin

bin\walkgame.exe: bin\walkgame.obj bin\drawlib.lib
	tlink c0l.obj bin\walkgame.obj bin\drawlib.lib, bin\walkgame.exe,,emu mathl cl -Lturboc3\tc\lib

bin\drawlib.lib: bin\drawlib.obj
	-@del $@
	tlib bin\drawlib.lib +bin\drawlib.obj

bin\svgablit.exe: bin\svgablit.obj
	tlink c0l.obj bin\svgablit.obj, bin\svgablit.exe,,emu mathl cl -Lturboc3\tc\lib
