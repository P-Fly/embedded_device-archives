all:
	@echo "BUild..."
	@gcc main.c encoder.c -T x86.ld -o example
	@gcc decoder.c -o exe_decoder
	@objcopy --only-section=.log_string -O binary example log.str

clean:
	@echo "Clean..."
	@rm -rf *.o
	@rm -rf log.str example exe_decoder
