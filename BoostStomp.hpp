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

#include "StompFrame.hpp"

// datatypes used inside the FSM
namespace STOMP {

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
}

using namespace boost::asio;
using namespace boost::asio::ip;

namespace STOMP {
    
    // Stomp message callback function prototype
    typedef void (*pfnOnStompMessage_t)( Frame* _frame );

    // here we go
    class BoostStomp
    {        
        //----------------
        protected:
        //----------------
            io_service&             m_io_service;
            tcp::resolver           m_resolver;
            tcp::socket             m_socket;
        //
            std::string             m_hostname;
            int                      m_port;
            //Connection*     m_connection;
            AckMode                 m_ackmode;
            //
        /*
            Poco::Thread*           m_thread;
            Poco::Mutex*             m_mutex;
            Poco::Mutex*            m_initcond_mutex;
            Poco::Condition*        m_initcond; */
            std::queue<Frame>     m_sendqueue;
            std::map<std::string, pfnOnStompMessage_t>   m_subscriptions;
            //
            //  Set of valid STOMP server commands
            std::set<std::string> m_stomp_server_commands;
	
            // ASIO handlers
            void handle_resolve(const boost::system::error_code& err, ip::tcp::resolver::iterator endpoint_iterator);
            void handle_connect(const boost::system::error_code& err, ip::tcp::resolver::iterator endpoint_iterator);
            void handle_server_response(const boost::system::error_code& err);
            
        //----------------
        private:
        //----------------
            boost::asio::streambuf stomp_request;
            boost::asio::streambuf stomp_response;

            void notify_callbacks(Frame* _frame);
        
        //----------------
        public:
        //----------------
            BoostStomp(boost::asio::io_service& io_service, const std::string& hostname, const int port);
            ~BoostStomp();
            // thread-safe methods called from outside the thread loop
            bool connect   ();
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
