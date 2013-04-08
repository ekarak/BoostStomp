/*
 * helpers.cpp
 *
 *  Created on: 22 Απρ 2012
 *      Author: ekarak
 */


#include <helpers.h>
#include <iostream>

void hexdump(const void *ptr, int buflen) {
    unsigned char *buf = (unsigned char*)ptr;
    int i, j;
    for (i=0; i<buflen; i+=16) {
        printf("%06x: ", i);
        for (j=0; j<16; j++)
          if (i+j < buflen)
              printf("%02x ", buf[i+j]);
          else
              printf("   ");
        printf(" ");
        for (j=0; j<16; j++)
          if (i+j < buflen)
              printf("%c", isprint(buf[i+j]) ? buf[i+j] : '.');
        printf("\n");
    }
}

void hexdump(boost::asio::streambuf& sb) {
    const char* rawdata = boost::asio::buffer_cast<const char*>(sb.data());
    hexdump(rawdata, sb.size());
}


