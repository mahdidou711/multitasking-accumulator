# Practical Work

This is the Multitasking Training Practical work materials for master students.

The exercise instructions are in `exercice-1/docs/tp_part1.pdf`.

## Build and run

On a Linux x86-64 system, install GNU Make and a C compiler with POSIX threads.
The Makefile uses `-mcx16` and places generated files in `exercice-1/build/`.

```sh
cd exercice-1
make preambule
./build/bin/preambule
make posix
```

The POSIX target builds the exercise skeleton; a successful build does not mean
that the exercise is complete. The other implementation targets are intended for
later work.

## Diagrams

The editable PlantUML and Draw.io sources are in `exercice-1/diagrams/`.
To regenerate the PlantUML PNGs, start Docker and run:

```sh
cd exercice-1
make pumls
```

The Makefile uses the `ghcr.io/plantuml/plantuml` image. Generated PNGs and
build files are not tracked by Git.

Q6 splits the class and sequence diagrams into named views for the report.
Render all six views with the dedicated command, which preserves the preamble
and Q1 images:

`sh exercice-1/diagrams/renderQ6.sh`

The script uses, in order: a `plantuml` command on the `PATH`; or the PlantUML
jar named by `PLANTUML_JAR` (requires `java`); or Docker as a fallback. Example:
`PLANTUML_JAR=/path/to/plantuml.jar sh exercice-1/diagrams/renderQ6.sh`.

Use this command for Q6 instead of `make pumls`: the generic pipe recipe does
not produce all the named views. Q6 documents the intended architecture; the C
sources and headers remain the exercise skeleton.
