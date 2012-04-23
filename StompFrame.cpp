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

#include "StompFrame.hpp"

#include <boost/format.hpp>

namespace STOMP {

	using namespace boost;
	using namespace boost::xpressive;

  /*
   * Escaping is needed to allow header keys and values to contain those frame header
   * delimiting octets as values. The CONNECT and CONNECTED frames do not escape the
   * colon or newline octets in order to remain backward compatible with STOMP 1.0.
   * C style string literal escapes are used to encode any colons and newlines that
   * are found within the UTF-8 encoded headers. When decoding frame headers, the
   * following transformations MUST be applied:
   *
   * \n (octet 92 and 110) translates to newline (octet 10)
   * \c (octet 92 and 99) translates to : (octet 58)
   * \\ (octet 92 and 92) translates to \ (octet 92)
   */
  string* encode_header_token(const char* str) {
	  string* result = new string(str);
	  boost::algorithm::replace_all(*result, "\n", "\\n");
	  boost::algorithm::replace_all(*result, ":", "\\c");
	  boost::algorithm::replace_all(*result, "\\", "\\\\");
	  return(result);
  };

  string* decode_header_token(const char* str) {
	  string* result = new string(str);
	  boost::algorithm::replace_all(*result, "\\n", "\n");
	  boost::algorithm::replace_all(*result, "\\c", ":");
	  boost::algorithm::replace_all(*result, "\\\\", "\\");
	  return(result);
  };

  void Frame::encode()
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
		os << *encode_header_token((*it).first.c_str())
			<< ":"
			<< *encode_header_token((*it).second.c_str())
			<< "\n";
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

  // --------------------------------------------------
  Frame::Frame(xpressive::smatch& frame_match, boost::asio::streambuf& stomp_response)
  // --------------------------------------------------
  {
		size_t framesize = frame_match.length(0);
		size_t bytes_to_consume = framesize + 2; // plus one for the NULL
		debug_print(boost::format("-- parse_frame, frame: %1% bytes, content: \n%2%") % framesize % frame_match[0]);
		hexdump(frame_match.str(0).c_str(), framesize);
		stomp_response.consume(bytes_to_consume);
		debug_print(boost::format("-- parse_frame, consumed %1% bytes from stomp_response") % bytes_to_consume);
		//std::cout << "Command:" << frame_match[command] << std::endl;
		// break down headers
		std::string h = std::string(frame_match[tag_headers]);

		xpressive::sregex_iterator cur( h.begin(), h.end(), re_header );
		xpressive::sregex_iterator end;
		for( ; cur != end; ++cur ) {
			xpressive::smatch const &header = *cur;
			m_headers[*decode_header_token(header[tag_key].str().c_str())] = *decode_header_token(header[tag_value].str().c_str());
		}
		//
		m_command = string(frame_match[tag_command]);
		m_body = (frame_match[tag_body]) ? string(frame_match[tag_body]) : "";
  };

  // ----------------------------
  vector<Frame*> parse_all_frames(boost::asio::streambuf& stomp_response)
  // ----------------------------
  {
	  vector<Frame*> results;
	  istream response_stream(&stomp_response);
	  xpressive::smatch frame_match;
	  string str;
	  char lala[1024];
	  //
	  // get all the responses in response stream
	  debug_print(boost::format("parse_all_frames before: (%1% bytes in stomp_response)") % stomp_response.size() );
	  //
	  // iterate over all frame matches
	  //
	  //while ( std::getline( response_stream, str, '\0' ) ) {
	  while ( response_stream.get(lala, 1023, '\0') ) {
		  str = string(lala);
hexdump(str.c_str(), str.size());
		  debug_print(boost::format("parse_all_frames in loop: (%1% bytes in str, %2% bytes still in stomp_response)") % str.size() % stomp_response.size());

		  if ( regex_match(str, frame_match, re_stomp_server_frame ) ) {
			  try {
				  Frame* next_frame = new Frame(frame_match, stomp_response);
				  results.push_back(next_frame);
			  } catch(...) {
				  debug_print("parse_response in loop: exception in Frame constructor");
// TODO
			  }
		  } else {
			  debug_print("parse_response error: mismatched frame:");
			  hexdump(str.c_str(), str.length());
		  }

	  }
	  //cout << "exiting, " << stomp_response.size() << " bytes still in stomp_response" << endl;
      return(results);
  };

}
