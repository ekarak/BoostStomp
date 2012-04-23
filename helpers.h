/*
 * helpers.h
 *
 *  Created on: 22 Απρ 2012
 *      Author: ekarak
 */

#ifndef HELPERS_H_
#define HELPERS_H_

#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <string>

using namespace std;

// helper function
void hexdump(boost::asio::streambuf&);
void hexdump(const void *ptr, int buflen);
void debug_print(boost::format& fmt);
void debug_print(string& str);
void debug_print(const char* str);



#endif /* HELPERS_H_ */
