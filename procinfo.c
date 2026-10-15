CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -pthread -O2
TARGETS = procinfo procfd

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	./procinfo
	./procfd -demo

clean:
	rm -f $(TARGETS) procfd_demo.txt

.PHONY: all run clean
