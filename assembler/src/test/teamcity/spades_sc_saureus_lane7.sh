#!/bin/bash

############################################################################
# Copyright (c) 2011-2012 Saint-Petersburg Academic University
# All Rights Reserved
# See file LICENSE for details.
############################################################################

set -e
pushd ../../../

rm -rf /tmp/data/output/spades_output/SAUREUS_LANE_7

#./spades_compile.sh
./spades.py --sc -m 160 -k 21,33,55 --12 data/input/S.aureus/sc_lane_7/bacteria_mda_lane7.fastq.gz -o /tmp/data/output/spades_output/SAUREUS_LANE_7

rm -rf ~/quast-1.3/SAUREUS_LANE_7/

python ~/quast-1.3/quast.py -R data/input/E.coli/MG1655-K12.fasta.gz -G data/input/E.coli/genes/genes.gff -O data/input/E.coli/genes/operons.gff -o ~/quast-1.3/SAUREUS_LANE_7/ /tmp/data/output/spades_output/SAUREUS_LANE_7/contigs.fasta

python src/test/teamcity/assess.py ~/quast-1.3/SAUREUS_LANE_7/transposed_report.tsv 112000 5 2543 99.8 5.1 1.0
exitlvl=$?
popd
