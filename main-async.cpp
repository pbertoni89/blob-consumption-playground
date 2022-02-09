#include <thread>
#include <queue>
#include <future>

#include "utils.h"


class Driver final : public IDriver
{
protected:
	std::queue<std::future<int>> m_qProcessingFutures;

	[[nodiscard]] size_t size() const override { return m_qProcessingFutures.size(); }

	[[nodiscard]] bool empty() const override { return m_qProcessingFutures.empty(); }

	void _producer(Producer & prod, const int iIncomingMs, const int iOutgoingMs) override
	{
		const auto t0 = tic();
		auto spBlobIncoming = std::make_shared<Blob>(prod());

		m_qProcessingFutures.emplace(std::async(std::launch::async, work, spBlobIncoming, t0, iOutgoingMs));
	}

	void _consumer(int idx, int iOutgoingMs, size_t & uMaxTasksEver) override
	{
		const bool CONSUME = (not m_qProcessingFutures.empty()
							  and m_qProcessingFutures.front().wait_for(std::chrono::seconds(0)) == std::future_status::ready);

		if (CONSUME)
		{
			const auto rv = m_qProcessingFutures.front().get();
			m_qProcessingFutures.pop();

			iWorkCons ++;
			uMaxTasksEver = std::max(m_qProcessingFutures.size(), uMaxTasksEver);
			if (bDebug)
				LOG(info) << "consumer " << idx << " work " << iWorkCons << ", rv " << rv;
		}
		else
		{
			// std::this_thread::yield();  // deprecated: it may flood the CPU with events
			if (bDebug)
				LOG(info) << "consumer " << idx << " empty";
			std::this_thread::sleep_for(1ms);
		}
	}
};


int main(int argc, char ** argv)
{
	auto vm = arg_parse(argc, argv);
	// NJOBS forced with 1
	Driver driver;
	driver.drive(1, vm["nblobs"].as<int>(), vm["inms"].as<int>(), vm["outms"].as<int>());
	return 0;
}
