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

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include "BoostStomp.hpp"
#include "helpers.h"

namespace STOMP {

	using namespace boost;
	using namespace boost::asio;

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
  string& encode_header_token(string& str) {
	  boost::algorithm::replace_all(str, "\n", "\\n");
	  boost::algorithm::replace_all(str, ":", "\\c");
	  boost::algorithm::replace_all(str, "\\", "\\\\");
	  return(str);
  };

  string& decode_header_token(string& str) {
	  boost::algorithm::replace_all(str, "\\n", "\n");
	  boost::algorithm::replace_all(str, "\\c", ":");
	  boost::algorithm::replace_all(str, "\\\\", "\\");
	  return(str);
  };

  boost::asio::streambuf& Frame::encode(boost::asio::streambuf& _request)
  // -------------------------------------
  {
	// prepare an output stream
	ostream os(&_request);
	// step 1. write the command
	if (m_command.length() > 0) {
	  os << m_command << "\n";
	} else {
	  throw("stomp_write: command not set!!");
	}
	// step 2. Write the headers (key-value pairs)
	if( m_headers.size() > 0 ) {
	  for ( hdrmap::iterator it = m_headers.begin() ; it != m_headers.end(); it++ ) {
		  string key = (*it).first;
		  string val = (*it).second;
		os << encode_header_token(key)
			<< ":"
			<< encode_header_token(val)
			<< "\n";
	  }
	}
	// special header: content-length
	if( m_body.v.size() > 0 ) {
	  os << "content-length:" << m_body.v.size() << "\n";
	}
	// write newline signifying end of headers
	os << "\n";
	// step 3. Write the body
	if( m_body.v.size() > 0 ) {
		_request.sputn(m_body.v.data(), m_body.v.size());
		//_request.commit(m_body.v.size());
	}
	// write terminating NULL char
	_request.sputc('\0');
	//_request.commit(1);
	return(_request);
  };

  // my own version of getline for an asio streambuf
inline void mygetline (boost::asio::streambuf& sb, string& _str, char delim = '\n') {
	const char* line = boost::asio::buffer_cast<const char*>(sb.data());
	char _c;
	size_t i;
	_str.clear();
	for( i = 0;
		((i < sb.size()) && ((_c = line[i]) != delim));
		i++
	) _str += _c;
	//debug_print( boost::format("mygetline: i=%1%, sb.size()==%2%") % i % sb.size() );
	//hexdump(_str.c_str(), _str.size());
}

  // construct STOMP frame (command & header) from a streambuf
  // --------------------------------------------------
  Frame::Frame(boost::asio::streambuf& stomp_response, const stomp_server_command_map_t& cmd_map)
  // --------------------------------------------------
  {
		string _str;

		try {
			// STEP 1: find the next STOMP command line in stomp_response.
			// Chomp unknown lines till the buffer is empty, in which case an exception is raised
			//debug_print(boost::format("Frame parser phase 1, stomp_response.size()==%1%") % stomp_response.size());
			//hexdump(boost::asio::buffer_cast<const char*>(stomp_response.data()), stomp_response.size());
			while (stomp_response.size() > 0) {
				mygetline(stomp_response, _str);
				//hexdump(_str.c_str(), _str.length());
				stomp_response.consume(_str.size() + 1); // plus one for the newline
				if (cmd_map.find(_str) != cmd_map.end()) {
					//debug_print(boost::format("phase 1: COMMAND==%1%, sb.size==%2%") % _str % stomp_response.size());
					m_command = _str;
					break;
				}
			}
			// if after all this trouble m_command is not set, and there's no more data in stomp_response
			// (which shouldn't happen since we do async_read_until the double newline), then throw an exception
			if (m_command == "") throw(NoMoreFrames());

			// STEP 2: parse all headers
			//debug_print("Frame parser phase 2");
			vector< string > header_parts;
			while (stomp_response.size() > 0) {
				mygetline(stomp_response, _str);
				stomp_response.consume(_str.size()+1);
				boost::algorithm::split(header_parts, _str, is_any_of(":"));
				if (header_parts.size() > 1) {
					string& key = decode_header_token(header_parts[0]);
					string& val = decode_header_token(header_parts[1]);
					//debug_print(boost::format("phase 2: HEADER[%1%]==%2%") % key % val);
					m_headers[key] = val;
					//
				} else {
					// no valid header line detected, on to the body scanner
					break;
				}
			}
			//
		} catch(NoMoreFrames& e) {
			//debug_print("-- Frame parser ended (no more frames)");
			throw(e);
		}
  };

  // STEP 3: parse the body
  size_t Frame::parse_body(boost::asio::streambuf& _response)
  {
	  std::size_t _content_length = 0, bytecount = 0;
	  string _str;
	//debug_print("Frame parser phase 3");
	// special case: content-length
	if (m_headers.find("content-length") != m_headers.end()) {
		string& val = m_headers["content-length"];
		//debug_print(boost::format("phase 3: body content-length==%1%") % val);
		_content_length = lexical_cast<size_t>(val);
	}
	if (_content_length > 0) {
		bytecount += _content_length;
		// read back the body byte by byte
		const char* rawdata = boost::asio::buffer_cast<const char*>(_response.data());
		for (size_t i = 0; i < _content_length; i++ ) {
			m_body << rawdata[i];
		}
	} else {
		// read all bytes until the first NULL
		mygetline(_response, _str, '\0');
		bytecount += _str.size();
		m_body << _str;
	}
	bytecount += 1; // for the final frame-terminating NULL
	//debug_print(boost::format("phase 3: consumed %1% bytes, BODY(%2% bytes)==%3%") % bytecount % _str.size() % _str);
	_response.consume(bytecount);
	return(bytecount);
  }

}
