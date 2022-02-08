#include "utils.h"

#include <csignal>


bool bDebug;

void my_handler(int s)
{
	LOG(fatal) << "caught sig " << s;
}


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

	return 0;
}
