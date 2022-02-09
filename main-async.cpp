#include <thread>
#include <queue>
#include <future>

#include "utils.h"


class IBlobEngineClientManager
{
protected:
	std::queue<std::future<int>> m_qProcessingFutures;

public:
	void th_produce(const int iIncomingMs, const int iOutgoingMs)
	{
		Producer prod;

		while (bWork)
		{
			iWorkProd ++;
			if (bDebug)
				LOG(info) << "producer " << iWorkProd << ", backlog " << m_qProcessingFutures.size();

			std::this_thread::sleep_for(std::chrono::milliseconds(iIncomingMs));

			const auto t0 = tic();
			auto spBlobIncoming = std::make_shared<Blob>(prod());

			m_qProcessingFutures.emplace(std::async(std::launch::async, work, spBlobIncoming, t0, iOutgoingMs));
		}
		LOG(info) << "producer end with " << iWorkProd;
	}

	void th_consume()
	{
		size_t uMaxTasksEver = 0;

		while (bWork or not m_qProcessingFutures.empty())
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
					LOG(info) << "consumer work " << iWorkCons << ", rv " << rv;
			}
			else
			{
				// std::this_thread::yield();  // deprecated: it may flood the CPU with events
				if (bDebug)
					LOG(info) << "consumer empty";
				std::this_thread::sleep_for(1ms);
			}
		}
		LOG(info) << "consumer end with " << iWorkCons << ", max futures ever " << uMaxTasksEver;
	}


	void persist(const int njobs, const int nblobs, const int in_ms, const int out_ms)
	{
		const auto ta = alpha(njobs, nblobs, in_ms, out_ms);

		std::vector<std::thread> vt;
		vt.emplace_back(&IBlobEngineClientManager::th_produce, this, in_ms, out_ms);
		vt.emplace_back(&IBlobEngineClientManager::th_consume, this);

		for (auto & t : vt)
			t.join();

		omega(njobs, nblobs, in_ms, out_ms, ta);
	}
};


int main(int argc, char ** argv)
{
	auto vm = arg_parse(argc, argv);
	// NJOBS forced with 1
	IBlobEngineClientManager ibecm;
	ibecm.persist(1, vm["nblobs"].as<int>(), vm["inms"].as<int>(), vm["outms"].as<int>());
	return 0;
}
