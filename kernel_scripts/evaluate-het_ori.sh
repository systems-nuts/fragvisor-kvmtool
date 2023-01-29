#!/bin/bash

BIN=""
TEST_DIR=""
MACHINES="echo7 fox7"
THREADS="16:96"
RATIOS="3:2 2:1 9:4 5:2 11:4 3:1 7:2 4:1 9:2 5:1"
RUNS="10"
PERFORMANCE=0
ME=$(hostname)

function die {
	echo "ERROR: $1!"
	exit 1
}

function item {
	echo $1 | sed -e 's/:/\n/g' | head -n $2 | tail -n 1
}

function print_help {
	echo "Evaluate an OpenMP application on a heterogeneous-ISA setup"
	echo
	echo "Options:"
	echo "  -h | --help              : print help & exit"
	echo "  -b BIN | --binary BIN    : binary to test (must already be replicated across nodes)"
	echo "  -r NUM | --runs NUM      : number of times to run each configuration (Default: $RUNS)"
	echo "  -p                       : run under the performance governor (Default: $PERFORMANCE)"
}

function run_cmd {
	local cur=$1
	local cmd="$2"
	if [[ $1 == $ME ]]; then
		$2
	else
		ssh $1 "$2"
	fi
}

function setup_test {
	# Create output directory & README
	TEST_DIR="$(readlink -f $(basename $BIN)_$(date +'%R-%a-%b-%d-%Y'))"
	mkdir $TEST_DIR || die "directory already exists"
	echo "Test date: $(date +'%A, %b %d %Y at %R')" >> $TEST_DIR/README
	echo "Runs per configuration: $RUNS" >> $TEST_DIR/README
	echo >> $TEST_DIR/README
	for m in $MACHINES; do
		echo "$m" >> $TEST_DIR/README
		echo "Running: $(run_cmd $m "uname -a")" >> $TEST_DIR/README
		echo "Binary: $BIN ($(run_cmd $m "md5sum $BIN"))" >> $TEST_DIR/README
		echo >> $TEST_DIR/README
	done

	# Set governor if requested
	if [[ $PERFORMANCE -eq 1 ]]; then
		echo "Set CPUs 0 - 16 to performance governor" >> $TEST_DIR/README
		for i in $(seq 16); do
			sudo cpufreq-set -c $((i - 1)) -g performance
		done
	fi

	# On Popcorn, we *have* to be in the same directory as the executable
	cd $(dirname $BIN)
	BIN="./$(basename $BIN)"
}

function run_test {
	export OMP_WAIT_POLICY=active

	for t in $THREADS; do
		local xthr=$(item $t 1)
		local athr=$(item $t 2)
		local nthr=$(($xthr + $athr))
		export POPCORN_PLACES="{$xthr},{$athr}"

		echo "-> Running with $xthr Xeon threads and $athr Cavium threads <-"

		for r in $RATIOS; do
			local xrat=$(item $r 1)
			local arat=$(item $r 2)
			export POPCORN_HET_WORKSHARE="{$xrat},{$arat}"
			local log="$TEST_DIR/${t}_${r}.log"

			echo "  + $r Xeon/Cavium ratio"
			echo -n "     Run"

			for i in $(seq $RUNS); do
				echo -n " $i"
				echo "--------" >> $log
				echo " Run $i" >> $log
				echo "--------" >> $log
				echo >> $log
				(time $BIN -t $nthr) &>> $log
				echo >> $log
				sleep 1
			done
			echo
		done
	done
}

function cleanup_test {
	if [[ $PERFORMANCE -eq 1 ]]; then
		for i in $(seq 16); do
			sudo cpufreq-set -c $((i - 1)) -g ondemand
		done
	fi
}

while [[ $1 != "" ]]; do
	case $1 in
		-h | --help) print_help; exit 0;;
		-b | --binary) BIN=$(readlink -f $2); shift;;
		-r | --runs) RUNS=$2; shift;;
		-p) PERFORMANCE=1;;
	esac
	shift
done

if [[ $BIN == "" ]]; then
	echo "Please specify a binary to run!"
	print_help
	die "did not specify a binary"
fi
if [[ ! -f $BIN ]]; then die "binary does not exist"; fi

setup_test
run_test
cleanup_test
