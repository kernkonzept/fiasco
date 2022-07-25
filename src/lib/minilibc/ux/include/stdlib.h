#pragma once

#include_next <stdlib.h>

int __ltostr(char *s, int size, unsigned long i, int base, int UpCase);
__extension__
int __lltostr(char *s, int size, unsigned long long i, int base, int UpCase);
