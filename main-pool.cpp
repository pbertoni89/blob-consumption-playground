#include <thread>

#include "utils.h"


class Driver final : public IDriver
{
protected:
	ConcurrentQueue<std::pair<std::shared_ptr<Blob>, t_tp>> m_qSpBlobs;

	[[nodiscard]] size_t size() const override { return m_qSpBlobs.size(); }

	[[nodiscard]] bool empty() const override { return m_qSpBlobs.empty(); }

	void _producer(Producer & prod, const int iIncomingMs, const int iOutgoingMs) override
	{
		const auto t0 = tic();

		m_qSpBlobs.push({std::make_shared<Blob>(prod()), t0});
	}

	void _consumer(const int idx, const int iOutgoingMs, size_t & uMaxTasksEver) override
	{
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
};


int main(int argc, char ** argv)
{
	auto vm = arg_parse(argc, argv);
	Driver driver;
	driver.drive(vm["njobs"].as<int>(), vm["nblobs"].as<int>(), vm["inms"].as<int>(), vm["outms"].as<int>());
	return 0;
}
