/**
 * ESP8266 SPIFFS HAL configuration.
 *
 * Part of esp-open-rtos
 * Copyright (c) 2016 sheinz https://github.com/sheinz
 * MIT License
 */
#include "prelude.h"
#include "esp_spiffs.h"
#include "spiffs.h"
//#include "spiflash.h"
#include <fcntl.h>
#include <sys/reent.h>


spiffs fs;

int _fstat_r(struct _reent *r, int fd, void *buf) {
  spiffs_stat s;
  struct stat *sb = (struct stat*)buf;

  int result = SPIFFS_fstat(&fs, (spiffs_file)fd, &s);
  sb->st_size = s.size;

  return result;
}

isize _lseek_r(struct _reent *r, int fd, off_t offset, int whence) {
  return SPIFFS_lseek(&fs, (spiffs_file)fd, offset, whence);
}


