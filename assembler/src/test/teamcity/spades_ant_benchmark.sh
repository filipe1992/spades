#!/bin/bash

./prepare_cfg
errlvl=$?
if [ "$errlvl" -ne 0 ]
then
    echo "prepare_cfg finished with exit code $errlvl"
    exit $errlvl
fi

./spades_compile.sh
errlvl=$?
if [ "$errlvl" -ne 0 ]
then
    echo "spades_compile finished with exit code $errlvl"
    exit $errlvl
fi

timestamp="`date +%Y-%m-%d_%H-%M-%S`"_$1
if [ "$#" > 0 ]
then
    timestamp=$timestamp"_"$1
fi

mkdir -p ./ant_benchmark

echo "Starting E.coli is220 on ant16..."
rm -f ./ant_benchmark/ECOLI_IS220.log
srun -w ant16 ./src/test/teamcity/teamcity.py /tmp/data/input/E.coli/is220/ECOLI_IS200.info $1 >> ./ant_benchmark/ECOLI_IS220_$timestamp.log 2>> ./ant_benchmark/ECOLI_IS220_$timestamp.log &
echo "done"

echo "Starting E.coli UCSD lane 1 on ant01..."
rm -f ./ant_benchmark/ECOLI_SC_LANE_1.log
srun -w ant01 ./src/test/teamcity/teamcity.py /tmp/data/input/E.coli/sc_lane_1/corrected/ECOLI_SC_LANE_1_CORR.info $1 >> ./ant_benchmark/ECOLI_SC_LANE_1_$timestamp.log 2>> ./ant_benchmark/ECOLI_SC_LANE_1_$timestamp.log &
echo "done"

echo "Starting M.ruber JGI lane 9 on ant02..."
rm -f ./ant_benchmark/MRUBER_JGI_LANE_9.log
srun -w ant02 ./src/test/teamcity/teamcity.py /tmp/data/input/M.ruber/jgi_lane_9/MRUBER_JGI_LANE_9_MANUAL.info $1 >> ./ant_benchmark/MRUBER_JGI_LANE_9_$timestamp.log 2>> ./ant_benchmark/MRUBER_JGI_LANE_9_$timestamp.log &
echo "done"


echo "Starting P.heparinus JGI lane 7 on ant16..."
rm -f ./ant_benchmark/PHEPARINUS_JGI_LANE_7.log
srun -w ant16 ./src/test/teamcity/teamcity.py /tmp/data/input/P.heparinus/jgi_lane_7/PHEPARINUS_JGI_LANE_7.info $1 >> ./ant_benchmark/PHEPARINUS_JGI_LANE_7_$timestamp.log 2>> ./ant_benchmark/PHEPARINUS_JGI_LANE_7_$timestamp.log &
echo "done"
