#ifndef BOOST_FRAME_HPP
#define BOOST_FRAME_HPP

#include <string>
#include <map>
#include <iostream>
#include <sstream>

namespace STOMP {
  
  using namespace std;
  
  typedef map<string, string> hdrmap;
    
  class Frame {
    private:
      string    command;
      hdrmap    headers;
      string    body;

    public:
      // constructors
      Frame(std::string cmd) : command(cmd) {};
      Frame(std::string cmd, hdrmap h) : command(cmd), headers(h) {};
      Frame(std::string cmd, hdrmap h, std::string b) : command(cmd), headers(h), body(b) {};

      // copy constructor
      Frame(const Frame& other)  {
          command = other.command;
          headers = other.headers;
          body = other.body;
      };

      // return the encoded frame as a const string
      string encoded() {
        
        stringstream os (stringstream::binary);
        
        // step 1. write the command
        if (command.length() > 0) {
          os << command << "\n";
        } else {
          throw("stomp_write: command not set!!");
        }
        
        // step 2. Write the headers (key-value pairs)
        if( headers.size() > 0 ) {
          for ( hdrmap::iterator it = headers.begin() ; it != headers.end(); it++ ) {
            os << (*it).first << ":" << (*it).second << "\n";
          }
        }
        
        // special header: content-length
        if( body.length() > 0 ) {
          os << "content-length:" << body.length() << "\n";
        }
        // write newline signifying end of headers
        os << "\n";

        // step 3. Write the body
        if( body.length() > 0 ) {
          os << body;
        }
        // write terminating NULL char
        os << "\0";
        //
        return (os.str());
      };
  
    }; // class Frame
    
} // namespace STOMP

#endif // BOOST_FRAME_HPP
