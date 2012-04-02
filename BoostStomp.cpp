/*
 BoostStomp - a STOMP (Simple Text Oriented Messaging Protocol) client
----------------------------------------------------
Copyright (c) 2012 Elias Karakoulakis <elias.karakoulakis@gmail.com>

SOFTWARE NOTICE AND LICENSE

BoostStomp is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.

BoostStomp is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with BoostStomp.  If not, see <http://www.gnu.org/licenses/>.

for more information on the LGPL, see:
http://en.wikipedia.org/wiki/GNU_Lesser_General_Public_License
*/

#include <cstdlib>
#include <deque>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "BoostStomp.hpp"

namespace STOMP {
  
  using namespace std;
  using namespace boost::asio;
  using boost::asio::ip::tcp;
  
  typedef std::deque<Frame> stomp_frame_queue;
  // ----------------------------
  // constructor
  // ----------------------------
  BoostStomp::BoostStomp(boost::asio::io_service& io_service, const std::string& hostname, const int port):
    m_io_service(io_service),
    m_resolver(io_service),
    m_socket(io_service),
    m_hostname(hostname),
    m_port(port),
    m_ackmode(ACK_AUTO)
  // ----------------------------
  {
    // initiate STOMP server resolution
    tcp::resolver::query _query(m_hostname, to_string<int>(m_port, std::dec), boost::asio::ip::resolver_query_base::numeric_service);
    m_resolver.async_resolve( _query, 
      boost::bind(
        &BoostStomp::handle_resolve, 
        this, 
        boost::asio::placeholders::error, 
        boost::asio::placeholders::iterator
    ));
  }
  
  bool BoostStomp::connect()
  {
    return(0);
  }
    
  bool BoostStomp::subscribe( std::string& topic, pfnOnStompMessage_t callback )
  {
    return(0);
  }
  
  bool BoostStomp::send( std::string& topic, hdrmap _headers, std::string& body )
  {
    return(0);
  }
  
  void BoostStomp::notify_callbacks(Frame* _frame)
  {
  }

  // ----------------------------
  void BoostStomp::debug_print(std::string message)
  // ----------------------------
  {
  #ifdef DEBUG_STOMP
      std::cout << "FSM DEBUG: " << message << std::endl;
  #endif
  }
    
  // ----------------------------
  // private:
  
  // ----------------------------
  void BoostStomp::handle_resolve(const boost::system::error_code& err, tcp::resolver::iterator endpoint_iterator)
  // ----------------------------
  {
    if (!err)
    {
      // Attempt a connection to the first endpoint in the list. Each endpoint
      // will be tried until we successfully establish a connection.
      ip::tcp::endpoint endpoint = *endpoint_iterator;
      m_socket.async_connect( endpoint, boost::bind(
          &BoostStomp::handle_connect, 
          this,
          boost::asio::placeholders::error, 
          ++endpoint_iterator
      ));
    }
    else
    {
      std::cout << "Error: " << err.message() << "\n";
    }
  }
  
  // ----------------------------
  void BoostStomp::handle_connect (const boost::system::error_code& err,  ip::tcp::resolver::iterator endpoint_iterator)
  // ----------------------------
  {
    if (!err)
    {
      // The connection was successful. Send the CONNECT request.
      Frame _frame("CONNECT");
      boost::asio::write(m_socket, _frame.encoded().c_str());
      // Wait for server response
      boost::asio::async_read_until(m_socket, stomp_response, "\n\n",
          boost::bind(&BoostStomp::handle_server_response, this,
          boost::asio::placeholders::error
      ));
    }
    else if (endpoint_iterator != ip::tcp::resolver::iterator())
    {
      // The connection was unsuccessful. Close socket and retry.
      m_socket.close();
      tcp::endpoint endpoint = *endpoint_iterator;
      m_socket.async_connect(endpoint,
          boost::bind(&BoostStomp::handle_connect, this,
            boost::asio::placeholders::error, ++endpoint_iterator));
    }
  }
  
  // ----------------------------
  void handle_server_response(const boost::system::error_code& err)
  // ----------------------------
  {
    if (!err)
    {
      //
    } else {
      
      //
    }
  }
} // end namespace STOMP
