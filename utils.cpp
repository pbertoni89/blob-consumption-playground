//
// Created by pbertoni on 08/02/22.
//

#include "utils.h"

#include <thread>
#include <shared_mutex>

t_tp tic() noexcept
{
	return std::chrono::high_resolution_clock::now();
}


int blob_load(const std::shared_ptr<Blob> & spBlob, const t_tp t0, const bool bDebug, const int iOutgoingMs)
{
	std::stringstream ss;
	if (bDebug)
	{
		ss << "th " << std::this_thread::get_id() << " working on " << spBlob << " - id " << spBlob->m_id << " ";
		LOG(info) << ss.str() << "start";
	}

	int rv = 1;
	bool bOnTime = true;
	try
	{
		while (bOnTime)
		{
			const auto sum = spBlob->sum();
			// std::this_thread::sleep_for(std::chrono::milliseconds(iOutgoingMs));
			const auto ELAPSED = tictoc<std::chrono::milliseconds>(t0);
			bOnTime = ELAPSED < iOutgoingMs;
		}
		rv = 0;
	}
	catch (const std::runtime_error & RE)  // catches also xis::utils::tcp::connection_error
	{
		LOG(error) << "while consuming: " << RE.what();
	}

	if (bDebug)
		LOG(info) << ss.str() << "end within [ms] " << tictoc<std::chrono::milliseconds>(t0);
	return rv;
}
