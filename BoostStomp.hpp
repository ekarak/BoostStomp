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
#include <boost/asio/deadline_timer.hpp>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/xpressive/xpressive.hpp>

#include "StompFrame.hpp"

// helper template function for pretty-printing just about anything
template <class T>
std::string to_string(T t, std::ios_base & (*f)(std::ios_base&))
{
  std::ostringstream oss;
  oss.setf (std::ios_base::showbase);
  oss << f << t;
  return oss.str();
}

namespace STOMP {

	using namespace boost::asio;
	using namespace boost::asio::ip;
	using namespace boost::xpressive;

    // ACK mode
    typedef enum {
        ACK_AUTO=0, // implicit acknowledgment (no ACK is sent)
        ACK_CLIENT  // explicit acknowledgment (must ACK)
    } AckMode;
    
    // Stomp message callback function prototype
    typedef void (*pfnOnStompMessage_t)( Frame* _frame );

    // here we go
	// -------------
    class BoostStomp
    // -------------
    {        
        //----------------
        protected:
        //----------------

            std::queue<Frame>   m_sendqueue;
            boost::mutex        m_sendqueue_mutex;
            std::map<std::string, pfnOnStompMessage_t>   m_subscriptions;

		 	tcp::socket* 		m_socket;
        //
            std::string         m_hostname;
            int                 m_port;
            AckMode             m_ackmode;
            //
            bool		m_stopped;
            bool		m_connected;

            boost::shared_ptr< io_service > 	m_io_service;
			//boost::shared_ptr< io_service::work > m_io_service_work;

        //----------------
        private:
        //----------------
            boost::asio::streambuf stomp_request;
            boost::asio::streambuf stomp_response;
            boost::mutex 			stream_mutex;
            boost::thread*		worker_thread;
            boost::shared_ptr<deadline_timer>	m_heartbeat_timer;
            boost::asio::streambuf m_heartbeat;

            bool send_frame( Frame& _frame );
            vector<Frame*> 	parse_response	();
            Frame* 			parse_frame		(smatch const framestr);
            //

            void consume_frame(Frame& _rcvd_frame);

            void start(tcp::resolver::iterator endpoint_iter);
            void stop();

            void start_connect(tcp::resolver::iterator endpoint_iter);
            void handle_connect(const boost::system::error_code& ec, tcp::resolver::iterator endpoint_iter);

            void start_stomp_connect(tcp::resolver::iterator endpoint_iter);
            void start_stomp_heartbeat();
            void handle_stomp_heartbeat(const boost::system::error_code& ec);

            void start_stomp_read();
            void handle_stomp_read(const boost::system::error_code& ec);

            void start_stomp_write();
            void handle_stomp_write(const boost::system::error_code& ec);

            void worker( boost::shared_ptr< boost::asio::io_service > io_service );
        //----------------
        public:
        //----------------
            //BoostStomp(boost::asio::io_service& io_service, const std::string& hostname, const int port);
            BoostStomp(string& hostname, int& port, AckMode ackmode = ACK_AUTO);
            ~BoostStomp();
            // thread-safe methods called from outside the thread loop

            bool subscribe ( std::string& topic, pfnOnStompMessage_t callback );
            bool send      ( std::string& topic, hdrmap _headers, std::string& body );
            //
            //BoostStompState& get_state() { return m_fsm.getState(); };
            AckMode get_ackmode() { return m_ackmode; };
            //
    }; //class
    
    // helper function
    void hexdump(const void *ptr, int buflen);
    void debug_print(boost::format& fmt);
    void debug_print(string& str);
    void debug_print(const char* str);

}


#endif
