#include <future>
#include <mutex>
#include <vector>
#include <iostream>
#include <cstdio>


// a semaphore class
//
// All threads can wait on this object. When a waiting thread
// is woken up, it does its work and then notifies another waiting thread.
// In this way only n threads will be be doing work at any time.
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
	SemWaiterNotifier(Sem &s) : s{s} { s.wait(); }
	~ SemWaiterNotifier() { s.notify(); }
};


// some inefficient work for our threads to do
[[nodiscard]] int fib(const int n)
{
	if (n<2) return n;
	return fib(n-1) + fib(n-2);
}


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
Sem thread_limiter(4);


template <typename V, typename U>
void demo(const U trials)
{
	std::vector<V> input(30);
	std::vector<std::future<V>> output;

	for_each(input, [] (U i, V & e)
	{
		e = (i%10) + 35;
	});
	std::cout << "input args:";
	for (const auto v : input)
		std::cout << " " << v;
	std::cout << std::endl;

	for_each(input, [&output] (U i, V e)
	{
		output.push_back(std::async(std::launch::async, [] (U task, V n) -> V {
			SemWaiterNotifier w(thread_limiter);
			std::printf("Start %d\n", task);
			const auto res = fib(n);
			std::printf("\t\t\t\tEnd   %d\n", task);
			return res;
		}, i, e));
	});

	for_each(output, [] (V i, std::future<V> & e)
	{
		std::printf("\t\tWait  %d\n", i);
		const auto res = e.get();
		std::printf("\t\t\t\t\t\tResult %d: %d\n", i, res);
	});
}


int main()
{
	demo<int>(100);
}
