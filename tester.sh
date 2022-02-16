#!/bin/bash

# Run me with
# mkdir -p ./data-${HOSTNAME} && ./tester.sh | tee ./data-${HOSTNAME}/results.txt 2>&1

function sigint_handler() {
	{ set +x ;} 2> /dev/null
	echo "${0} caught sigint inside script, tear down everything"
	exit 1
}
# trap sigint_handler INT  # otherwise how could we trap it inside processes

nCores=$(grep -c ^processor /proc/cpuinfo)

while getopts ":bdh:" opt; do
	case "${opt}" in
		b)
			echo "will be brief"
			myBrief=y ;;
		d)
			echo "will debug"
			myDebug=y ;;
		h)
			echo "usage: ${0} [-d debug] [-b brief]" && exit 0 ;;
		*)
			usage && exit 1 ;;
	esac
done
shift $((OPTIND-1))


if [[ -n ${myBrief} ]]; then
	myDebug=1
	arrIms=(34)
	arrOms=(200)
	arrBlobs=(10)
	arrJobs=(1 $((nCores)))
else
	myDebug=0
	arrIms=(26 34 43)
	arrOms=(50 100 150 200)
	arrBlobs=(100 1000)
	arrJobs=(1 $((nCores / 2)) $((nCores)))
fi

numTestsPool=$(( ${#arrBlobs[@]} * ${#arrIms[@]} * ${#arrOms[@]} * ${#arrJobs[@]} ))
numTestsAsnc=$(( ${#arrBlobs[@]} * ${#arrIms[@]} * ${#arrOms[@]} * 1 ))
numTests=$(( numTestsPool + numTestsAsnc ))
idxTest=0

for myElf in pool async; do
	# shellcheck disable=SC2043
	for myBlobs in "${arrBlobs[@]}"; do
		for myIms in "${arrIms[@]}"; do
			for myOms in "${arrOms[@]}"; do
				for myJobs in "${arrJobs[@]}"; do
					# skip useless runs, will be fixed during post-processing
					if [[ ${myJobs} -gt 1 && ${myElf} == "async" ]]; then break; fi
					idxTest=$(( idxTest + 1 ))
					# ensure sudden flush with >&2 ? AVOID
					echo "test(${idxTest}/${numTests}) --elf ${myElf} --inms ${myIms} --outms ${myOms} --njobs ${myJobs} --debug ${myDebug} --nblobs ${myBlobs}"
					# do the actual work
					if [[ -n ${myDebug} ]]; then
						gdb -q -ex r --args cmake-build-debug/${myElf} --inms "${myIms}" --outms "${myOms}" --njobs "${myJobs}" --debug ${myDebug} --nblobs "${myBlobs}"
					else
						if ! time cmake-build-release/${myElf} --inms "${myIms}" --outms "${myOms}" --njobs "${myJobs}" --debug ${myDebug} --nblobs "${myBlobs}"; then
							echo "${0} caught error inside elf, tear down everything"
							exit 2
						fi
					fi
				done
			done
		done
	done
done
