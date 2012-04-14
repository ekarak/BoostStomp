#ifndef BOOST_FRAME_HPP
#define BOOST_FRAME_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>

namespace STOMP {

  using namespace std;
  using namespace boost::asio;
  
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

      void encode()
      // -------------------------------------
      {
    	// prepare an output stream
    	ostream os(&request);
        // step 1. write the command
        if (m_command.length() > 0) {
          os << m_command << "\n";
        } else {
          throw("stomp_write: command not set!!");
        }
        // step 2. Write the headers (key-value pairs)
        if( m_headers.size() > 0 ) {
          for ( hdrmap::iterator it = m_headers.begin() ; it != m_headers.end(); it++ ) {
            os << (*it).first << ":" << (*it).second << "\n";
          }
        }
        // special header: content-length
        if( m_body.length() > 0 ) {
          os << "content-length:" << m_body.length() << "\n";
        }
        // write newline signifying end of headers
        os << "\n";
        // step 3. Write the body
        if( m_body.length() > 0 ) {
          os << m_body;
        }
        // write terminating NULL char
        request.sputc('\0');
      };
    }; // class Frame
    
} // namespace STOMP

#endif // BOOST_FRAME_HPP
