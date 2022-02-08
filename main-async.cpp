#include <iostream>
#include <thread>
#include <queue>
#include <future>
#include <mutex>
#include <csignal>
#include <shared_mutex>
#include <sstream>

#include "utils.h"


std::shared_mutex mtxGr;  // fake gui recipe
std::atomic<int> iWorkProd, iWorkCons;
std::atomic<bool> bWork;
bool bDebug;


void my_handler(int s)
{
	LOG(fatal) << "caught sig " << s;
	bWork = false;
}



constexpr bool ENABLE_PARALLEL_PROC = true;

constexpr size_t RATE_INCOMING_MS = 80, RATE_OUTGOING_MS = 200;


class IBlobEngineClientManager
{
protected:
	std::queue<std::future<int>> m_qProcessingFutures;

public:
	void th_produce(int iIncomingMs, int iOutgoingMs)
	{
		std::atomic<int> iFakeSemOut = 0;

		auto blob_independent_processing = [&] (const std::shared_ptr<Blob> & spBlob, const t_tp t0) -> int
		{
			std::stringstream ss;
			if (bDebug)
			{
				ss << "th " << std::this_thread::get_id() << " working on " << spBlob << " - id " << spBlob->m_id << " ";
				LOG(info) << ss.str() << "start";
			}

			int rv = 1;
			std::shared_lock<std::shared_mutex> lock(mtxGr);    // ensure recipe const correctness
			try
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(iOutgoingMs));
				rv = 0;
			}
			catch (const std::runtime_error & RE)  // catches also xis::utils::tcp::connection_error
			{
				LOG(error) << "while consuming: " << RE.what();
			}

			iFakeSemOut ++;
			if (bDebug)
				LOG(info) << ss.str() << "end within [ms] " << tictoc<std::chrono::milliseconds>(t0);
			return rv;
		};

		Producer prod;
		LOG(info) << "will produce with rates [ms]: incoming " << iIncomingMs << ", outgoing " << iOutgoingMs;

		while (bWork)
		{
			iWorkProd ++;
			if (bDebug)
				LOG(info) << "th produce " << iWorkProd;

			std::this_thread::sleep_for(std::chrono::milliseconds(iIncomingMs));

			const auto t0 = tic();

			auto spBlobIncoming = std::make_shared<Blob>(prod());

			if (ENABLE_PARALLEL_PROC)
			{
				// m_qProcessingFutures.emplace(std::async(std::launch::async, blob_independent_processing, spBlobIncoming, t0));
				m_qProcessingFutures.emplace(std::async(std::launch::async, blob_load, spBlobIncoming, t0, bDebug, iOutgoingMs));
			}
			else
			{
				const auto rv = blob_independent_processing(spBlobIncoming, t0);
			}
		}
		LOG(info) << "th produce end with " << iWorkProd;
	}

	void th_consume()
	{
		size_t uMaxFuturesEver = 0;

		while (bWork)
		{
			const bool CONSUME = (not m_qProcessingFutures.empty()
				and m_qProcessingFutures.front().wait_for(std::chrono::seconds(0)) == std::future_status::ready);

			if (CONSUME)
			{
				iWorkCons ++;

				const auto FUTURES_LEFT = m_qProcessingFutures.size() - 1;
				uMaxFuturesEver = std::max(FUTURES_LEFT, uMaxFuturesEver);
				auto rv = m_qProcessingFutures.front().get();
				m_qProcessingFutures.pop();

				if (bDebug and FUTURES_LEFT > 1)
					LOG(info) << "th consume " << iWorkCons << ", futures in queue " << FUTURES_LEFT;
			}
			else
			{
				// std::this_thread::yield();  // deprecated: it may flood the CPU with events
				std::this_thread::sleep_for(2ms);
			}
		}
		LOG(info) << "th consume end with " << iWorkCons << ", max futures ever " << uMaxFuturesEver;
	}


	void persist(int problem_dim, int in_ms, int out_ms)
	{
		iWorkProd = 0;
		iWorkCons = 0;
		bWork = true;
		std::vector<std::thread> vt;

		vt.emplace_back(&IBlobEngineClientManager::th_produce, this, in_ms, out_ms);
		if (ENABLE_PARALLEL_PROC)
			vt.emplace_back(&IBlobEngineClientManager::th_consume, this);

		for (auto & t : vt)
			t.join();
	}
};


/**
 *
 * @param argc
 * @param argv N_BLOBS, INCOMING_MS, OUTGOING_MS
 * @return
 */
int main(int argc, char ** argv)
{
	namespace po = boost::program_options;
	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "describe arguments")
		("debug", po::value<bool>()->default_value(false), "enable deep logging")
		("nblobs", po::value<int>()->default_value(50), "number of blobs")
		("inms", po::value<int>()->default_value(100), "rate incoming ms")
		("outms", po::value<int>()->default_value(100), "rate outgoing ms");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		return 1;
	}
	bDebug = vm["debug"].as<bool>();

	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = my_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, nullptr);

	std::cout << "Hello, World!" << std::endl;
	IBlobEngineClientManager ibecm;
	ibecm.persist(vm["nblobs"].as<int>(), vm["inms"].as<int>(), vm["outms"].as<int>());
	return 0;
}
