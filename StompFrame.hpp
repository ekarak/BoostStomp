#ifndef BOOST_FRAME_HPP
#define BOOST_FRAME_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/xpressive/xpressive.hpp>

#include "helpers.h"

namespace STOMP {

  using namespace std;
  using namespace boost;
  using namespace boost::asio;
  using namespace boost::xpressive;
  
	static xpressive::sregex re_stomp_client_command = as_xpr("CONNECT")  | "DISCONNECT"
			| "SEND" 	| "SUBSCRIBE" |  "UNSUBSCRIBE"
			| "BEGIN" | "COMMIT" | "ABORT"
			| "ACK" | "NACK" ;


	// regular expressions to parse a STOMP server frame

	static xpressive::sregex re_stomp_server_command = as_xpr("CONNECTED")
			| "MESSAGE" | "RECEIPT" | "ERROR"
			| "ACK" | "NACK" ;
	//
	static xpressive::mark_tag tag_command(1), tag_headers(2), tag_body(3);
	static xpressive::mark_tag tag_key(1), tag_value(2);
	//
	//static sregex re_header = (key= -+alnum) >> ':' >> (value= -+_) >> _n;
	static xpressive::sregex re_header = (tag_key= -+~as_xpr(':')) >> ':' >> (tag_value= -+_) >> _n;
	// FIXME: use content-length header to read not up to the end of the stream (which could contain another frame too!)
	// 		but up to the content-length byte.
	static xpressive::sregex re_stomp_server_frame  = bos >> (tag_command= re_stomp_server_command ) >> _n // command and newline
						>> (tag_headers= -+(re_header)) >> _n  // headers and terminating newline
						>> (tag_body= *_) >> eos; //body till end of stream (\0)

  /* STOMP Frame header map */
  typedef map<string, string> hdrmap;

  struct BoostStomp;

  class Frame {
	friend class BoostStomp;

    protected:
      string    m_command;
      hdrmap    m_headers;
      string    m_body;

      boost::asio::streambuf request;

    public:

      // constructors
      Frame(string cmd):
    	  m_command(cmd)
      {};

      Frame(string cmd, hdrmap h):
    	  m_command(cmd),
    	  m_headers(h)
      {};

      Frame(string cmd, hdrmap h, string b):
    	  m_command(cmd),
    	  m_headers(h),
    	  m_body(b)
      {};

      // copy constructor
      Frame(const Frame& other)  {
    	  //cout<<"Frame copy constructor called" <<endl;
          m_command = other.m_command;
          m_headers = other.m_headers;
          m_body = other.m_body;
      };

      // constructor from boost::xpressive regex match object
      Frame(xpressive::smatch& framestr, boost::asio::streambuf& stomp_response);

      //
      string command()  { return m_command; };
      hdrmap headers()  { return m_headers; };
      string body()	 	{ return m_body; };

      // encode a STOMP Frame into a streambuf
      void encode();

  }; // class Frame

  // return a vector of all Frame's in a streambuf
  vector<Frame*> parse_all_frames(boost::asio::streambuf& stomp_response);

  string* encode_header_token(const char* str);
  string* decode_header_token(const char* str);

} // namespace STOMP

#endif // BOOST_FRAME_HPP
