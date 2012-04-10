/*
 BoostStomp - a STOMP (Simple Text Oriented Messaging Protocol) client using BOOST (http://www.boost.org)
----------------------------------------------------
Copyright (c) 2012 Elias Karakoulakis <elias.karakoulakis@gmail.com>

SOFTWARE NOTICE AND LICENSE

BoostStomp is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.

Thrift4OZW is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with BoostStomp.  If not, see <http://www.gnu.org/licenses/>.

for more information on the LGPL, see:
http://en.wikipedia.org/wiki/GNU_Lesser_General_Public_License
*/

//	BoostStomp.h
// 

#ifndef __BoostStomp_H_
#define __BoostStomp_H_

#include <string>
#include <sstream>
#include <iostream>
#include <queue>
#include <map>
#include <set>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/xpressive/xpressive.hpp>

#include "StompFrame.hpp"

namespace STOMP {

	using namespace boost::asio;
	using namespace boost::asio::ip;
	using namespace boost::xpressive;

    // ACK mode
    typedef enum {
        ACK_AUTO=0, // implicit acknowledgment (no ACK is sent)
        ACK_CLIENT  // explicit acknowledgment (must ACK)
    } AckMode;

    // helper template function for pretty-printing just about anything
    template <class T>
    std::string to_string(T t, std::ios_base & (*f)(std::ios_base&))
    {
      std::ostringstream oss;
      oss.setf (std::ios_base::showbase);
      oss << f << t;
      return oss.str();
    }
    
    // Stomp message callback function prototype
    typedef void (*pfnOnStompMessage_t)( Frame* _frame );


	static mark_tag tag_command(1), tag_headers(2), tag_body(3);
	static mark_tag tag_key(1), tag_value(2);

	// this regex finds a STOMP server frame
	static cregex re_stomp_client_command = as_xpr("CONNECT")  | "DISCONNECT"
		| "SEND" | "SUBSCRIBE" | "UNSUBSCRIBE"
		| "BEGIN" | "COMMIT" | "ABORT"
		| "ACK" | "NACK" ;
	static cregex re_stomp_server_command = as_xpr("CONNECTED")
		| "MESSAGE" | "RECEIPT" | "ERROR"
		| "ACK" | "NACK" ;
	static cregex re_stomp_server_frame  = bos >> (tag_command= re_stomp_server_command ) >> _n // command and newline
						>> (tag_headers= -+(-+_ >> ':' >> -+_ >> _n)) >> _n  // tag_headers
						>> (tag_body= *_) >> eos;

    // here we go
    class BoostStomp
    {        
        //----------------
        protected:
        //----------------
            io_service&             m_io_service;
            tcp::socket* 			m_socket;
        //
            std::string             m_hostname;
            int                     m_port;
            AckMode                 m_ackmode;
            //

            std::queue<Frame>     m_sendqueue;
            std::map<std::string, pfnOnStompMessage_t>   m_subscriptions;
            //
            // ASIO handlers
            void handle_server_response(const boost::system::error_code& err, Frame& frame);
            void handle_subscribe_response(const boost::system::error_code& err, Frame& frame);
            
        //----------------
        private:
        //----------------
            boost::asio::streambuf stomp_request;
            boost::asio::streambuf stomp_response;

            bool send_frame( Frame& _frame );
            Frame* parse_response(boost::asio::streambuf& rawdata);

        //----------------
        public:
        //----------------
            //BoostStomp(boost::asio::io_service& io_service, const std::string& hostname, const int port);
            BoostStomp(boost::asio::io_service& io_service, string& hostname, int& port, AckMode ackmode = ACK_AUTO);
            ~BoostStomp();
            // thread-safe methods called from outside the thread loop

            bool subscribe ( std::string& topic, pfnOnStompMessage_t callback );
            bool send      ( std::string& topic, hdrmap _headers, std::string& body );
            //
            //BoostStompState& get_state() { return m_fsm.getState(); };
            AckMode get_ackmode() { return m_ackmode; };
            //
            void debug_print(std::string message);
    }; //class
    
}
#endif
