#pragma once

#include <chrono>

#include <boost/program_options.hpp>
#include <boost/random.hpp>
#include <boost/generator_iterator.hpp>
#include <boost/log/trivial.hpp>

#define LOG(sev) BOOST_LOG_TRIVIAL(sev)

using namespace std::chrono_literals;

using t_tp = std::chrono::time_point<std::chrono::high_resolution_clock>;

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

int blob_load(const std::shared_ptr<Blob> & spBlob, t_tp t0, bool bDebug, int iOutgoingMs);
