run: main.c my_print.asm
	nasm -f elf64 my_print.asm -o my_print.o
	gcc -o main  main.c my_print.o


clean: 
	rm -rf my_print.o
	rm -rf main