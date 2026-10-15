CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -pthread -O2

all: race msort race_tsan

race: race.c
	$(CC) $(CFLAGS) -o $@ $<

msort: msort.c
	$(CC) $(CFLAGS) -o $@ $<

# Сборка с ThreadSanitizer для обнаружения гонки
race_tsan: race.c
	$(CC) -Wall -Wextra -std=c11 -pthread -g -O1 -fsanitize=thread -o $@ $<

race_dbg: race.c
	$(CC) -Wall -Wextra -std=c11 -pthread -g -O0 -o $@ $<

tsan: race_tsan
	./race_tsan none 1

helgrind: race_dbg
	valgrind --tool=helgrind ./race_dbg none 1

bench: msort
	./msort bench | tee bench.csv
	python3 plot.py bench.csv speedup.png

clean:
	rm -f race msort race_tsan race_dbg bench.csv speedup.png

.PHONY: all tsan helgrind bench clean
