# Building scripts

## Assemble 

nasm -f elf32 testfiles/hello.asm -o testfiles/hello.o
ld -m elf_i386 testfiles/hello.o -o testfiles/hello
