#include <thread>

#include "utils.h"


template <typename T>
class ConcurrentQueue
{
public:
	mutable std::mutex m_mu;
	std::queue<T> m_qu;

public:
	[[nodiscard]] T pop()
	{
		if (m_qu.empty())
			throw std::logic_error("popping from empty queue");
		auto t = m_qu.front();
		m_qu.pop();
		return std::move(t);
	}

	void push(const T & t)
	{
		{
			std::unique_lock lock(m_mu);
			m_qu.push(t);
		}
		if (bDebug)
			LOG(trace) << "push const& " << t.first->m_id;
	}

	template <typename... Args>
	void push(Args && ... args)
	{
		{
			std::unique_lock lock(m_mu);
			m_qu.emplace(std::forward<Args>(args)...);
		}
		if (bDebug)
			LOG(trace) << "push ...";
	}

	void push(T && t)
	{
		{
			std::unique_lock lock(m_mu);
			if (bDebug)
				LOG(trace) << "push && " << t.first->m_id;
			m_qu.push(std::move(t));
		}
	}

	[[nodiscard]] size_t size() const { return m_qu.size(); }
	[[nodiscard]] bool empty() const { return m_qu.empty(); }
};


using Pair = std::pair<std::shared_ptr<Blob>, t_tp>;
using FinalQueue = ConcurrentQueue<Pair>;


class Driver final :
	public IDriver<FinalQueue>
{
protected:
	void _produce(Producer & prod, const int iIncomingMs, const int iOutgoingMs) override
	{
		const auto t0 = tic();
		m_q.push({std::make_shared<Blob>(prod()), t0});
		if (bDebug) LOG(trace) << "pushed";
	}

	void _consume(const int idx, const int iOutgoingMs, size_t & uMaxTaskEver, uint & uMaxMissEver, uint & uMiss) override
	{
		bool bConsume{};
		Pair pair{nullptr, {}};
		// DOBBIAMO ECCOME e dobbiamo prendere il mutex QUA. Altrimenti è probabilissimo rompere la regione critica

		{
			std::lock_guard lock(m_q.m_mu);
			bConsume = not m_q.empty();
			if (bConsume)
			{
				pair = m_q.pop();
			}
		}

		if (bConsume)
		{
			const auto rv = work(pair.first, pair.second, iOutgoingMs);
			consumer_success(idx, rv, uMaxTaskEver, uMiss);
		}
		else
			consumer_miss(idx, uMaxMissEver, uMiss);
	}
};


int main(int argc, char ** argv)
{
	auto vm = arg_parse(argc, argv);
	Driver driver;
	driver.drive(vm["njobs"].as<int>(), vm["nblobs"].as<int>(), vm["inms"].as<int>(), vm["outms"].as<int>());
	return iExitValue;
}
