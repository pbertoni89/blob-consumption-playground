#pragma once

#include <mutex>
#include <chrono>
#include <queue>
#include <atomic>
#include <string>
#include <thread>
#include <memory>
#include <condition_variable>

#include <boost/program_options.hpp>
#include <boost/random.hpp>
#include <boost/generator_iterator.hpp>
#include <boost/log/trivial.hpp>

#define LOG(sev) BOOST_LOG_TRIVIAL(sev)

using namespace std::chrono_literals;

using t_tp = std::chrono::time_point<std::chrono::high_resolution_clock>;


extern std::atomic<bool> bWork, bDebug;
extern std::atomic<int> iWorkProd, iWorkCons;


[[nodiscard]] t_tp tic() noexcept;

template<typename T>
[[nodiscard]] decltype(auto)
tictoc(t_tp t0, t_tp t1) noexcept
{
	return std::chrono::duration_cast<T>(t1 >= t0 ? t1 - t0 : t0 - t1).count();
}


template<typename T>
[[nodiscard]] decltype(auto)
tictoc(t_tp t0) noexcept
{
	return tictoc<T>(t0, tic());
}


template<typename T>
[[nodiscard]]
T vector_sum(const std::vector<T> & v)
{
	const T sum = std::accumulate(v.begin(), v.end(), 0.0);
	return sum;
}


constexpr size_t SZ_DATA = 1024;


class Inferrable
{
	std::vector<int> m_i;
public:
	int m_id;

	explicit Inferrable(const int _id) : m_i(SZ_DATA), m_id(_id) {}

	[[nodiscard]] decltype(auto) sum() const noexcept
	{
		return vector_sum(m_i);
	}
};


class Blob : public Inferrable
{
	std::vector<int> m_b;
public:
	explicit Blob(const int _id) : Inferrable(_id), m_b(SZ_DATA * 2) {}
};


class Producer
{
	boost::mt19937 rng;
	boost::uniform_int<> one_to_six;
	boost::variate_generator<decltype(rng), boost::uniform_int<>> dice;

public:
	Producer() : one_to_six(1, 6666), dice(rng, one_to_six) {}

	int operator () () { return dice(); }
};


// some inefficient work for our threads to do
template <typename T>
[[nodiscard]] T fib(const T n)
{
	if (n<2) return n;
	return fib(n-1) + fib(n-2);
}


int work(const std::shared_ptr<Blob> & spBlob, const t_tp t0, const int iOutgoingMs);


template <typename T>
class ConcurrentQueue
{
	const size_t _SZ_MAX, _SZ_WRN;
	const bool _STRICT;
	const std::string _NICK;

	mutable std::mutex m_mu;
	std::queue<T> m_qu;
	std::condition_variable m_cv;

	inline void _roll_if_needed()
	{
		if (_SZ_WRN > 0 and m_qu.size() >= _SZ_WRN)
			LOG(warning) << "ConcurrentQueue " << _NICK << " overlap warn: " << m_qu.size() << " >= " << _SZ_WRN << ", max " << _SZ_MAX;

		while (_SZ_MAX > 0 and m_qu.size() >= _SZ_MAX)
		{
			if (_STRICT)
			{
				LOG(error) << "ConcurrentQueue " << _NICK << " overlap: " << m_qu.size() << " >= " << _SZ_MAX;
				throw std::runtime_error("strict ConcurrentQueue full");
			}
			else
			{
				const auto dropped = m_qu.front();
				// `<< dropped` would require an operator overload. I'm pretty sure enable_if could help here, but who cares
				LOG(error) << "ConcurrentQueue " << _NICK << " overlap drop: " << m_qu.size() << " >= " << _SZ_MAX;
				m_qu.pop();
			}
		}
	}

public:
	explicit ConcurrentQueue(const size_t size_max = 0, const size_t size_warn = 0, const bool strict = false, const std::string & nickname = "") :
		_SZ_MAX(size_max),
		_SZ_WRN(size_warn > 0
				? std::min(size_max, size_warn)
				: std::floor(0.9 * float(size_max))),
		_STRICT(strict and size_max > 0),
		_NICK(nickname)
	{}

	[[nodiscard]] T pop()
	{
		std::unique_lock<std::mutex> lock(m_mu);
		while (m_qu.empty())
		{
			m_cv.wait(lock);
		}
		const auto item = m_qu.front();
		m_qu.pop();
		return item;
	}

	void pop(T & item)
	{
		std::unique_lock<std::mutex> lock(m_mu);
		while (m_qu.empty())
		{
			m_cv.wait(lock);
		}
		item = m_qu.front();
		m_qu.pop();
	}

	void push(const T & item)
	{
		{
			std::unique_lock<std::mutex> lock(m_mu);
			_roll_if_needed();
			m_qu.push(item);
		}
		m_cv.notify_one();
	}

	template <typename... Args>
	void push(Args && ... args)
	{
		{
			std::unique_lock<std::mutex> lock(m_mu);
			_roll_if_needed();
			m_qu.emplace(std::forward<Args>(args)...);
		}
		m_cv.notify_one();
	}

	void push(T && item)
	{
		{
			std::unique_lock<std::mutex> lock(m_mu);
			_roll_if_needed();
			m_qu.push(std::move(item));
		}
		m_cv.notify_one();
	}

	[[nodiscard]] size_t size() const { std::unique_lock<std::mutex> lock(m_mu); return m_qu.size(); }
	[[nodiscard]] bool empty() const { std::unique_lock<std::mutex> lock(m_mu); return m_qu.empty(); }
};


[[nodiscard]] t_tp alpha(int njobs, int nblobs, int iIncomingMs, int iOutgoingMs);
void omega(int njobs, int nblobs, int iIncomingMs, int iOutgoingMs, t_tp tpAlpha);

boost::program_options::variables_map arg_parse(int argc, char ** argv);


[[nodiscard]] bool has_work(std::atomic<int> & ai) noexcept;


class IDriver
{
protected:
	virtual void _producer(Producer & prod, int iIncomingMs, int iOutgoingMs) = 0;
	virtual void _consumer(int idx, int iOutgoingMs, size_t & uMaxTasksEver) = 0;
	[[nodiscard]] virtual size_t size() const = 0;
	[[nodiscard]] virtual bool empty() const = 0;

public:
	void th_produce(const int iIncomingMs, const int iOutgoingMs)
	{
		Producer prod;

		while (has_work(iWorkProd))
		{
			iWorkProd ++;
			if (bDebug)
				LOG(info) << "producer " << iWorkProd << ", backlog " << size();

			std::this_thread::sleep_for(std::chrono::milliseconds(iIncomingMs));

			_producer(prod, iIncomingMs, iOutgoingMs);
		}
		LOG(info) << "producer end with " << iWorkProd;
	}

	void th_consume(const int idx, const int iOutgoingMs)
	{
		size_t uMaxTasksEver = 0;

		while (has_work(iWorkCons) or not empty())
		{
			_consumer(idx, iOutgoingMs, uMaxTasksEver);
		}
		LOG(info) << "consumer " << idx << " end with " << iWorkCons << ", max tasks ever " << uMaxTasksEver;
	}

	void drive(const int njobs, const int nblobs, const int in_ms, const int out_ms)
	{
		const auto ta = alpha(njobs, nblobs, in_ms, out_ms);

		std::vector<std::thread> vt;
		vt.emplace_back(&IDriver::th_produce, this, in_ms, out_ms);
		for (auto i=0; i<njobs; i++)
			vt.emplace_back(&IDriver::th_consume, this, i, out_ms);

		for (auto & t : vt)
			t.join();

		omega(njobs, nblobs, in_ms, out_ms, ta);
	}
};
