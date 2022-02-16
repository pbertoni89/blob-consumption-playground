#include <thread>

#include "utils.h"


class Driver final :
	public IDriver<ConcurrentQueue<std::pair<std::shared_ptr<Blob>, t_tp>>>
{
protected:
	void _producer(Producer & prod, const int iIncomingMs, const int iOutgoingMs) override
	{
		const auto t0 = tic();
		m_q.push({std::make_shared<Blob>(prod()), t0});
	}

	void _consumer(const int idx, const int iOutgoingMs, size_t & uMaxTaskEver, uint & uMaxMissEver, uint & uMiss) override
	{
		// we don't need to query for queue emptiness: already handled inside pop()
		// if (m_q.empty()) consumer_miss(idx, uMaxMissEver, uMiss) else {
		auto [spBlob, t0] = m_q.pop();
		const auto rv = work(spBlob, t0, iOutgoingMs);
		consumer_success(idx, rv, uMaxTaskEver, uMiss);
	}
};


int main(int argc, char ** argv)
{
	auto vm = arg_parse(argc, argv);
	Driver driver;
	driver.drive(vm["njobs"].as<int>(), vm["nblobs"].as<int>(), vm["inms"].as<int>(), vm["outms"].as<int>());
	return iExitValue;
}
