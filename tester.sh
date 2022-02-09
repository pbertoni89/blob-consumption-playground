#!/bin/bash


function sigint_handler() {
	{ set +x ;} 2> /dev/null
	echo "${0} caught sigint, tear down everything"
	exit 1
}
trap sigint_handler INT

nCores=$(grep -c ^processor /proc/cpuinfo)

for myElf in pool async; do
	# shellcheck disable=SC2043
	for myIms in 34; do
		for myOms in 100 150; do
			for myJobs in 1 $((nCores / 2)) $((nCores)); do
				if [[ ${myJobs} -gt 1 && ${myElf} == "async" ]]; then break; fi
				set -x
				time cmake-build-release/${myElf} --inms ${myIms} --outms ${myOms} --njobs ${myJobs} --debug 0 --nblobs 1000
				{ set +x ;} 2> /dev/null
			done
		done
	done
done
