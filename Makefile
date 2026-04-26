CC = gcc
CFLAGS = -Wall -g -Iinclude $(shell pkg-config --cflags glib-2.0)
LDFLAGS = $(shell pkg-config --libs glib-2.0)
all: folders controller runner
controller: bin/controller
runner: bin/runner
folders:
	@mkdir -p src include obj bin tmp
bin/controller: obj/controller.o obj/policies.o
	$(CC) $(LDFLAGS) $^ -o $@
bin/runner: obj/runner.o
	$(CC) $(LDFLAGS) $^ -o $@
obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f obj/* tmp/* bin/
