#pragma once
#include <stdio.h>

bool flash_read(void *data, int len);
bool flash_write(void *data, int len);
