#include "utils.h"

#include <thread>
#include <csignal>


std::atomic<bool> bWork = false, bDebug = false;
std::atomic<int> iWorkProd = 0, iWorkCons = false;

t_tp tic() noexcept
{
	return std::chrono::high_resolution_clock::now();
}


int work(const std::shared_ptr<Blob> & spBlob, const t_tp t0, const int iOutgoingMs)
{
	const auto t1 = tic();
	std::stringstream ss;
	int sum = 0;
	bool bOnTime = true;
	uint uLoops = 0;

	if (bDebug)
	{
		ss << "worker   " << std::this_thread::get_id() << " on " << spBlob << " - id " << spBlob->m_id << " ";
		LOG(info) << ss.str() << "start";
	}

	while (bOnTime)
	{
		sum += (spBlob->sum() + fib(spBlob->m_id % 40));
		bOnTime = (tictoc<std::chrono::milliseconds>(t1)) < iOutgoingMs;
		uLoops ++;
	}

	const auto t2 = tic();
	const auto TIME_LIFE_MS = tictoc<std::chrono::milliseconds>(t0, t2),
		TIME_WORK = tictoc<std::chrono::milliseconds>(t1, t2);
	if (bDebug)
		LOG(info) << ss.str() << "end: sum " << sum << ", loops " << uLoops << ", life [ms] "
				  << TIME_LIFE_MS << ", work [ms] " << TIME_WORK;
	return sum;
}


void my_handler(const int s)
{
	std::cerr << "\n";
	bWork = false;
	// LOG(fatal) << "caught sig " << s;
}


t_tp alpha(const int nJobs, const int nBlobs, const int iIncomingMs, const int iOutgoingMs)
{
	LOG(info) << "will run with " << nJobs << " jobs on " << nBlobs << " blobs";
	const auto EXP_RATE_S_INC = 1000 / iIncomingMs, EXP_RATE_S_OUT = (1000 / iOutgoingMs) * nJobs;
	LOG(info) << "alpha: jobs " << nJobs << ", blobs " << nBlobs
		<< ", prod " << iIncomingMs << " [ms] -> " << EXP_RATE_S_INC << " [u/s]"
		<< ", cons " << iOutgoingMs << " [ms] -> " << EXP_RATE_S_OUT << " [u/s]";
	iWorkProd = 0;
	iWorkCons = 0;
	bWork = true;
	return tic();
}


void omega(const int nJobs, const int nBlobs, const int iIncomingMs, const int iOutgoingMs, const t_tp tpAlpha)
{
	const auto ALPHA_OMEGA_MS = tictoc<std::chrono::milliseconds>(tpAlpha);
	const auto EXP_RATE_INC = ALPHA_OMEGA_MS / iIncomingMs,
		EXP_RATE_OUT = (ALPHA_OMEGA_MS / iOutgoingMs) * nJobs;
	LOG(info) << "omega: elapsed [ms] " << ALPHA_OMEGA_MS << ", prod " << iWorkProd << " act / " << EXP_RATE_INC << " exp, "
		<< "cons " << iWorkCons << " act / " << EXP_RATE_OUT << " exp";
}


boost::program_options::variables_map arg_parse(int argc, char ** argv)
{
	namespace po = boost::program_options;
	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "describe arguments")
		("debug", po::value<bool>()->default_value(false), "enable deep logging")
		("njobs", po::value<int>()->default_value(1), "number of jobs")
		("nblobs", po::value<int>()->default_value(0), "number of blobs (0 <-> endless)")
		("inms", po::value<int>()->default_value(100), "rate incoming ms")
		("outms", po::value<int>()->default_value(100), "rate outgoing ms");

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << "\n";
		exit(1);
	}

	bDebug = vm["debug"].as<bool>();

	struct sigaction sigIntHandler{};
	sigIntHandler.sa_handler = my_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, nullptr);

	return vm;
}
