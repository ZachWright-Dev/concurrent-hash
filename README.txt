Concurrent Hash Table
======================

Build
-----
1. Run `make` to compile all sources into the `chash` executable.
2. Run `make clean` to remove the executable, hash.log, and object files.

Run
---
- Place your workload file at the project root with the name `commands.txt`.
- Execute `./chash` (or `chash.exe` on Windows). The program automatically reads `commands.txt`, writes diagnostic logs to `hash.log`, and prints command feedback and database dumps to stdout.

Notes
-----
- Logging follows the format described in the assignment, including timestamps, per-thread state changes, and lock acquisition/release events.
- The hash table employs a Jenkins one-at-a-time hash and maintains the records in sorted order by hash to simplify deterministic printing.
- Reader/writer locks (`pthread_rwlock_t`) protect the shared list, and a condition variable enforces command priority ordering while still using multiple threads.
- Update `commands.txt` with your own data to match grading workloads. A comprehensive sample file mirroring the provided workload is included for convenience.
