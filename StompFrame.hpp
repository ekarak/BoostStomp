#ifndef BOOST_FRAME_HPP
#define BOOST_FRAME_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/algorithm/string/replace.hpp>

namespace STOMP {

  using namespace std;
  using namespace boost::asio;
  
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

      string command()  { return m_command; };
      hdrmap headers()  { return m_headers; };
      string body()	 	{ return m_body; };

      void encode();

  }; // class Frame
    
  string* encode_header_token(const char* str);
  string* decode_header_token(const char* str);

} // namespace STOMP

#endif // BOOST_FRAME_HPP
