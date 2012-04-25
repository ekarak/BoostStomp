#ifndef BOOST_FRAME_HPP
#define BOOST_FRAME_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/assign/list_inserter.hpp>

namespace STOMP {

  using namespace std;
  using namespace boost;
  using namespace boost::asio;
  using namespace boost::assign;
  using namespace boost::algorithm;

  /* STOMP Frame header map */
  typedef map<string, string> hdrmap;

  class BoostStomp;

  // an std::vector encapsulation in order to store binary strings
  // (STOMP doesn't prohibit NULLs inside the frame body)
  class binbody {

  public:
	  // one vector to hold them all
	  vector<char> v;
	  // constructors:
	  binbody() {};
	  binbody(string b) {
		  v.assign(b.begin(), b.end());
	  }
	  binbody(string::iterator begin, string::iterator end) {
		  v.assign(begin, end);
	  };
	  // append a string at the end of the body vector
	  binbody& operator << (std::string s) {
		  v.insert(v.end(), s.begin(), s.end());
		  return(*this);
	  };

	  // append a char at the end of the body vector
	  binbody& operator << (const char& c) {
		  v.push_back(c);
		  return(*this);
	  };

	  // return the body vector content as a c-string
	  char* c_str() {
		  return(v.data());
	  };
  };

  class Frame {
	friend class BoostStomp;
	friend Frame* parse_next(boost::asio::streambuf&);

    protected:
      string    m_command;
      hdrmap    m_headers;
      binbody 	m_body;

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

      template <typename BodyType>
      Frame(string cmd, hdrmap h, BodyType b):
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

      //
      string command()  { return m_command; };
      hdrmap headers()  { return m_headers; };
      binbody& body()	 { return m_body; };

      // encode a STOMP Frame into a streambuf
      void encode();

  }; // class Frame

  string* encode_header_token(const char* str);
  string* decode_header_token(const char* str);

} // namespace STOMP

#endif // BOOST_FRAME_HPP
