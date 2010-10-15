all: brain
clean:
	- rm -f *.o brain
%.o: %.c
	gcc -c -Wall $^ -I vm/ -I .
brain: mem.o loader.o vm.o
	gcc -o $@ $^
.PHONY: all clean
