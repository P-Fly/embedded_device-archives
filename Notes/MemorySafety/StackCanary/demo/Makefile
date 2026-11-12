SHELL := /bin/bash
CC := gcc
OBJDUMP := objdump
CFLAGS := -std=c11 -O0 -g -Wall -Wextra -fno-omit-frame-pointer \
          -fstack-protector-strong -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

.PHONY: all run check disasm clean

all: canary_demo

canary_demo: canary_demo.c Makefile
	$(CC) $(CFLAGS) $< -o $@

run: check

check: canary_demo
	@set -eu; \
	ulimit -c 0; \
	run_log=$$(mktemp); \
	trap 'rm -f "$$run_log"' EXIT; \
	status=0; \
	LC_ALL=C ./canary_demo "$$(printf 'A%.0s' {1..31})" >"$$run_log" 2>&1 || status=$$?; \
	cat "$$run_log"; \
	printf 'exit status: %d\n' "$$status"; \
	[[ "$$status" -eq 134 ]]; \
	grep -Fxq 'copy finished' "$$run_log"; \
	grep -Fq 'stack smashing detected' "$$run_log"; \
	if grep -Fq 'returned to main' "$$run_log"; then exit 1; fi; \
	printf 'PASS: canary failure detected before returning to main.\n'

disasm: canary_demo
	$(OBJDUMP) -d -Mintel --disassemble=copy_text $<

clean:
	$(RM) canary_demo