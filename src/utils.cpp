#include "utils.h"

uint16_t get_checksum(void* temp_data, int len) {
  uint16_t* data = (uint16_t*) temp_data;
  uint32_t sum = 0;

  while(len>1) {
    sum += *data++;
    len -= 2;
  }

  if(len==1) {
    uint16_t left = *(uint8_t*) data;
    sum += left; 
  }

  while(sum>>16) {
    sum = (sum&(0xffff))+(sum>>16);
  }

  return (uint16_t)(~sum);
}