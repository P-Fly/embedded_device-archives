How to use it?

This demo will generate 3 files: log.str example exe_decoder.

 - log.str records all the format strings in this demo.
 - example is an executable file that outputs some encoded log information.
 - exe_decoder decodes this log information and prints it by the standard output.

Build in this directory:

make

Run the encoder and decoder in this directory, where log.str is available:

./example | ./exe_decoder

Expected output for the original five small-value samples:

[M:TEST C:info ]: This is test 1, value = 55.
[M:TEST C:info ]: This is test 2, ch = A.
[M:TEST C:info ]: This is test 3, data = 3.
[M:TEST C:info ]: This is test 4, str = Hello, data = 3.
[M:TEST C:info ]: This is test 5, str = Hello, data = WORLD.

Remove generated files:

make clean
