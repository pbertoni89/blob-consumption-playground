#!/bin/bash

# Run me with ./tester.sh | tee ./results.txt 2>&1

function sigint_handler() {
	{ set +x ;} 2> /dev/null
	echo "${0} caught sigint inside script, tear down everything"
	exit 1
}
# trap sigint_handler INT  # otherwise how could we trap it inside processes

nCores=$(grep -c ^processor /proc/cpuinfo)
myDebug=0
myBlobs=1000

for myElf in pool async; do
	# shellcheck disable=SC2043
	for myIms in 26 34 43; do
		for myOms in 50 100 150 200; do
			for myJobs in 1 $((nCores / 4)) $((nCores / 2)) $((nCores)); do
				if [[ ${myJobs} -gt 1 && ${myElf} == "async" ]]; then break; fi

				echo "--elf ${myElf} --inms ${myIms} --outms ${myOms} --njobs ${myJobs} --debug ${myDebug} --nblobs ${myBlobs}"
				if ! time cmake-build-release/${myElf} --inms ${myIms} --outms ${myOms} --njobs ${myJobs} --debug ${myDebug} --nblobs ${myBlobs}; then
					echo "${0} caught sigint inside elf, tear down everything"
					exit 2
				fi
			done
		done
	done
done
