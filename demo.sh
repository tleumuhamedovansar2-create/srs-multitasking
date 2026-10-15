# СРС «Многозадачность в Linux» — вариант 8

Тлеумухамедов Ансар Нурланович, группа ИДу-26. Дисциплина «Системное программирование».

| Задание | Каталог | Что внутри | Запуск |
|---|---|---|---|
| 1. Процессы и планировщик | `task1/` | `procinfo` (общая часть), `procfd` — открытые дескрипторы процесса | `make && ./procinfo && ./procfd -demo` |
| 2. Процессы, каналы, сигналы | `task2/` | клиент-сервер на FIFO (`server`, `client`, `demo.sh`) | `make && ./demo.sh` |
| 3. Потоки | `task3/` | `race` (гонка + 3 исправления), `msort` — параллельная сортировка слиянием | `make && ./race none 5 && make tsan && make bench` |
| 4. Расширенная многозадачность | `task4/` | `shmcount` — межпроцессный мьютекс vs именованный семафор | `make && make run && make bench` |

Сборка: `gcc -Wall -Wextra -std=c11 -pthread`, в каждом каталоге свой Makefile.
Для графиков (`make bench`) нужен `python3-matplotlib`: `sudo apt install python3-matplotlib`.
Для `make helgrind` нужен valgrind: `sudo apt install valgrind`; для pstree — пакет `psmisc`.
