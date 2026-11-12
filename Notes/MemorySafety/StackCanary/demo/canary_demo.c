#include <stdio.h>
#include <string.h>

__attribute__((noinline))
static void copy_text(const char *text)
{
    char buffer[16];

    strcpy(buffer, text);
    fputs("copy finished\n", stderr);
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        return 2;
    }

    copy_text(argv[1]);
    fputs("returned to main\n", stderr);
    return 0;
}