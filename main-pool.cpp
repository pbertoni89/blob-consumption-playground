#include <future>
#include <thread>

#include "utils.h"


class IBlobEngineClientManager
{
protected:
	ConcurrentQueue<std::pair<std::shared_ptr<Blob>, t_tp>> m_qSpBlobs;

public:
	void th_produce(const int iIncomingMs)
	{
		Producer prod;

		while (bWork)
		{
			iWorkProd ++;
			if (bDebug)
				LOG(info) << "producer " << iWorkProd << ", backlog " << m_qSpBlobs.size();

			std::this_thread::sleep_for(std::chrono::milliseconds(iIncomingMs));

			const auto t0 = tic();

			m_qSpBlobs.push({std::make_shared<Blob>(prod()), t0});
		}
		LOG(info) << "producer end with " << iWorkProd;
	}

	void th_consume(const int idx, const int iOutgoingMs)
	{
		size_t uMaxTasksEver = 0;

		while (bWork or not m_qSpBlobs.empty())
		{
			// at least because that's a concurrent queue and somebody may be pushing right now
			if (m_qSpBlobs.empty())
			{
				if (bDebug)
					LOG(info) << "consumer " << idx << " empty";
				std::this_thread::sleep_for(1ms);
			}
			else
			{
				auto [spBlob, t0] = m_qSpBlobs.pop();
				const auto rv = work(spBlob, t0, iOutgoingMs);

				iWorkCons ++;
				uMaxTasksEver = std::max(m_qSpBlobs.size(), uMaxTasksEver);
				if (bDebug)
					LOG(info) << "consumer " << idx << " work " << iWorkCons << ", rv " << rv;
			}
		}
		LOG(info) << "consumer " << idx << " end with " << iWorkCons << ", max futures ever " << uMaxTasksEver;
	}


	void persist(const int njobs, const int nblobs, const int in_ms, const int out_ms)
	{
		const auto ta = alpha(njobs, nblobs, in_ms, out_ms);
		std::vector<std::thread> vt;

		vt.emplace_back(&IBlobEngineClientManager::th_produce, this, in_ms);

		for (auto i=0; i<njobs; i++)
			vt.emplace_back(&IBlobEngineClientManager::th_consume, this, i, out_ms);

		for (auto & t : vt)
			t.join();

		omega(njobs, nblobs, in_ms, out_ms, ta);
	}
};


int main(int argc, char ** argv)
{
	auto vm = arg_parse(argc, argv);
	IBlobEngineClientManager ibecm;
	ibecm.persist(vm["njobs"].as<int>(), vm["nblobs"].as<int>(), vm["inms"].as<int>(), vm["outms"].as<int>());
	return 0;
}
