#!/bin/bash

outdir=$1
CWD=`pwd`

if [[ $# -ne 1 ]];then
    echo "pass output dir"
    exit 2
fi

echo $outdir

$CWD/gem5/build/X86/gem5.opt --outdir=$outdir $CWD/gem5/configs/example/fs.py --mem-size=3GB --nvm-size=2GB --caches --l3cache --cpu-type TimingSimpleCPU --hybrid-channel True --mem-type=DDR4_2400_16x4 --nvm-type=NVM_2666_1x64 --kernel=$CWD/gemOS/gemOS.kernel --disk-image=$CWD/bench_diskimages/gemOS.img

