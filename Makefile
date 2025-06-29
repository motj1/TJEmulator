CC = clang

all: assembler emulator assemble
asrun: assemble run

assembler: src/assembler.c
	$(CC) -o assembler src/assembler.c

emulator: src/emulator.c
	$(CC) -o emulator src/emulator.c

assemble: $(fn)
	./assembler $(fn)

run: $(fn)
	./emulator $(fn)


########################################################################

clean:
	rm -f assembler emulator $(fn)