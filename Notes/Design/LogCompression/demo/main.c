#include "syslog.h"

void main(void)
{
    char ch = 'A';

    int data = 3;

    char* str = "Hello";

    LOG_MSGID_I(TEST, "This is test 1, value = %d.\n", 1, 55);

    LOG_MSGID_I(TEST, "This is test 2, ch = %c.\n", 1, ch);

    LOG_MSGID_I(TEST, "This is test 3, data = %d.\n", 1, data);

    LOG_MSGID_I(TEST, "This is test 4, str = %s, data = %d.\n", 2, str, data);
	
	LOG_MSGID_I(TEST, "This is test 5, str = %s, data = %s.\n", 2, str, "WORLD");
}
