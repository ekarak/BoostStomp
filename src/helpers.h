/*
 * helpers.h
 *
 *  Created on: 22 Απρ 2012
 *      Author: ekarak
 */

#ifndef HELPERS_H_
#define HELPERS_H_

#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <string>
#include <queue>

using namespace std;

// helper function
void hexdump(boost::asio::streambuf&);
void hexdump(const void *ptr, int buflen);

std::string FormatTime(boost::posix_time::ptime&);

// helper template function for pretty-printing just about anything
template <class T>
std::string to_string(T t, std::ios_base & (*f)(std::ios_base&))
{
  std::ostringstream oss;
  oss.setf (std::ios_base::showbase);
  oss << f << t;
  return oss.str();
}

// -------------------------------
// Concurrent queue, courtesy of:
// http://www.justsoftwaresolutions.co.uk/threading/implementing-a-thread-safe-queue-using-condition-variables.html
// -------------------------------
template<typename Data>
class concurrent_queue
{
private:
    std::queue<Data> the_queue;
    mutable boost::mutex the_mutex;
    boost::condition_variable the_condition_variable;
public:
    void push(Data const& data)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        the_queue.push(data);
        lock.unlock();
        the_condition_variable.notify_one();
    }

    bool empty() const
    {
        boost::mutex::scoped_lock lock(the_mutex);
        return the_queue.empty();
    }

    bool try_pop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        if(the_queue.empty())
        {
            return false;
        }

        popped_value=the_queue.front();
        the_queue.pop();
        return true;
    }

    void wait_and_pop(Data& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        while(the_queue.empty())
        {
            the_condition_variable.wait(lock);
        }

        popped_value=the_queue.front();
        the_queue.pop();
    }

};


#endif /* HELPERS_H_ */
