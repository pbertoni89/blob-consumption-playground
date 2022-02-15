#include <future>
#include <mutex>
#include <vector>
#include <iostream>
#include <cstdio>

#include "utils.h"


// a semaphore class
//
// All threads can wait on this object. When a waiting thread
// is woken up, it does its work and then notifies another waiting thread.
// In this way only n threads will be doing work at any time.
//
class Sem
{
private:
	std::mutex m;
	std::condition_variable cv;
	unsigned int count;

public:
	explicit Sem(const int n) : count(n) {}

	void notify()
	{
		std::unique_lock<std::mutex> l(m);
		++count;
		cv.notify_one();
	}

	void wait()
	{
		std::unique_lock<std::mutex> l(m);
		cv.wait(l, [this]{ return count!=0; });
		--count;
	}
};


// an RAII class to handle waiting and notifying the next thread
// Work is done between when the object is created and destroyed
class SemWaiterNotifier
{
	Sem & s;
public:
	explicit SemWaiterNotifier(Sem & s) : s{s} { s.wait(); }
	~ SemWaiterNotifier() { s.notify(); }
};


// for_each algorithm for iterating over a container but also
// making an integer index available.
//
// f is called like f(index, element)
template<typename Container, typename F>
F for_each(Container & c, F f)
{
	typename Container::size_type i = 0;
	for (auto & e : c)
		f(i++, e);
	return f;
}


// global semaphore so that lambdas don't have to capture it
// Sem thread_limiter(4);


template <typename V, typename U>
void demo(const U trials)
{
	std::vector<V> v_tInp(30);
	std::vector<std::future<V>> v_futOut;

	Sem thread_limiter(4);

	for_each(v_tInp, [] (U i, V & e)
	{
		e = (i % 10) + 35;
	});
	std::cout << "v_tInp args:";
	for (const auto v : v_tInp)
		std::cout << " " << v;
	std::cout << std::endl;

	for_each(v_tInp, [&v_futOut, &thread_limiter] (U i, V e)
	{
		v_futOut.push_back(std::async(std::launch::async, [&thread_limiter] (U task, V n) -> V {
			SemWaiterNotifier w(thread_limiter);
			std::printf("Start %d\n", task);
			const auto res = fib(n);
			std::printf("\t\t\t\tEnd   %d\n", task);
			return res;
		}, i, e));
	});

	for_each(v_futOut, [] (V i, std::future<V> & e)
	{
		std::printf("\t\tWait  %d\n", i);
		const auto res = e.get();
		std::printf("\t\t\t\t\t\tResult %d: %d\n", i, res);
	});
}


#if 0
class Driver final : public IDriver<std::queue<int>>
{
protected:

	void _producer(Producer & prod, const int iIncomingMs, const int iOutgoingMs) override
	{}

	void _consumer(int idx, int iOutgoingMs, size_t & uMaxTasksEver, uint & uMiss) override
	{}
};
#endif


int main(int argc, char ** argv)
{

	demo<int>(100);

#if 0
	auto vm = arg_parse(argc, argv);
	// NJOBS forced with 1
	Driver driver;
	driver.drive(1, vm["nblobs"].as<int>(), vm["inms"].as<int>(), vm["outms"].as<int>());
#endif
	return iExitValue;
}
