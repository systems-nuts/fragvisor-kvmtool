#!/bin/bash

BIN=""
TEST_DIR=""
MACHINES="echo4 fox4"
#THREADS="24:144"
#THREADS="20:120"
THREADS="16:96"
#THREADS="18:112"
#THREADS="16:0"
#THREADS="0:96"
#THREADS="8:48"
RATIOS="1" # export from outside
#RATIOS="1:1"
#RATIOS="2:1 5:1"
#RATIOS="3:2 2:1 9:4 5:2 11:4 3:1 7:2 4:1 9:2 5:1"
RUNS="1"
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
	#TEST_DIR="$(readlink -f $(basename $BIN)_$(date +'%R-%a-%b-%d-%Y'))"
	TSTAMP=`date +%Y%m%d_%T | sed 's/:/_/g'`
	TEST_DIR="$(readlink -f $(basename $BIN)_$TSTAMP)"
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
		export OMP_SCHEDULE=STATIC

		echo "-> Running with $xthr Xeon threads and $athr Cavium threads <-"

		for r in $RATIOS; do
			#local xrat=$(item $r 1)
			#local yrat=$(item $r 2)
			local xrat=$1
			local yrat=$2
			echo "inner \$1 $1 \$2 $2 {$xrat},{$yrat}"
			export POPCORN_HET_WORKSHARE="{$xrat},{$yrat}"
			echo "(inner last) export POPCORN_HET_WORKSHARE=\"{$xrat},{$yrat}\""
			#echo "jack todo remove \":\""
			#echo "jack learn: \"$r\""
			#echo "jack learn: \"xrat=\$(item \$r 1)\""
			trimed1=`echo ${t} | sed 's/:/_/g'`
			trimed2=`echo ${r} | sed 's/:/_/g'`

			trimed2="${xrat}_${yrat}"
			#echo "\$trimed2 = $trimed2"
			# File Name
			#local log="$TEST_DIR/${t}_${r}.log"
			#echo "file name = $TEST_DIR/${trimed1}_${trimed2}.log"
			local log="$TEST_DIR/${trimed1}_${trimed2}.log"
			echo "file name \%log = $TEST_DIR/${trimed1}_${trimed2}.log"

			echo "  + $r Xeon/Cavium ratio"
			echo "     Run: $BIN"
			#echo -n "     Run"

			for i in $(seq $RUNS); do
				echo -n " iter $i"
				echo "--------" >> $log
				echo " Run $i" >> $log
				echo "--------" >> $log
				echo >> $log
				#(time $BIN -t $nthr) &>> $log
				echo "time $BIN -t $nthr"
				(time $BIN -t $nthr) 2>&1 | tee -a $log
				echo >> $log
				sleep 1

				########## for popcorn barrier
				for node in $MACHINES; do
					echo "$node time: " | tee -a $LOG
					ssh $node "sudo bash -c \"sudo dmesg\"" | \
							grep popcorn_global_time | grep -v "nd clean popcorn_g" \
																		| tee -a $log
				done
				
				########## 
				for node in $MACHINES; do
					echo "$node mw_time: " | tee -a $LOG
					ssh $node "sudo bash -c \"sudo dmesg\"" | \
											grep "mw_time:" | tee -a $log
				done
				########## 
				for node in $MACHINES; do
					echo "$node pf_time: " | tee -a $LOG
					ssh $node "sudo bash -c \"sudo dmesg\"" | \
											grep "pf_time:" | tee -a $log
				done
				########## 
				for node in $MACHINES; do
					echo "$node mw_cnt: " | tee -a $LOG
					ssh $node "sudo bash -c \"sudo dmesg\"" | \
											grep "smart_region_skip" | tee -a $log
				done
				########## 
				for node in $MACHINES; do
					echo "$node pf_cnt: " | tee -a $LOG
					ssh $node "sudo bash -c \"sudo dmesg\"" | \
											grep "real prefetch cnt" | tee -a $log
				done
				##########
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

echo "inner \$1 $1 \$2 $2 \$3 $3 \$4 $4"
echo "inner \$3 $3 \$4 $4"
xrat=$3
yrat=$4

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
run_test $xrat $yrat
cleanup_test
