# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#
CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -I. -Ilib/include $(EXTRA_CFLAGS)

lib-objs-y = lib/rv_etrace_bits.o
lib-objs-y += lib/rv_etrace_payload.o
lib-objs-y += lib/rv_etrace_encap.o
lib-objs-y += lib/rv_etrace_itrace.o
lib-objs-y += lib/rv_etrace_display.o

test-y = test/test_encap.elf
test-y += test/test_itrace_encap.elf
test-y += test/test_etrace_pktdump.elf

# The default "make all" rule
.PHONY: all
all: $(test-y)

# Preserve all intermediate files
.SECONDARY:

%.elf: %.o $(lib-objs-y)
	$(CC) $(CFLAGS) $< $(lib-objs-y) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# The default "make clean" rule
.PHONY: clean
clean:
	rm -f lib/*.o
	rm -f test/*.o
	rm -f test/*.elf
