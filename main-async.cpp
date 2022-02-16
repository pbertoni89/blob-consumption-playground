#include <thread>
#include <queue>
#include <future>

#include "utils.h"


class Driver final :
	public IDriver<std::queue<std::future<int>>>
{
protected:
	void _produce(Producer & prod, const int iIncomingMs, const int iOutgoingMs) override
	{
		const auto t0 = tic();
		m_q.emplace(std::async(std::launch::async, work, std::make_shared<Blob>(prod()), t0, iOutgoingMs));
	}

	void _consume(const int idx, const int iOutgoingMs, size_t & uMaxTaskEver, uint & uMaxMissEver, uint & uMiss) override
	{
		const bool CONSUME = (not m_q.empty()
							  and m_q.front().wait_for(std::chrono::seconds(0)) == std::future_status::ready);

		if (CONSUME)
		{
			const auto rv = m_q.front().get();
			m_q.pop();
			consumer_success(idx, rv, uMaxTaskEver, uMiss);
		}
		else
			consumer_miss(idx, uMaxMissEver, uMiss);
	}
};


int main(int argc, char ** argv)
{
	auto vm = arg_parse(argc, argv);
	// NJOBS forced with 1
	Driver driver;
	driver.drive(1, vm["nblobs"].as<int>(), vm["inms"].as<int>(), vm["outms"].as<int>());
	return iExitValue;
}
