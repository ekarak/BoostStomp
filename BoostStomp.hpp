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

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/xpressive/xpressive.hpp>

#include "StompFrame.hpp"
#include "helpers.h"

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

	using namespace boost;
	using namespace boost::asio;
	using namespace boost::asio::ip;
	using namespace boost::xpressive;

    // ACK mode
    typedef enum {
        ACK_AUTO=0, // implicit acknowledgment (no ACK is sent)
        ACK_CLIENT,  // explicit acknowledgment (must ACK)
        ACK_CLIENT_INDIVIDUAL //
    } AckMode;

    // Stomp message callback function prototype
    typedef void (*pfnOnStompMessage_t)( Frame* _frame );

    // Stomp subscription map (topic => callback)
    typedef std::map<std::string, pfnOnStompMessage_t> subscription_map;

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
            subscription_map    m_subscriptions;

		 	tcp::socket* 		m_socket;
        //
            std::string         m_hostname;
            int                 m_port;
            AckMode             m_ackmode;
            //
            bool		m_stopped;
            bool		m_connected; // have we completed application-level STOMP connection?

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
            string	m_protocol_version;
            int 	m_transaction_id;
            //
            bool send_frame( Frame& _frame );
            bool do_subscribe (string& topic);
            //

            void consume_frame(Frame& _rcvd_frame);

            void start_connect(tcp::resolver::iterator endpoint_iter);
            void handle_connect(const boost::system::error_code& ec, tcp::resolver::iterator endpoint_iter);

            void start_stomp_connect(tcp::resolver::iterator endpoint_iter);

            //TODO: void setup_stomp_heartbeat(int cx, int cy);

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
            // constructor
            BoostStomp(string& hostname, int& port, AckMode ackmode = ACK_AUTO);
            // destructor
            ~BoostStomp();

            void start();
            void stop();

            // thread-safe methods called from outside the thread loop
            bool send      ( std::string& topic, hdrmap _headers, std::string& body );
            bool send      ( std::string& topic, hdrmap _headers, std::string& body, pfnOnStompMessage_t callback );
            bool subscribe 	( std::string& topic, pfnOnStompMessage_t callback );
            bool unsubscribe ( std::string& topic );
            bool acknowledge ( Frame& _frame );
            // STOMP transactions
            int  begin(); // returns a new transaction id
            bool commit(string& transaction_id);
            bool abort(string& transaction_id);
            //
            AckMode get_ackmode() { return m_ackmode; };
            //
    }; //class
    

}


#endif
