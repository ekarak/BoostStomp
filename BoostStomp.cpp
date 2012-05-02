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
    m_socket			(new tcp::socket(*m_io_service)),
    // private members
    m_protocol_version("1.0"),
    m_transaction_id(0)
  // ----------------------------
  {
	// Heartbeat setup
	m_heartbeat_timer = boost::shared_ptr< deadline_timer> ( new deadline_timer( *m_io_service ));
	std::ostream os( &m_heartbeat);
	os << "\n";
	// Server command map
	stomp_server_command_map["CONNECTED"] = &BoostStomp::process_CONNECTED;
	stomp_server_command_map["MESSAGE"]	=	&BoostStomp::process_MESSAGE;
	stomp_server_command_map["RECEIPT"] = 	&BoostStomp::process_RECEIPT;
	stomp_server_command_map["ERROR"] = 	&BoostStomp::process_ERROR;
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
  void BoostStomp::worker( boost::shared_ptr< boost::asio::io_service > io_service )
  {
	  debug_print("Worker thread starting...");
	  while(!m_stopped) {
		  io_service->run();
		  io_service->reset();
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
	  debug_print("Sending DISCONNECT frame...");
	  boost::asio::write(*m_socket, frame.encode());
	}
	m_connected = false;
    m_stopped = true;
    m_heartbeat_timer->cancel();
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
    	  //debug_print("Sending CONNECT frame...");
    	  boost::asio::write(*m_socket, frame.encode());

    	  // start the read actor so as to receive the CONNECTED frame
          start_stomp_read();

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
		vector<Frame*> received_frames = parse_all_frames();
		while ((!received_frames.empty()) && (frame = received_frames.back())) {
		  consume_frame(*frame);
		  // dispose frame, its not needed anymore
		  delete frame;
		  received_frames.pop_back();
		} //while
		// wait for any incoming frames from the server...
		start_stomp_read();
    }
    else
    {
      std::cerr << "BoostStomp: Error on receive: " << ec.message() << "\n";
      stop();
      start();
    }
  }

  // --------------------------------------------------
  Frame* BoostStomp::parse_next()
  // --------------------------------------------------
  {
		string _str;
		istream _input(&stomp_response);
		size_t bytes_to_consume = 0, content_length = 0;
		Frame* frame = NULL;

		try {

			// STEP 1: find the next STOMP command line in stomp_response, skipping non-matching lines
			//debug_print("parse_next phase 1");
			while (std::getline(_input, _str)) {
				//hexdump(_str.c_str(), _str.length());
				if ((_str.size() > 0) && (stomp_server_command_map.find(_str) != stomp_server_command_map.end())) {
					//debug_print(boost::format("parse_next phase 1: COMMAND==%1%") % _str);
					frame = new Frame(_str);
					bytes_to_consume += _str.size()+1;
					break;
				}
			}
			// STEP 2: parse all headers
			if (frame != NULL) {
				//debug_print("parse_next phase 2");
				vector< string > header_parts;
				while (std::getline(_input, _str)) {
					//hexdump(_str.c_str(), _str.length());
					boost::algorithm::split(header_parts, _str, is_any_of(":"));
					if (header_parts.size() > 1) {
						string& key = decode_header_token(header_parts[0]);
						string& val = decode_header_token(header_parts[1]);
						//debug_print(boost::format("parse_next phase 2: HEADER[%1%]==%2%") % key % val);
						frame->m_headers[key] = val;
						bytes_to_consume += _str.size()+1;
						// special case: content-length
						if (key == "content-length") {
							content_length = lexical_cast<int>(val);
							//debug_print(boost::format("content-length read back value==%1%") % content_length);
						}
					} else {
						break;
					}
				}
				bytes_to_consume += 1;
				// STEP 3: parse the body
				//debug_print("parse_next phase 3");
				if (content_length > 0) {
					// read back the body byte by byte
					char c;
					for (size_t i=0; i<content_length; i++) {
						_input.get(c);
						frame->m_body << c;
					}
				} else {
					// read all bytes until the first NULL
					std::getline(_input, _str, '\0');
					//debug_print(boost::format("parse_next phase 3: BODY(%1% bytes)==%2%") % _str.size() % _str);
					if (_str.length() > 0) {
						bytes_to_consume += _str.size() + 1;
						frame->m_body << _str;
					};
				}
			} else {
				throw("shit happens");
			}
		}
		catch (...) {

		}
		stomp_response.consume(bytes_to_consume);
		debug_print(boost::format("-- parse_frame, consumed %1% bytes from stomp_response") % bytes_to_consume);
		return(frame);
  };

  // ----------------------------
  vector<Frame*> BoostStomp::parse_all_frames()
  // ----------------------------
  {
	  vector<Frame*> results;
	  //
	  // get all the responses in response stream
	  debug_print(boost::format("parse_all_frames before: (%1% bytes in stomp_response)") % stomp_response.size() );
	  try {
		  //
		  // iterate over all frame matches
		  //
		  while (Frame* next_frame = parse_next()) {
			  debug_print(boost::format("parse_all_frames in loop: (%1% bytes still in stomp_response)") % stomp_response.size());
			  results.push_back(next_frame);
		  }
	  } catch(...) {
		  debug_print("parse_response in loop: exception in Frame constructor");
	  }
	  //cout << "exiting, " << stomp_response.size() << " bytes still in stomp_response" << endl;
      return(results);
  };


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
    	debug_print(boost::format("Sending %1% frame...") %  frame.command()  );
        boost::asio::async_write(
        		*m_socket,
        		frame.encode(),
        		boost::bind(&BoostStomp::handle_stomp_write, this, _1)
        );
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
		  // call pop() to delete the last frame in queue
		  m_sendqueue_mutex.lock();
		  m_sendqueue.pop();
		  m_sendqueue_mutex.unlock();
		  // process next frame in queue, if any
		  start_stomp_write();
	  } else {
		  m_connected = false;
		  std::cout << "Error writing to STOMP server: " << ec.message() << "\n";
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
  bool BoostStomp::acknowledge(Frame& frame, bool acked = true)
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["message-id"] = frame.headers()["message-id"];
	  hm["subscription"] = frame.headers()["subscription"];
	  string _ack_cmd = (acked ? "ACK" : "NACK");
	  Frame _ackframe( _ack_cmd, hm );
	  return(send_frame(_ackframe));
  }

  // ------------------------------------------
  void BoostStomp::consume_frame(Frame& _rcvd_frame)
  // ------------------------------------------
  {
	  debug_print(boost::format("-- consume_frame: calling %1% command handler") % _rcvd_frame.command());
	  pfnStompCommandHandler_t handler = stomp_server_command_map[_rcvd_frame.command()];
	  (this->*handler)(_rcvd_frame);


	  /*
	  if (_rcvd_frame.command() == "CONNECTED") process_CONNECTED(_rcvd_frame);
	  if (_rcvd_frame.command() == "MESSAGE") 	process_MESSAGE(_rcvd_frame);
	  if (_rcvd_frame.command() == "RECEIPT") 	process_RECEIPT(_rcvd_frame);
	  if (_rcvd_frame.command() == "ERROR")		process_ERROR(_rcvd_frame);
	  */
  };

  //-----------------------------------------
  void BoostStomp::process_CONNECTED(Frame& _rcvd_frame)
  //-----------------------------------------
  {
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
	  // in case of reconnection, we need to re-subscribe to all subscriptions
	  for (subscription_map::iterator it = m_subscriptions.begin(); it != m_subscriptions.end(); it++) {
		  string topic = (*it).first;
		  do_subscribe(topic);
	  };
  }

  //-----------------------------------------
  void BoostStomp::process_MESSAGE(Frame& _rcvd_frame)
  //-----------------------------------------
  {
	  bool acked = true;
	  string dest = string(_rcvd_frame.headers()["destination"]);
	  //
	  if (pfnOnStompMessage_t callback_function = m_subscriptions[dest]) {
		  debug_print(boost::format("-- consume_frame: firing callback for %1%") % dest);
		  //
		  acked = callback_function(_rcvd_frame);
	  };
	  // acknowledge frame, if in "Client" or "Client-Individual" ack mode
	  if ((m_ackmode == ACK_CLIENT) || (m_ackmode == ACK_CLIENT_INDIVIDUAL)) {
		  acknowledge(_rcvd_frame, acked);
	  }
  }

  //-----------------------------------------
  void BoostStomp::process_RECEIPT(Frame& _rcvd_frame)
  //-----------------------------------------
  {
  		  string receipt_id = string(_rcvd_frame.headers()["receipt_id"]);
  		  // do something with receipt...
  		  debug_print(boost::format("receipt-id == %1%") % receipt_id);
  }

  //-----------------------------------------
  void BoostStomp::process_ERROR(Frame& _rcvd_frame)
  //-----------------------------------------
  {
  		  string errormessage = (_rcvd_frame.headers().find("message") != _rcvd_frame.headers().end()) ?
  				  _rcvd_frame.headers()["message"] :
  				  "(unknown error!)";
  		  errormessage += _rcvd_frame.body().c_str();
  		  throw(errormessage);
  }


  //-----------------------------------------
  bool BoostStomp::send_frame( Frame& frame )
  //-----------------------------------------
  {
	  debug_print(boost::format("send_frame: Adding %1% frame to send queue...") %  frame.command() );
	  m_sendqueue_mutex.lock();
	  m_sendqueue.push(frame);
	  m_sendqueue_mutex.unlock();
	  start_stomp_write();
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
  bool BoostStomp::do_subscribe (string& topic)
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["id"] = lexical_cast<string>(boost::this_thread::get_id());
	  hm["destination"] = topic;
	  Frame frame( "SUBSCRIBE", hm );
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
	  // create a new transaction id
	  hm["transaction"] = lexical_cast<string>(m_transaction_id++);
	  Frame frame( "BEGIN", hm );
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
	  Frame frame( "COMMIT", hm );
	  return(send_frame(frame));
  };

  // ------------------------------------------
  bool 	BoostStomp::abort(int transaction_id)
  // ------------------------------------------
  {
	  hdrmap hm;
	  // add required header
	  hm["transaction"] = lexical_cast<string>(transaction_id);
	  Frame frame( "ABORT", hm );
	  return(send_frame(frame));
  };



} // end namespace STOMP

