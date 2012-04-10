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
  BoostStomp::BoostStomp(boost::asio::io_service& io_service, string& hostname, int& port, AckMode ackmode /*= ACK_AUTO*/):
    m_io_service(io_service),
    m_hostname(hostname),
    m_port(port),
    m_ackmode(ackmode)
  // ----------------------------
  {
	m_socket = new tcp::socket(io_service);

    // initiate STOMP server hostname resolution
    tcp::resolver::query 	query(m_hostname, to_string<int>(m_port, std::dec), boost::asio::ip::resolver_query_base::numeric_service);
    // Get a list of endpoints corresponding to the server name.
    tcp::resolver 			resolver(m_io_service);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::resolver::iterator end;

    // Try each endpoint until we successfully establish a connection.
    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end)
    {
      m_socket->close();
      m_socket->connect(*endpoint_iterator++, error);
    }
    if (error)
      throw boost::system::system_error(error);

    //
    cout << "TCP connection to " << m_hostname << " successful!\n";

    // now we are connected to STOMP server's TCP port/
    // The protocol negotiation phase requires we send a CONNECT frame

    // The connection was successful. Send the CONNECT request.
    Frame frame( "CONNECT" );
    boost::asio::write( *m_socket, frame.encode_request() );
    // Wait for server response ("CONNECTED")
    //boost::asio::read_until( *m_socket, frame.response,  "\n\n");
    boost::asio::read_until( *m_socket, frame.response,  "\0");

    /*if (frame.response == "CONNECTED") {
    	cout << "STOMP connection to " << m_hostname << " successful!\n";
    }*/

  }

  BoostStomp::~BoostStomp() {
	  delete m_socket;
  }

  bool BoostStomp::subscribe( string& topic, pfnOnStompMessage_t callback )
  {
	  hdrmap hm;
	  hm["destination"] = topic;
	  Frame frame( "SUBSCRIBE", hm );
	  m_subscriptions[topic] = callback;
	  return(send_frame(frame));
  }
  
  bool BoostStomp::send( string& topic, hdrmap _headers, std::string& body )
  {
	  _headers["destination"] = topic;
	  Frame frame( "SEND", _headers, body );
	  return(send_frame(frame));
  }

  bool BoostStomp::send_frame( Frame& frame )
  {
	 // io_service.post(boost::bind(&BoostStomp::do_write, this, msg));
	  boost::asio::write( *m_socket, frame.encode_request() );
	  // Wait for server response
	  boost::asio::async_read_until(
		  *m_socket,
		  frame.response,
		  "\0", // "\n\n"
		  boost::bind(
			  &BoostStomp::handle_server_response,
			  this,
			  boost::asio::placeholders::error,
			  frame
		  )
	   );
	  return(true);
  }

  // ----------------------------
  void BoostStomp::debug_print(std::string message)
  // ----------------------------
  {
  #ifdef DEBUG_STOMP
      std::cout << "DEBUG: " << message << std::endl;
  #endif
  }
    
  // ----------------------------
  // ASIO HANDLERS (protected)
  // ----------------------------

  
  // ----------------------------
  void BoostStomp::handle_server_response(const boost::system::error_code& err, Frame& frame) {
  // ----------------------------
	  if (!err)
	  {
		  std::cout << "received server response: " << frame.response.size() << " bytes" << endl;
		  std::cout << &(frame.response);
		  Frame* rcvd_frame = parse_response(frame.response);
		  if (rcvd_frame) {
			  if (rcvd_frame->command() == "MESSAGE") {
				  string* dest = new string(rcvd_frame->headers()["destination"]);
				  std::cout << "notify_callbacks dest=" << dest << std::endl;
				  //
				  if (pfnOnStompMessage_t callback_function = m_subscriptions[*dest]) {
					  callback_function(rcvd_frame);
				  };
			  }
			  // dispose frame, its not needed anymore
			  delete rcvd_frame;
		  } else {
			std::cerr << "ERRROR in handle_Server_response" << endl;
		  }
	  }
  }

  // ----------------------------
  void BoostStomp::handle_subscribe_response(const boost::system::error_code& err, Frame& frame) {
  // ----------------------------
	if (!err)
	{
	  std::cout << "received server response: " << frame.response.size() << " bytes" << endl;
	  std::cout << &(frame.response);

	} else {
		std::cerr << "ERRROR in handle_Server_response" << endl;
	  //
	}
  }

  // ----------------------------
  Frame* BoostStomp::parse_response(boost::asio::streambuf& rawdata) {
  // ----------------------------
	    cmatch regex_match;
	    std::istream is(&rawdata);
	    std::string str;
	    is >> str;
	    if( regex_search( str.c_str(), regex_match, re_stomp_server_frame ) )
	    {
	    	// construct header map from match
	    	hdrmap headermap;
	        std::string str = std::string(regex_match[tag_headers]);
	        sregex header_token = (tag_key= -+alnum) >> ':' >> (tag_value= -+alnum) >> _n;
	        sregex_iterator cur( str.begin(), str.end(), header_token );
	        sregex_iterator end;
			for( ; cur != end; ++cur ) {
				smatch const &h = *cur;
				headermap[h[tag_key]] = h[tag_value];
			}
			// construct new Frame, return it
	        return(new Frame(regex_match[tag_command], headermap, regex_match[tag_body]));
	    } else {
	        std::cout << "NO MATCH!" << std::endl;
	    }
	    return(NULL);
  };

} // end namespace STOMP
