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
		// protected members setup
    m_hostname(hostname),
    m_port(port),
    m_ackmode(ackmode),
    m_stopped(true),
    m_connected(false),
    m_io_service		(new io_service()),
    m_io_service_work	(new io_service::work(*m_io_service)),
    m_strand			(new io_service::strand(*m_io_service)),
    m_socket			(new tcp::socket(*m_io_service)),
    // private members
    m_protocol_version("1.0"),
    m_transaction_id(0)
  // ----------------------------
  {
	  debug_print("Initializing BoostStomp command map");
		// map STOMP server commands to handler methods
		cmd_map["CONNECTED"] = &BoostStomp::process_CONNECTED;
		cmd_map["MESSAGE"] 	= &BoostStomp::process_MESSAGE;
		cmd_map["RECEIPT"] 	= &BoostStomp::process_RECEIPT;
		cmd_map["ERROR"] 	= &BoostStomp::process_ERROR;
  }


  // ----------------------------
  // destructor
  // ----------------------------
  BoostStomp::~BoostStomp()
  // ----------------------------
  {
	  // first stop io_service so as to exit the run loop (when idle)
	  m_io_service->stop();
	  // then interrupt the worker thread
	  worker_thread->interrupt();
	  //delete m_heartbeat_timer;
	  //delete worker_thread;
  }

  // ----------------------------
  // worker thread
  // ----------------------------
  void BoostStomp::worker( boost::shared_ptr< boost::asio::io_service > _io_service )
  {
	  debug_print("Worker thread: starting...");
	  while(!m_stopped) {
		  _io_service->run();
		  debug_print("Worker thread: io_service is stopped...");
		  _io_service->reset();
		  sleep(1);
	  }
	  debug_print("Worker thread finished.");
  }


  // ----------------------------
  // ASIO HANDLERS (protected)
  // ----------------------------


  // Called by the user of the client class to initiate the connection process.
  // The endpoint iterator will have been obtained using a tcp::resolver.
  void BoostStomp::start()
  {
	debug_print("BoostStomp starting...");
	m_stopped = false;
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
	if (m_connected && m_socket->is_open()) {
	  Frame frame( "DISCONNECT");
	  frame.encode(stomp_request);
	  debug_print("Sending DISCONNECT frame...");
	  boost::asio::write(*m_socket, stomp_request);
	}
	m_connected = false;
    m_stopped = true;
    if (m_heartbeat_timer != NULL) {
    	m_heartbeat_timer->cancel();
    }
    //
    m_socket->close();
    //
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
      //debug_print(boost::format("TCP: Trying %1%...") % endpoint_iter->endpoint() );

      // Try TCP connection synchronously (the first frame to send is the CONNECT frame)
      boost::system::error_code ec;
      m_socket->connect(endpoint_iter->endpoint(), ec);
      if (!ec) {
          // now we are connected to STOMP server's TCP port/
          debug_print(boost::format("STOMP TCP connection to %1% is active") % endpoint_iter->endpoint() );

    	  // Send the CONNECT request synchronously (immediately).
    	  hdrmap headers;
    	  headers["accept-version"] = "1.1";
    	  headers["host"] = m_hostname;
    	  Frame frame( "CONNECT", headers );
    	  frame.encode(stomp_request);
    	  //debug_print("Sending CONNECT frame...");
    	  boost::asio::write(*m_socket, stomp_request);
    	 // FIXME: what about stomp_request.commit() ???
    	  // start the read actor so as to receive the CONNECTED frame
          start_stomp_read_headers();

          // start worker thread (m_io_service.run())
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
  void BoostStomp::start_stomp_read_headers()
  // -----------------------------------------------
  {
	debug_print("start_stomp_read");
    // Start an asynchronous operation to read at least the STOMP frame command & headers (till the double newline delimiter)
     boost::asio::async_read_until(
    	*m_socket,
    	stomp_response,
    	"\n\n",
        boost::bind(&BoostStomp::handle_stomp_read_headers, this, placeholders::error()));
  }

  // -----------------------------------------------
  void BoostStomp::handle_stomp_read_headers(const boost::system::error_code& ec)
  // -----------------------------------------------
  {
    if (m_stopped)
      return;

    if (!ec)
    {
    	std::size_t bodysize = 0;
    	debug_print(boost::format("received response (command+headers: %1% bytes)") %  stomp_response.size()  );
    	start_stomp_read_body(bodysize);

		try {
			m_rcvd_frame = new Frame(stomp_response, cmd_map);
		} catch(NoMoreFrames&) {
//			break;
		} catch(std::exception& e) {
			debug_print(boost::format("handle_stomp_read in loop: unknown exception in Frame constructor:\n%1%") % e.what());
			exit(10);
		}

    }
    else
    {
      std::cerr << "BoostStomp: Error on receive: " << ec.message() << "\n";
      stop();
      start();
    }
  }

  // -----------------------------------------------
  void BoostStomp::start_stomp_read_body(std::size_t bodysize)
  // -----------------------------------------------
  {
	debug_print("start_stomp_read");
    // Start an asynchronous operation to read at least the STOMP frame body
	if (bodysize == 0) {
		boost::asio::async_read_until(
				*m_socket, 	stomp_response,
				'\0', 	// NULL signifies the end of the body
				boost::bind(&BoostStomp::handle_stomp_read_body, this, placeholders::error(), placeholders::bytes_transferred()));
	} else {
		boost::asio::async_read(
			*m_socket, 	stomp_response,
			boost::asio::transfer_at_least(bodysize),
			boost::bind(&BoostStomp::handle_stomp_read_body, this, placeholders::error(), placeholders::bytes_transferred()));
	}
  }

  // -----------------------------------------------
  void BoostStomp::handle_stomp_read_body(const boost::system::error_code& ec, std::size_t bytes_transferred = 0)
  // -----------------------------------------------
  {
    if (m_stopped)
      return;

    if (!ec)
    {
    	debug_print(boost::format("received response (%1% bytes) (buffer: %2% bytes)") % bytes_transferred %  stomp_response.size()  );
    	if (m_rcvd_frame != NULL) {
    		m_rcvd_frame->parse_body(stomp_response);
    		consume_received_frame();
    	}
    	//
    	debug_print("stomp_response contents after Frame scanning:");
    	hexdump(stomp_response);
		// wait for the next incoming frame from the server...
		start_stomp_read_headers();
    }
    else
    {
      std::cerr << "BoostStomp: Error on receive: " << ec.message() << "\n";
      stop();
      start();
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
    Frame* frame;
    // send all STOMP frames in queue
    m_sendqueue_mutex.lock();
    if (m_sendqueue.size() > 0) {
    	frame = m_sendqueue.front();
    	if (frame != NULL) {
    		debug_print(boost::format("Sending %1% frame...") %  frame->command() );
    		frame->encode(stomp_request);

    		boost::asio::async_write(
        		*m_socket,
        		stomp_request,
        		boost::bind(&BoostStomp::handle_stomp_write, this, _1)
    		);
    	};
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
		  debug_print("Sent!");
		  // call pop() to delete the last frame in queue
		  m_sendqueue_mutex.lock();
		  m_sendqueue.pop();
		  m_sendqueue_mutex.unlock();
		  // process next frame in queue, if any
		  start_stomp_write();
	  } else {
		  m_connected = false;
		  debug_print(boost::format("Error writing to STOMP server: %1%") % ec.message());
		  stop();
	  }
  }

  // -----------------------------------------------
  void BoostStomp::start_stomp_heartbeat()
  // -----------------------------------------------
  {
			// Start an asynchronous operation to send a heartbeat message.
			//debug_print("Sending heartbeat...");
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
  bool BoostStomp::acknowledge(Frame* frame, bool acked = true)
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["message-id"] = frame->headers()["message-id"];
	  hm["subscription"] = frame->headers()["subscription"];
	  string _ack_cmd = (acked ? "ACK" : "NACK");
	  Frame* _ackframe = new Frame( _ack_cmd, hm );
	  return(send_frame(_ackframe));
  }

  // ------------------------------------------
  void BoostStomp::consume_received_frame()
  // ------------------------------------------
  {
	  if (m_rcvd_frame != NULL) {
		  pfnStompCommandHandler_t handler = cmd_map[m_rcvd_frame->command()];
		  if (handler != NULL) {
			  debug_print(boost::format("-- consume_frame: calling %1% command handler") % m_rcvd_frame->command());
			  // call STOMP command handler
			  (this->*handler)();
		  }
		  delete m_rcvd_frame;
	  }
	  m_rcvd_frame = NULL;
  };

  //-----------------------------------------
  void BoostStomp::process_CONNECTED()
  //-----------------------------------------
  {
	  m_connected = true;
	  // try to get supported protocol version from headers
	  hdrmap headers = m_rcvd_frame->headers();
	  if (headers.find("version") != headers.end()) {
		  m_protocol_version =  headers["version"];
		  debug_print(boost::format("server supports STOMP version %1%") % m_protocol_version);
	  }
	  if (m_protocol_version == "1.1") {
		  // we are connected to a version 1.1 STOMP server, setup heartbeat
			m_heartbeat_timer = boost::shared_ptr< deadline_timer> ( new deadline_timer( *m_io_service ));
			std::ostream os( &m_heartbeat);
			os << "\n";
		  // we can start the heartbeat actor
		  start_stomp_heartbeat();
	  }
	  // in case of reconnection, we need to re-subscribe to all subscriptions
	  for (subscription_map::iterator it = m_subscriptions.begin(); it != m_subscriptions.end(); it++) {
		  //string topic = (*it).first;
		  do_subscribe((*it).first);
	  };
  }

  //-----------------------------------------
  void BoostStomp::process_MESSAGE()
  //-----------------------------------------
  {
	  bool acked = true;
	  string dest = string(m_rcvd_frame->headers()["destination"]);
	  //
	  if (pfnOnStompMessage_t callback_function = m_subscriptions[dest]) {
		  debug_print(boost::format("-- consume_frame: firing callback for %1%") % dest);
		  //
		  acked = callback_function(m_rcvd_frame);
	  };
	  // acknowledge frame, if in "Client" or "Client-Individual" ack mode
	  if ((m_ackmode == ACK_CLIENT) || (m_ackmode == ACK_CLIENT_INDIVIDUAL)) {
		  acknowledge(m_rcvd_frame, acked);
	  }
  }

  //-----------------------------------------
  void BoostStomp::process_RECEIPT()
  //-----------------------------------------
  {
  		  // do something with receipt...
  		  debug_print(boost::format("receipt-id == %1%") % m_rcvd_frame->headers()["receipt_id"]);
  }

  //-----------------------------------------
  void BoostStomp::process_ERROR()
  //-----------------------------------------
  {
	  hdrmap headers = m_rcvd_frame->headers();
  		  string errormessage = (headers.find("message") != headers.end()) ?
  				  headers["message"] :
  				  "(unknown error!)";
  		  errormessage += m_rcvd_frame->body().c_str();
  		  throw(errormessage);
  }


  //-----------------------------------------
  bool BoostStomp::send_frame( Frame* frame )
  //-----------------------------------------
  {
	  // send_frame is called from the application thread. Do not dereference frame here!!! (shared data)
	  //debug_print(boost::format("send_frame: Adding frame to send queue...") %  frame->command() );
	  debug_print("send_frame: Adding frame to send queue...");
	  m_sendqueue_mutex.lock();
	  m_sendqueue.push(frame);
	  m_sendqueue_mutex.unlock();
	  // tell io_service to send the frame from the worker thread
	  usleep(1000);
	  m_strand->post(
			boost::bind(&BoostStomp::start_stomp_write, this)
	  );
	  return(true);
  }

  // ---------------------------------------------------------------------------------------
  // ---------------------------------------------------------------------------------------
  // ------------------------ 			PUBLIC INTERFACE 			------------------------
  // ---------------------------------------------------------------------------------------
  // ---------------------------------------------------------------------------------------


  // ------------------------------------------
  bool BoostStomp::subscribe( string& topic, pfnOnStompMessage_t callback )
  // ------------------------------------------
  {
	  m_subscriptions[topic] = callback;
	  return(do_subscribe(topic));
  }

  // ------------------------------------------
  bool BoostStomp::do_subscribe(const string& topic)
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["id"] = lexical_cast<string>(boost::this_thread::get_id());
	  hm["destination"] = topic;
	  Frame* frame = new Frame( "SUBSCRIBE", hm );
	  return(send_frame(frame));
  }


  // ------------------------------------------
  bool BoostStomp::unsubscribe( string& topic )
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["destination"] = topic;
	  Frame* frame = new Frame( "UNSUBSCRIBE", hm );
	  m_subscriptions.erase(topic);
	  return(send_frame(frame));
  }

  // ------------------------------------------
  int BoostStomp::begin()
  // ------------------------------------------
  // returns a new transaction id
  {
	  hdrmap hm;
	  // create a new transaction id
	  hm["transaction"] = lexical_cast<string>(m_transaction_id++);
	  Frame* frame = new Frame( "BEGIN", hm );
	  send_frame(frame);
	  return(m_transaction_id);
  };

  // ------------------------------------------
  bool 	BoostStomp::commit(int transaction_id)
  // ------------------------------------------
  {
	  hdrmap hm;
	  // add required header
	  hm["transaction"] = lexical_cast<string>(transaction_id);
	  Frame* frame = new Frame( "COMMIT", hm );
	  return(send_frame(frame));
  };

  // ------------------------------------------
  bool 	BoostStomp::abort(int transaction_id)
  // ------------------------------------------
  {
	  hdrmap hm;
	  // add required header
	  hm["transaction"] = lexical_cast<string>(transaction_id);
	  Frame* frame = new Frame( "ABORT", hm );
	  return(send_frame(frame));
  };

} // end namespace STOMP

