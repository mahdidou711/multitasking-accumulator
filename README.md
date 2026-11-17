# Multitasking Accumulator

POSIX multithreading in C11 on Linux: four producer threads feed a bounded buffer, one adder thread accumulates the messages, and one display thread prints the running sum and periodic diagnostics.

## Project overview

This is the coursework for the master's practical work "Conception et implémentation d'une application multitâche" (exercise 1, 2026 edition). The goals stated in the assignment are to design multitasking applications with an event-driven approach, to use POSIX mechanisms on Linux, and to use C11 mechanisms.

## Origin and attribution

This repository is derived from [`fthomasfr/multitasking_training_practical_work`](https://github.com/fthomasfr/multitasking_training_practical_work) by Frédéric Thomas (MIT License). The history of the original repository is preserved.

**Provided by the instructor:** the project skeleton (interface headers, `msg.c`, `sensorManager.c`, `display.c`, the `preambule.c` example) and the assignment PDF.

**Author's contributions (Mehdi Bouama):**

- design and implementation of exercise 1 in `acquisitionManagerPOSIX.c`, `acquisitionManagerAtomic.c`, `acquisitionManagerTestAndSet.c`, `messageAdder.c` and `displayManager.c`, with the related changes to `multitaskingAccumulator.c`, `preambule.c` and the interface headers;
- the requirements-allocation diagram and the UML class and sequence diagrams (`exercice-1/diagrams/`), including the two preamble sequence diagrams (provided program and corrected version);
- the reorganization into `src/`, `include/`, `docs/`, the Makefile changes and `renderQ6.sh`.

## Features

- Four producer threads acquire message blocks through a bounded buffer of four slots.
- The adder thread checks each block's checksum before adding it, and counts rejected blocks.
- The display thread prints each published sum and a periodic diagnostic (every 3 seconds by default).
- Orderly shutdown: producers finish, the buffer is drained, then the adder and display threads end. If initialization fails, the created tasks are aborted and joined.

## Software architecture

| Component | Source | Role |
|---|---|---|
| `AcquisitionManager` (POSIX) | `acquisitionManagerPOSIX.c` | producers and shared buffer; acquisition counter protected by a mutex |
| `AcquisitionManager` (atomic) | `acquisitionManagerAtomic.c` | same buffer; C11 atomic acquisition counter |
| `AcquisitionManager` (test-and-set) | `acquisitionManagerTestAndSet.c` | same buffer; acquisition counter protected by a C11 compare-exchange lock |
| `MessageAdder` | `messageAdder.c` | consumer thread, sum, rejections, diagnostic capture |
| `DisplayManager` | `displayManager.c` | display thread, publications and diagnostics |
| Provided | `msg.c`, `display.c`, `sensorManager.c` | message operations, output, sensor input |

Interfaces are in `exercice-1/include/` (`iAcquisitionManager.h`, `iMessageAdder.h`, `iDisplay.h`, `iSensor.h`). PlantUML and draw.io sources are in `exercice-1/diagrams/`; the PNG files are generated and not tracked.

## Concurrency and synchronization

- **Threads:** the main thread, four producers, one adder and one display thread.
- **Buffer:** four slots with a life cycle (free, reserved, ready, being read). A stack holds the free indices and a FIFO holds the ready indices in publication order. One mutex (`mAcq`) and two condition variables (`cvFree`, `cvReady`) protect the metadata. A slot is written or copied outside the mutex only while one thread owns it.
- **Acquisition counter:** the POSIX implementation uses a mutex; the atomic variant uses a C11 atomic counter; the test-and-set variant uses a C11 compare-exchange lock.
- **Adder:** one mutex (`mAdder`) and two condition variables: `cvCommit`, and `cvDisplay` configured on `CLOCK_MONOTONIC`.
- **Diagnostics:** the display thread waits for the next event, with a priority order between abort, end, diagnostic due and publication. The adder freezes the counters while the diagnostic is captured.
- **Shutdown:** a `closed` flag and an `aborted` flag, with condition-variable broadcasts, wake every waiting thread.

## Build and execution

Requirements: Linux x86-64 (the Makefile uses `-mcx16`), a C compiler with POSIX threads, GNU Make. Docker is optional (diagrams).

```sh
cd exercice-1
make preambule && ./build/bin/preambule     # semaphore and thread example
make posix && ./build/bin/multitaskingAccumulatorPosix
make atomic && ./build/bin/multitaskingAccumulatorAtomic
make testandset && ./build/bin/multitaskingAccumulatorTestAndSet
```

| Target | Effect |
|---|---|
| `make preambule`, `make posix`, `make atomic`, `make testandset` | build into `exercice-1/build/bin/` |
| `make runpreambule`, `make runposix`, `make runatomic`, `make runtestandset` | remove `build/`, rebuild and run the corresponding program |
| `make pumls` | renders `diagrams/*.puml` with the `ghcr.io/plantuml/plantuml` Docker image |
| `sh exercice-1/diagrams/renderQ6.sh` | renders the named Q6 views (plantuml, a jar, or Docker) |

`make test`, `make testbins` and `make testtsan` need a `tests/` directory that is not published (see Verification).

The Q6 views are rendered by `renderQ6.sh`, not by `make pumls`: the generic pipe recipe does not produce all the named views.

## Project structure

```
exercice-1/
  Makefile
  docs/tp_part1.pdf          assignment
  include/                   interface and module headers
  src/                       C sources
  diagrams/                  PlantUML, draw.io, renderQ6.sh
```

## Verification

From `exercice-1/`, build and run the four programs, then remove build artifacts:

```sh
make clean preambule posix atomic testandset
./build/bin/preambule
./build/bin/multitaskingAccumulatorPosix
./build/bin/multitaskingAccumulatorAtomic
./build/bin/multitaskingAccumulatorTestAndSet
make clean
```

**Local verification, not published:** a local test bench (scenarios for acquisition, rejection, diagnostics and shutdown, plus a ThreadSanitizer run) is kept outside this repository. It cannot be reproduced from here, and no result of it is claimed.

## Known limitations

- **Requirement E5.** `IDisplay.print` takes only (produced, consumed, left). The diagnostic sends `print(A, S+R)` and adds a complement line `A, S, R, B` outside `IDisplay`. With the provided `sensorManager.c` every checksum is valid (R = 0), so `print(A, S+R)` equals `print(A, S)` and E5 is met literally through `print`. Literal conformance is impossible only once rejections occur (R > 0, for example with injected faults); this design interpretation remains open.
- x86-64 only (`-mcx16`); `CLOCK_MONOTONIC` condition variables are required.
- The test bench is not published.

## Technologies

C11, POSIX threads, mutexes and condition variables, GNU Make, PlantUML, draw.io. POSIX semaphores appear only in the preamble example.

## License and attribution

Original work: Copyright (c) 2017-2025 Frédéric Thomas, MIT License (see `LICENSE`). Modifications and additions by Mehdi Bouama, 2026, are released under the same MIT License. The assignment PDF is distributed with the instructor's repository; its reuse is subject to the instructor's terms.
