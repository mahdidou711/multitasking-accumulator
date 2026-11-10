.POSIX:

# ============================================================
# Configuration du compilateur
# ============================================================

CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -O3 -mcx16 -pthread #-DDEBUG
LDFLAGS = -pthread
#LDLIBS  = -latomic

# ============================================================
# Organisation des répertoires
# ============================================================

SRC_DIR     = src
INCLUDE_DIR = include
BUILD_DIR   = build
OBJ_DIR     = $(BUILD_DIR)/obj
BIN_DIR     = $(BUILD_DIR)/bin

CPPFLAGS = -I$(INCLUDE_DIR)

# ============================================================
# Fichiers sources et dépendances
# ============================================================

C = $(wildcard $(SRC_DIR)/*.c)
H = $(wildcard $(INCLUDE_DIR)/*.h)

PREAMBULE_SRCS := $(SRC_DIR)/preambule.c

POSIX_SRCS := $(filter-out \
    $(SRC_DIR)/preambule.c \
    $(SRC_DIR)/%Atomic.c \
    $(SRC_DIR)/%TestAndSet.c, $(C))

ATOMIC_SRCS := $(filter-out \
    $(SRC_DIR)/preambule.c \
    $(SRC_DIR)/%POSIX.c \
    $(SRC_DIR)/%TestAndSet.c, $(C))

TESTANDSET_SRCS := $(filter-out \
    $(SRC_DIR)/preambule.c \
    $(SRC_DIR)/%POSIX.c \
    $(SRC_DIR)/%Atomic.c, $(C))

PREAMBULE_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(PREAMBULE_SRCS))
POSIX_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(POSIX_SRCS))
ATOMIC_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(ATOMIC_SRCS))
TESTANDSET_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(TESTANDSET_SRCS))

# ============================================================
# Diagrammes PlantUML
# ============================================================

PUMLS := $(wildcard diagrams/*.puml)
PNGS := $(PUMLS:.puml=.png)

# ============================================================
# Cibles principales
# ============================================================

.PHONY: preambule posix atomic testandset
.PHONY: clean runpreambule runposix runatomic runtestandset pumls

preambule: $(BIN_DIR)/preambule

posix: $(BIN_DIR)/multitaskingAccumulatorPosix

atomic: $(BIN_DIR)/multitaskingAccumulatorAtomic

testandset: $(BIN_DIR)/multitaskingAccumulatorTestAndSet

# ============================================================
# Compilation des fichiers objets
# ============================================================

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(H) | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# ============================================================
# Édition des liens
# ============================================================

$(BIN_DIR)/preambule: $(PREAMBULE_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $(PREAMBULE_OBJS) $(LDLIBS)

$(BIN_DIR)/multitaskingAccumulatorPosix: $(POSIX_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $(POSIX_OBJS) $(LDLIBS)

$(BIN_DIR)/multitaskingAccumulatorAtomic: $(ATOMIC_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $(ATOMIC_OBJS) $(LDLIBS)

$(BIN_DIR)/multitaskingAccumulatorTestAndSet: $(TESTANDSET_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $(TESTANDSET_OBJS) $(LDLIBS)

# ============================================================
# Nettoyage
# ============================================================

clean:
	rm -rf $(BUILD_DIR)

# ============================================================
# Exécution
# ============================================================

runpreambule: clean preambule
	./$(BIN_DIR)/preambule

runposix: clean posix
	./$(BIN_DIR)/multitaskingAccumulatorPosix

runatomic: clean atomic
	./$(BIN_DIR)/multitaskingAccumulatorAtomic

runtestandset: clean testandset
	./$(BIN_DIR)/multitaskingAccumulatorTestAndSet

# ============================================================
# Génération des diagrammes PlantUML
# ============================================================

pumls: $(PNGS)

diagrams/%.png: diagrams/%.puml
	@tmp=$$(mktemp "$@.tmp.XXXXXX") || exit 1; \
	trap 'rm -f "$$tmp"' 0; \
	docker run --rm -i ghcr.io/plantuml/plantuml -pipe < $< > "$$tmp" && mv -f "$$tmp" $@
