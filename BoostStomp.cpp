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
#include <boost/asio/buffer.hpp>
#include "BoostStomp.hpp"

namespace STOMP {
  
  using namespace std;
  using namespace boost;
  using namespace boost::asio;
  using boost::asio::ip::tcp;
  
  typedef std::deque<Frame> stomp_frame_queue;


  // regular expressions to parse a STOMP server frame

  static sregex re_stomp_client_command = as_xpr("CONNECT")  | "DISCONNECT"
		| "SEND" 	| "SUBSCRIBE" |  "UNSUBSCRIBE"
		| "BEGIN" | "COMMIT" | "ABORT"
		| "ACK" | "NACK" ;

  static sregex re_stomp_server_command = as_xpr("CONNECTED")
		| "MESSAGE" | "RECEIPT" | "ERROR"
		| "ACK" | "NACK" ;
  //
  static mark_tag command(1), headers(2), body(3);
  static mark_tag key(1), value(2);
  //
  static sregex re_header = (key= -+alnum) >> ':' >> (value= -+_) >> _n;
  static sregex re_stomp_server_frame  = bos >> (command= re_stomp_server_command ) >> _n // command and newline
                      >> (headers= -+(re_header)) >> _n  // headers and terminating newline
                      >> (body= *_) >> eos; //body till end of stream (\0)

  // ----------------------------
  // constructor
  // ----------------------------
  BoostStomp::BoostStomp(string& hostname, int& port, AckMode ackmode /*= ACK_AUTO*/):
    m_hostname(hostname),
    m_port(port),
    m_ackmode(ackmode),
    m_stopped(false),
    m_connected(false)
  // ----------------------------
  {
	m_io_service = boost::shared_ptr< io_service > ( new io_service  );
	//m_io_service_work = boost::shared_ptr< io_service::work > ( new io_service::work( *m_io_service ) );

	m_heartbeat_timer = new  deadline_timer(*m_io_service );

	m_socket = new tcp::socket(*m_io_service);

	tcp::resolver 			resolver(*m_io_service);
	// engage!
	start(resolver.resolve(tcp::resolver::query(
			m_hostname,
			to_string<int>(m_port, std::dec),
			boost::asio::ip::resolver_query_base::numeric_service)
	));

	worker_thread = new boost::thread( boost::bind( &BoostStomp::worker, this, m_io_service ) );

  }

  BoostStomp::~BoostStomp() {
	  worker_thread->interrupt();
	  delete m_socket;
  }
  
  // ----------------------------
  vector<Frame*> BoostStomp::parse_response()
  // ----------------------------
  {
	  vector<Frame*> results;
	  istream response_stream(&stomp_response);
	  smatch frame_match;
	  string str;
	  //
	  // get all the responses in response stream
	  std::cout << "[" << boost::this_thread::get_id() << "] parse_response before: (" << stomp_response.size() << " bytes in stomp_response)" << endl;
	  //
	  // iterate over all frame matches
	  //
	  while ( std::getline( response_stream, str, '\0' ) ) {
		  std::cout << "[" << boost::this_thread::get_id() << "] parse_response in loop: (" << stomp_response.size() << " bytes in stomp_response)" << endl;
		  if ( regex_match(str, frame_match, re_stomp_server_frame ) ) {
			  Frame* next_frame = parse_frame(frame_match);
			  if (next_frame) {
				  results.push_back(next_frame);
			  }
			  cout << "[" << boost::this_thread::get_id() << "] parse_response matched frame:" << endl;
		  } else {
			  cout << "[" << boost::this_thread::get_id() << "] parse_response mismatched frame! was:" << endl;
		  }
		  hexdump(str.c_str(), str.length());
	  }
	  cout << "exiting, " << stomp_response.size() << " bytes still in stomp_response" << endl;
      return(results);
  };

  // --------------------------------------------------
  Frame* BoostStomp::parse_frame(smatch const frame_match)
  // --------------------------------------------------
  {
	  	hdrmap hm;
		size_t framesize = frame_match.length(0);
		std::cout << "[" << boost::this_thread::get_id() << "] -- parse_frame, frame size:" << framesize  << " bytes" << endl;
		hexdump(frame_match.str(0).c_str(), framesize);
		stomp_response.consume(framesize);

		std::cout << "Command:" << frame_match[command] << std::endl;
		// break down headers
		std::string h = std::string(frame_match[headers]);

		sregex_iterator cur( h.begin(), h.end(), re_header );
		sregex_iterator end;
		for( ; cur != end; ++cur ) {
			smatch const &header = *cur;
			//std::cout << "H:" << header[key] << "==" <<  header[value] << std::endl;
			hm[header[key]] = header[value];
		}
		//
		string c = string(frame_match[command]);
		string b = (frame_match[body]) ? string(frame_match[body]) : "";
		//
		return(new Frame(c, hm , b));
  };

  void BoostStomp::consume_frame(Frame& _rcvd_frame) {
	  std::cout << "[" << boost::this_thread::get_id() << "] -- recevied frame:" << _rcvd_frame.command()  << endl;
	  if (_rcvd_frame.command() == "CONNECTED") {
		  m_connected = true;
		  start_stomp_write();
	  }
	  if (_rcvd_frame.command() == "MESSAGE") {
		  string* dest = new string(_rcvd_frame.headers()["destination"]);
		  std::cout << "[" << boost::this_thread::get_id() << " notify_callbacks dest=" << dest << std::endl;
		  //
		  if (pfnOnStompMessage_t callback_function = m_subscriptions[*dest]) {
			  callback_function(&_rcvd_frame);
		  };
	  };
  };


  void hexdump(const void *ptr, int buflen) {
      unsigned char *buf = (unsigned char*)ptr;
      int i, j;
      for (i=0; i<buflen; i+=16) {
          printf("%06x: ", i);
          for (j=0; j<16; j++)
              if (i+j < buflen)
                  printf("%02x ", buf[i+j]);
              else
                  printf("   ");
          printf(" ");
          for (j=0; j<16; j++)
              if (i+j < buflen)
                  printf("%c", isprint(buf[i+j]) ? buf[i+j] : '.');
          printf("\n");
      }
  }

  void hexdump(boost::asio::streambuf& sb) {
      std::istream is(&sb);
      std::string str;
      is >> str;
  	hexdump(str.c_str(), str.size());
  }

  // ----------------------------
  // ASIO HANDLERS (protected)
  // ----------------------------


  // Called by the user of the client class to initiate the connection process.
  // The endpoint iterator will have been obtained using a tcp::resolver.
  void BoostStomp::start(tcp::resolver::iterator endpoint_iter)
  {
    // Start the connect actor.
    start_connect(endpoint_iter);
  }

  // This function terminates all the actors to shut down the connection. It
  // may be called by the user of the client class, or by the class itself in
  // response to graceful termination or an unrecoverable error.
  void BoostStomp::stop()
  {
    m_stopped = true;
    m_socket->close();
    m_heartbeat_timer->cancel();
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
      std::cout << "Trying " << endpoint_iter->endpoint() << "...\n";

      // Start the asynchronous connect operation.
      m_socket->async_connect(endpoint_iter->endpoint(),
          boost::bind(&BoostStomp::handle_connect,
            this, _1, endpoint_iter));
    }
    else
    {
      // There are no more endpoints to try. Shut down the client.
      stop();
    }
  }

  // --------------------------------------------------
  void BoostStomp::handle_connect(const boost::system::error_code& ec,
       tcp::resolver::iterator endpoint_iter)
  // --------------------------------------------------
   {
     if (m_stopped)
       return;

     // The async_connect() function automatically opens the socket at the start
     // of the asynchronous operation. If the socket is closed at this time then
     // the timeout handler must have run first.
     if (!m_socket->is_open())
     {
       std::cout << "Connect timed out\n";

       // Try the next available endpoint.
       start_connect(++endpoint_iter);
     }

     // Check if the connect operation failed before the deadline expired.
     else if (ec)
     {
       // We need to close the socket used in the previous connection attempt
       // before starting a new one.
       m_socket->close();

       // Try the next available endpoint.
       start_connect(++endpoint_iter);
     }

     // Otherwise we have successfully established a connection.
     else
     {
       std::cout << "Connected to " << endpoint_iter->endpoint() << "\n";

       // now we are connected to STOMP server's TCP port/
       // The protocol negotiation phase requires we send a CONNECT frame

       // The connection was successful. SEND the CONNECT request immediately.
       Frame frame( "CONNECT" );
       frame.encode();
       boost::asio::write(*m_socket, frame.request);

       // start the reading actor so as to receive the CONNECTED frame,
       // and the read handler will also start the writing actor, stomp_write()
       start_stomp_read();
     }
   }

  // -----------------------------------------------
  // ---------- INPUT ACTOR SETUP ------------------
  // -----------------------------------------------

  // -----------------------------------------------
  void BoostStomp::start_stomp_read()
  // -----------------------------------------------
  {
	cout << "start_stomp_read" << endl;
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
    	std::cout << "[" << boost::this_thread::get_id() << "] received server response: " << stomp_response.size() << " bytes" << endl;
		vector<Frame*> received_frames = parse_response();
		while ((!received_frames.empty()) && (frame = received_frames.back())) {
		  consume_frame(*frame);
		  // dispose frame, its not needed anymore
		  delete frame;
		  received_frames.pop_back();
		} //while

		start_stomp_read();
    }
    else
    {
      std::cout << "Error on receive: " << ec.message() << "\n";

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
    if (m_stopped)
      return;

    if (!m_connected)
    	return;

    // send all STOMP frames in queue
    m_sendqueue_mutex.lock();
    while (m_sendqueue.size() > 0) {
    	Frame& frame = m_sendqueue.front();
    	frame.encode();
    	cout << "Sending " << frame.command() << endl;
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
  void BoostStomp::start_stomp_heartbeat()
  // -----------------------------------------------
  {
	    // Start an asynchronous operation to send a heartbeat message.
	   /* boost::asio::async_write(
	    		*m_socket,
	    		boost::asio::buffer("\n", 1),
	    		boost::bind(&BoostStomp::handle_stomp_write, this, _1)
	    ); */
  }

  // -----------------------------------------------
  void BoostStomp::handle_stomp_write(const boost::system::error_code& ec)
  // -----------------------------------------------
  {
    if (m_stopped)
      return;

    if (!ec)
    {
    	cout << "sending heartbeat" << endl;
      // Wait 10 seconds before sending the next heartbeat.
      m_heartbeat_timer->expires_from_now(boost::posix_time::seconds(10));
      m_heartbeat_timer->async_wait(boost::bind(&BoostStomp::start_stomp_heartbeat, this));
    }
    else
    {
      std::cout << "Error on heartbeat: " << ec.message() << "\n";

      stop();
    }
  }

  // ------------------------------------------
  // ------------ PUBLIC INTERFACE ------------
  // ------------------------------------------

  // ------------------------------------------
  bool BoostStomp::subscribe( string& topic, pfnOnStompMessage_t callback )
  // ------------------------------------------
  {
	  hdrmap hm;
	  hm["destination"] = topic;
	  Frame frame( "SUBSCRIBE", hm );
	  m_subscriptions[topic] = callback;
	  return(send_frame(frame));
  }

  // ------------------------------------------
  bool BoostStomp::send( string& topic, hdrmap _headers, std::string& body )
  // ------------------------------------------
  {
	  _headers["destination"] = topic;
	  Frame frame( "SEND", _headers, body );
	  return(send_frame(frame));
  }


  //-----------------------------------------
  bool BoostStomp::send_frame( Frame& frame )
  //-----------------------------------------
  {
	  smatch tmp;
	  if (!regex_match(frame.command(), tmp, re_stomp_client_command)) {
		  std::cout << "invalid frame command: " << frame.command() << endl;
		  exit(1);
	  }

	  std::cout << "[" << boost::this_thread::get_id() << "] send_frame (" << frame.command() << ")" << endl;
	  m_sendqueue_mutex.lock();
	  m_sendqueue.push(frame);
	  m_sendqueue_mutex.unlock();
	  start_stomp_write();
	  return(true);
  }

  void BoostStomp::worker( boost::shared_ptr< boost::asio::io_service > io_service )
  {
          //global_stream_lock.lock();
          std::cout << "[" << boost::this_thread::get_id() << "] Thread Start" << std::endl;
          //global_stream_lock.unlock();

          io_service->run();

          //global_stream_lock.lock();
          std::cout << "[" << boost::this_thread::get_id()
                  << "] Thread Finish" << std::endl;
          //global_stream_lock.unlock();
  }
} // end namespace STOMP
