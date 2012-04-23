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

// based on the ASIO async TCP client example found on Boost documentation:
// http://www.boost.org/doc/libs/1_46_1/doc/html/boost_asio/example/timeouts/async_tcp_client.cpp

// #include <cstdlib>
// #include <deque>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>

#include "BoostStomp.hpp"

namespace STOMP {
  
  using namespace std;
  using namespace boost;
  using namespace boost::asio;
  using boost::asio::ip::tcp;

  // ----------------------------
  // constructor
  // ----------------------------
  BoostStomp::BoostStomp(string& hostname, int& port, AckMode ackmode /*= ACK_AUTO*/):
  // ----------------------------
    m_hostname(hostname),
    m_port(port),
    m_ackmode(ackmode),
    m_stopped(false),
    m_connected(false),
    m_protocol_version("1.0"),
    m_transaction_id(0)
  // ----------------------------
  {
	m_io_service = boost::shared_ptr< io_service > ( new io_service  );
	m_heartbeat_timer = boost::shared_ptr< deadline_timer> ( new deadline_timer( *m_io_service ));
	std::ostream os( &m_heartbeat);
	os << "\n";
  }


  // ----------------------------
  // destructor
  // ----------------------------
  BoostStomp::~BoostStomp()
  // ----------------------------
  {
	  worker_thread->interrupt();
	  delete m_socket;
  }

  // ----------------------------
  // worker thread
  // ----------------------------
  void BoostStomp::worker( boost::shared_ptr< boost::asio::io_service > io_service )
  {
	  debug_print("Worker thread starting...");
	  io_service->run();
	  debug_print("Worker thread finished.");
  }


  // ----------------------------
  // ASIO HANDLERS (protected)
  // ----------------------------


  // Called by the user of the client class to initiate the connection process.
  // The endpoint iterator will have been obtained using a tcp::resolver.
  void BoostStomp::start()
  {
	m_socket 		= new tcp::socket(*m_io_service);
	tcp::resolver 	resolver(*m_io_service);
	tcp::resolver::iterator endpoint_iter = resolver.resolve(tcp::resolver::query(
			m_hostname,
	  		to_string<int>(m_port, std::dec),
	  		boost::asio::ip::resolver_query_base::numeric_service)
	);
    // Start the connect actor.
    start_connect(endpoint_iter);
  }

  // This function terminates all the actors to shut down the connection. It
  // may be called by the user of the client class, or by the class itself in
  // response to graceful termination or an unrecoverable error.
  void BoostStomp::stop()
  {
	debug_print("stopping...");
    m_stopped = true;
    m_heartbeat_timer->cancel();
    //
    m_socket->close();
    delete m_socket;
  }


  //
  // --------------------------------------------------
  // ---------- TCP CONNECTION SETUP ------------------
  // --------------------------------------------------


  // --------------------------------------------------
  void BoostStomp::start_connect(tcp::resolver::iterator endpoint_iter)
  // --------------------------------------------------
  {
    if (endpoint_iter != tcp::resolver::iterator())
    {
      debug_print(boost::format("TCP: Trying %1%...") % endpoint_iter->endpoint() );

      // Try TCP connection synchronously (the first frame to send is the CONNECT frame)
      boost::system::error_code ec;
      m_socket->connect(endpoint_iter->endpoint(), ec);
      if (!ec) {
          // now we are connected to STOMP server's TCP port/
          debug_print(boost::format("TCP connection to %1% is active") % endpoint_iter->endpoint() );

    	  // Send the CONNECT request synchronously (immediately).
    	  hdrmap headers;
    	  headers["accept-version"] = "1.1";
    	  headers["host"] = m_hostname;
    	  Frame frame( "CONNECT", headers );
    	  frame.encode();
    	  debug_print("Sending CONNECT frame...");
    	  boost::asio::write(*m_socket, frame.request);

    	  // start the read actor so as to receive the CONNECTED frame
          start_stomp_read();

          // start worker thread (io_service.run())
          worker_thread = new boost::thread( boost::bind( &BoostStomp::worker, this, m_io_service ) );
      } else {
          // We need to close the socket used in the previous connection attempt
          // before starting a new one.
          m_socket->close();
          // Try the next available endpoint.
          start_connect(++endpoint_iter);
      }
    }
    else
    {
      // There are no more endpoints to try.
      stop();
      debug_print("Connection unsuccessful. Sleeping, then retrying...");
      sleep(3);
      start();
    }
  }

  // -----------------------------------------------
  // ---------- INPUT ACTOR SETUP ------------------
  // -----------------------------------------------

  // -----------------------------------------------
  void BoostStomp::start_stomp_read()
  // -----------------------------------------------
  {
	debug_print("start_stomp_read");
    // Start an asynchronous operation to read a null-delimited message.
    boost::asio::async_read_until(
    	*m_socket,
    	stomp_response,
    	'\0',
        boost::bind(&BoostStomp::handle_stomp_read, this, _1));
  }

  // -----------------------------------------------
  void BoostStomp::handle_stomp_read(const boost::system::error_code& ec)
  // -----------------------------------------------
  {
	Frame* frame;

    if (m_stopped)
      return;

    if (!ec)
    {
    	debug_print(boost::format("received server response (%1% bytes)") %  stomp_response.size()  );
		vector<Frame*> received_frames = parse_all_frames(stomp_response); // in StompFrame.cpp
		while ((!received_frames.empty()) && (frame = received_frames.back())) {
		  consume_frame(*frame);
		  // dispose frame, its not needed anymore
		  delete frame;
		  received_frames.pop_back();
		} //while
		// OK, go on to the next outgoing frame in send queue
		start_stomp_write();
    }
    else
    {
      std::cerr << "BoostStomp: Error on receive: " << ec.message() << "\n";
      stop();
    }
  }


  // ------------------------------------------------
  // ---------- OUTPUT ACTOR SETUP ------------------
  // ------------------------------------------------

  // -----------------------------------------------
  void BoostStomp::start_stomp_write()
  // -----------------------------------------------
  {
    if ((m_stopped) || (!m_connected))
      return;

    debug_print("start_stomp_write");

    // send all STOMP frames in queue
    m_sendqueue_mutex.lock();
    if (m_sendqueue.size() > 0) {
    	Frame& frame = m_sendqueue.front();
    	frame.encode();
    	debug_print(boost::format("Sending %1% frame...") %  frame.command()  );
        boost::asio::async_write(
        		*m_socket,
        		frame.request,
        		boost::bind(&BoostStomp::handle_stomp_write, this, _1)
        );
        m_sendqueue.pop();
    }
    m_sendqueue_mutex.unlock();

  }

  // -----------------------------------------------
  void BoostStomp::handle_stomp_write(const boost::system::error_code& ec)
  // -----------------------------------------------
  {
	    if (m_stopped)
	      return;

	  if (!ec) {
		  // read back the server's response
		  start_stomp_read();
	  } else {
		  // TODO: if disconnected, go back to the connection phase
		  std::cout << "Error writing to STOMP server: " << ec.message() << "\n";
		  stop();
	  }
  }

  // -----------------------------------------------
  void BoostStomp::start_stomp_heartbeat()
  // -----------------------------------------------
  {
			// Start an asynchronous operation to send a heartbeat message.
			debug_print("Sending heartbeat...");
			boost::asio::async_write(
					*m_socket,
					m_heartbeat,
					boost::bind(&BoostStomp::handle_stomp_heartbeat, this, _1)
			);
  }

  // -----------------------------------------------
  void BoostStomp::handle_stomp_heartbeat(const boost::system::error_code& ec)
  // -----------------------------------------------
  {
    if (m_stopped)
      return;

    if (!ec)
    {
      // Wait 10 seconds before sending the next heartbeat.
      m_heartbeat_timer->expires_from_now(boost::posix_time::seconds(10));
      m_heartbeat_timer->async_wait(
    		  boost::bind(
    				 &BoostStomp::start_stomp_heartbeat,
    				 this
    		  )
      );
    }
    else
    {
      std::cout << "Error on sending heartbeat: " << ec.message() << "\n";
      stop();
    }
  }

  // ------------------------------------------
  bool BoostStomp::acknowledge(Frame& frame)
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["message-id"] = frame.headers()["message-id"];
	  hm["subscription"] = frame.headers()["subscription"];
	  Frame _ackframe( "ACK", hm );
	  return(send_frame(_ackframe));
  }

  void BoostStomp::consume_frame(Frame& _rcvd_frame) {
	  debug_print(boost::format("-- consume_frame: received %1%") % _rcvd_frame.command());
	  if (_rcvd_frame.command() == "CONNECTED") {
		  m_connected = true;
		  // try to get supported protocol version from headers
		  if (_rcvd_frame.headers().find("version") != _rcvd_frame.headers().end()) {
			  m_protocol_version =  _rcvd_frame.headers()["version"];
			  debug_print(boost::format("server supports STOMP version %1%") % m_protocol_version);
		  }
		  if (m_protocol_version == "1.1") {
			  // we are connected to a version 1.1 STOMP server, we can start the heartbeat actor
			  start_stomp_heartbeat();
		  }
	  }
	  if (_rcvd_frame.command() == "MESSAGE") {
		  string* dest = new string(_rcvd_frame.headers()["destination"]);
		  //
		  if (pfnOnStompMessage_t callback_function = m_subscriptions[*dest]) {
			  debug_print("-- consume_frame: firing callback");
			  //
			  callback_function(&_rcvd_frame);
		  };
	  };
	  if (_rcvd_frame.command() == "RECEIPT") {
		  string* receipt_id = new string(_rcvd_frame.headers()["receipt_id"]);
		  // do something with receipt...
		  debug_print(boost::format("receipt-id == %1%") % receipt_id);
	  }
	  if (_rcvd_frame.command() == "ERROR") {
		  string errormessage = (_rcvd_frame.headers().find("message") != _rcvd_frame.headers().end()) ?
				  _rcvd_frame.headers()["message"] :
				  "(unknown error!)";

		  throw(errormessage);
	  }
  };

  //-----------------------------------------
  bool BoostStomp::send_frame( Frame& frame )
  //-----------------------------------------
  {
	  xpressive::smatch tmp;
	  if (!regex_match(frame.command(), tmp, re_stomp_client_command)) {
		  debug_print(boost::format("send_frame: Invalid frame command (%1%)") %  frame.command() );
		  exit(1);
	  }

	  debug_print(boost::format("send_frame: Adding %1% frame to send queue...") %  frame.command() );
	  m_sendqueue_mutex.lock();
	  m_sendqueue.push(frame);
	  m_sendqueue_mutex.unlock();
	  // start the write actor
	  start_stomp_write();
	  return(true);
  }

  // ------------------------------------------
  // ------------ PUBLIC INTERFACE ------------
  // ------------------------------------------

  // ------------------------------------------
  bool BoostStomp::send( string& topic, hdrmap _headers, std::string& body )
  // ------------------------------------------
  {
	  _headers["destination"] = topic;
	  Frame frame( "SEND", _headers, body );
	  return(send_frame(frame));
  }

  // ------------------------------------------
  bool BoostStomp::send( string& topic, hdrmap _headers, std::string& body, pfnOnStompMessage_t callback )
  // ------------------------------------------
  {
	  _headers["destination"] = topic;
	  Frame frame( "SEND", _headers, body );
	  return(send_frame(frame));
  }

  // ------------------------------------------
  bool BoostStomp::subscribe( string& topic, pfnOnStompMessage_t callback )
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["id"] = lexical_cast<string>(boost::this_thread::get_id());
	  hm["destination"] = topic;
	  Frame frame( "SUBSCRIBE", hm );
	  m_subscriptions[topic] = callback;
	  return(send_frame(frame));
  }

  // ------------------------------------------
  bool BoostStomp::unsubscribe( string& topic )
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["destination"] = topic;
	  Frame frame( "UNSUBSCRIBE", hm );
	  m_subscriptions.erase(topic);
	  return(send_frame(frame));
  }

  // ------------------------------------------
  int BoostStomp::begin()
  // ------------------------------------------
  // returns a new transaction id
  {
	  hdrmap hm;
	  hm["transaction"] = lexical_cast<string>(m_transaction_id++);
	  Frame frame( "BEGIN", hm );
	  send_frame(frame);
	  return(m_transaction_id);
  };

  // ------------------------------------------
  bool 	BoostStomp::commit(string& transaction_id)
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["transaction"] = transaction_id;
	  Frame frame( "COMMIT", hm );
	  return(send_frame(frame));
  };

  // ------------------------------------------
  bool 	BoostStomp::abort(string& transaction_id)
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["transaction"] = transaction_id;
	  Frame frame( "ABORT", hm );
	  return(send_frame(frame));
  };

} // end namespace STOMP
