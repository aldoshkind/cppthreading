#pragma once

#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>

namespace adn::threading
{

template <class type>
class concurrent_queue
{
public:
    typedef type value_type;
    
    concurrent_queue(std::optional<size_t> max_queue_length = {});
	~concurrent_queue();

	bool push(const type &value);
	bool push_front(const type &value);
	bool pop(type &value);
	bool pop_timed(type &value, float time_to_wait);
    
    void stop();
    bool running() const {return !exited;}
    
    size_t size() const
    {
        std::unique_lock<decltype (mutex)> lock(mutex);
        return elements.size();
    }
    
private:
    mutable std::recursive_mutex mutex;
	std::condition_variable_any condvar;

	std::list<type> elements;
    
    std::atomic<bool> exited = false;
    
    std::optional<size_t> max_length;
};

template <class type>
concurrent_queue<type>::concurrent_queue(std::optional<size_t> max_queue_length) : max_length(max_queue_length)
{
	//
}

template <class type>
concurrent_queue<type>::~concurrent_queue()
{
    stop();
}

template <class type>
void concurrent_queue<type>::stop()
{
    exited = true;
    std::unique_lock<decltype (mutex)> lock(mutex);
    elements.clear();
    condvar.notify_all();
}

template <class type>
bool concurrent_queue<type>::push(const type &value)
{
    std::unique_lock<decltype (mutex)> lock(mutex);
    if(exited)
    {
        return false;
    }
    if(max_length.has_value() and elements.size() >= max_length)
    {
        elements.pop_front();
    }
	elements.push_back(value);
	condvar.notify_one();
    
    //printf("queue size is %ld\n", elements.size());

	return true;
}

template <class type>
bool concurrent_queue<type>::push_front(const type &value)
{
	std::unique_lock<decltype (mutex)> lock(mutex);
    if(exited)
    {
        return false;
    }
    if(max_length.has_value() and elements.size() >= max_length)
    {
        elements.pop_front();
    }
	elements.push_front(value);
	condvar.notify_one();

	return true;
}

template <class type>
bool concurrent_queue<type>::pop(type &value)
{
	std::unique_lock<decltype (mutex)> lock(mutex);
    auto need_stop = [this](){return exited or !elements.empty();};
    for( ; !need_stop() ; )
    {
        condvar.wait_for(lock, std::chrono::milliseconds(100), need_stop);
    }
    if(exited)
    {
        return false;
    }
    
	value = elements.front();
	elements.pop_front();

	return true;
}

template <class type>
bool concurrent_queue<type>::pop_timed(type &value, float time_to_wait)
{
	std::unique_lock<decltype (mutex)> lock(mutex);
    auto start_time = std::chrono::steady_clock::now();
    for( ; !exited ; )
    {
        condvar.wait_for(lock, std::chrono::milliseconds(100));
	    if(!elements.empty())
	    {
	    	break;
	    }
        if(exited)
	    {
    	    return false;
	    }
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<double>(now - start_time).count();
        if(dt > time_to_wait)
        {
//            printf("%s timed out\n", __PRETTY_FUNCTION__);
        	return false;
        }
    }
    
	value = elements.front();
	elements.pop_front();

	return true;
}

}
