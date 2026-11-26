# PoC-002 W0 ABI test. Usage (from this directory):
#   make                  build and run the full matrix via run-abi.sh
#   make build-O2         build only build/O2/abi_test
#   make static-checks    groups A3/A4 (scripts), B (objdump), function list
# Variants: O0, O2, asan (address + undefined + float-cast-overflow), cnt (O2 plus allocation
# counters via --wrap), vg (O0, the binary run under valgrind by run-valgrind.sh).

REPO := $(abspath ../../..)
LIB := $(REPO)/dist/libwebui-2-static.a
ifeq ($(origin CC),default)
CC := gcc
endif
WARN := -Wall -Wextra

C89 := -std=c89 -pedantic-errors $(WARN) -I.
C99 := -std=c99 $(WARN) -I$(REPO)/include -I.
I64 := -std=c99 -pedantic-errors $(WARN) -I.

OPT_O0 := -O0 -g
OPT_O2 := -O2 -g
OPT_asan := -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all

OPT_cnt := -O2 -g
LDX_cnt := -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free
DEF_cnt := -DABI_COUNTERS
OPT_vg := -O0 -g

VARIANTS := O0 O2 asan cnt vg

.PHONY: all lib static-checks clean $(addprefix build-,$(VARIANTS))

all: static-checks
	./run-abi.sh

$(LIB):
	$(MAKE) -C $(REPO)

lib: $(LIB)

abi_enums_gen.h: check_protos.py hdrparse.py $(REPO)/include/webui.h
	python3 check_protos.py --gen-enums > /dev/null

static-checks: abi_enums_gen.h
	python3 gen_functions.py
	python3 check_protos.py
	python3 check_bool_abi.py

define VARIANT_RULES
build-$(1): build/$(1)/abi_test

build/$(1)/abi_bridge.o: abi_bridge.c bridge_decl.h bridge_consts.h bridge_num.h
	@mkdir -p build/$(1)
	$$(CC) $$(C89) $$(OPT_$(1)) -c abi_bridge.c -o $$@

build/$(1)/abi_real.o: abi_real.c bridge_consts.h abi_enums_gen.h $(REPO)/include/webui.h
	@mkdir -p build/$(1)
	$$(CC) $$(C99) $$(OPT_$(1)) -c abi_real.c -o $$@

build/$(1)/abi_i64.o: abi_i64.c
	@mkdir -p build/$(1)
	$$(CC) $$(I64) $$(OPT_$(1)) -c abi_i64.c -o $$@

build/$(1)/abi_counters.o: abi_counters.c
	@mkdir -p build/$(1)
	$$(CC) -std=c99 $$(WARN) $$(OPT_$(1)) $$(DEF_$(1)) -c abi_counters.c -o $$@

build/$(1)/abi_test: build/$(1)/abi_bridge.o build/$(1)/abi_real.o build/$(1)/abi_i64.o build/$(1)/abi_counters.o $(LIB)
	$$(CC) $$(OPT_$(1)) $$(LDX_$(1)) build/$(1)/abi_bridge.o build/$(1)/abi_real.o build/$(1)/abi_i64.o build/$(1)/abi_counters.o $(LIB) -lpthread -lm -ldl -o $$@
endef

$(foreach v,$(VARIANTS),$(eval $(call VARIANT_RULES,$(v))))

clean:
	rm -rf build
