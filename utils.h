#pragma once

#include <mutex>
#include <chrono>
#include <queue>
#include <atomic>
#include <string>
#include <thread>
#include <memory>
#include <future>
#include <condition_variable>

#include <boost/program_options.hpp>
#include <boost/random.hpp>
#include <boost/generator_iterator.hpp>
#include <boost/log/trivial.hpp>

#define LOG(sev) BOOST_LOG_TRIVIAL(sev)

using namespace std::chrono_literals;

using t_tp = std::chrono::time_point<std::chrono::high_resolution_clock>;


extern std::atomic<bool> bWork, bDebug;
extern std::atomic<int> iWorkProd, iWorkCons, iExitValue;


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


// constexpr size_t SZ_DATA = 1024 * 1024;   // SEEMS REALLY HEAVY
constexpr size_t SZ_DATA = 128;


class Inferrable
{
	std::vector<int> m_i;
public:
	int m_id;

	explicit Inferrable(const int _id) : m_i(SZ_DATA), m_id(_id) {}

	[[nodiscard]] decltype(auto) sum() const noexcept
	{
		return std::accumulate(m_i.begin(), m_i.end(), 0.0);
	}
};


class Blob : public Inferrable
{
	std::vector<int> m_b;
public:
	explicit Blob(const int _id) : Inferrable(_id), m_b(SZ_DATA * 2)
	{
		if (bDebug)
			LOG(trace) << "ctor blob " << _id;
	}

	~ Blob()
	{
		if (bDebug)
			LOG(trace) << "dtor blob " << m_id;
	}
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


int work(const std::shared_ptr<Blob> & spBlob, t_tp t0, int iOutgoingMs);


[[nodiscard]] t_tp alpha(int njobs, int nblobs, int iIncomingMs, int iOutgoingMs);
void omega(int njobs, int nblobs, int iIncomingMs, int iOutgoingMs, t_tp tpAlpha);

boost::program_options::variables_map arg_parse(int argc, char ** argv);


[[nodiscard]] bool has_work(const char * const reason, std::atomic<int> & ai) noexcept;


template <typename Q>
class IDriver
{
protected:
	Q m_q;

	virtual void _produce(Producer & prod, int iIncomingMs, int iOutgoingMs) = 0;
	virtual void _consume(int idx, int iOutgoingMs, size_t & uMaxTaskEver, uint & uMaxMissEver, uint & uMiss) = 0;
	[[nodiscard]] decltype(auto) size() const { return m_q.size(); }
	[[nodiscard]] decltype(auto) empty() const { return m_q.empty(); }

	inline void consumer_success(const int idx, const int rv, size_t & uMaxTaskEver, uint & uMiss)
	{
		iWorkCons ++;
		uMaxTaskEver = std::max(m_q.size(), uMaxTaskEver);
		uMiss = 0;
		if (bDebug)
			LOG(info) << "consumer " << idx << " work " << iWorkCons << ", rv " << rv;
	}

	inline void consumer_miss(const int idx, uint & uMaxMissEver, uint & uMiss)
	{
		constexpr uint MISS_BEFORE_THROW = 1e5, MISS_LOG = 5e3;  // one second at least bc 1us
		uMaxMissEver = std::max(uMaxMissEver, ++uMiss);
		if (uMiss >= MISS_BEFORE_THROW)
			throw std::runtime_error("too many consumer miss");

		if (bDebug)
		{
			// TODO LOG(trace) << "consumer " << idx << " miss: " << uMiss;
			std::this_thread::sleep_for(1ms);  // slow down log flood
		}
		else
		{
			if (uMiss % MISS_LOG == 0)
				LOG(warning) << "consumer " << idx << " miss: " << uMiss;
			std::this_thread::sleep_for(1ms);
			// std::this_thread::sleep_for(1us);  // FIXME starvation ??
			// std::this_thread::yield();         // FIXME starvation ???
		}
	}

public:
	void th_produce(const int iIncomingMs, const int iOutgoingMs)
	{
		Producer prod;

		while (has_work("producer", iWorkProd))
		{
			iWorkProd ++;
			if (bDebug)
				LOG(info) << "producer " << iWorkProd << ", backlog " << size();

			std::this_thread::sleep_for(std::chrono::milliseconds(iIncomingMs));

			_produce(prod, iIncomingMs, iOutgoingMs);
		}
		bWork = false;  // FIXME experimental
		LOG(info) << "producer end with " << iWorkProd;
	}

	void th_consume(const int idx, const int iOutgoingMs)
	{
		const std::string NICK = "consumer " + std::to_string(idx);
		size_t uMaxTaskEver = 0;
		uint uMaxMissEver = 0, uMiss = 0;

		LOG(info) << NICK << " start with " << iWorkCons;

		while (has_work(NICK.c_str(), iWorkCons) or not empty())
		{
			_consume(idx, iOutgoingMs, uMaxTaskEver, uMaxMissEver, uMiss);
		}
		LOG(info) << NICK << " end with " << iWorkCons << ", MaxTaskEver " << uMaxTaskEver << ", MaxMissEver " << uMaxMissEver;
	}

	void drive(const int njobs, const int nblobs, const int in_ms, const int out_ms)
	{
		// FIXME iff async, nJobs shall be equal to NUM_CORES for alpha, omega stats ONLY

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
