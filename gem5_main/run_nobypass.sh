#!/bin/bash

outdir=$1
kernel=$2
diskimage=$3

if [[ $# -ne 3 ]];then
    echo "pass output dir,kernel,disk image"
    exit 2
fi

rm PhyMem.0
rm PhyMem.1
echo $outdir
echo $kernel
echo $diskimage

./build/X86/gem5.opt --outdir=$outdir configs/example/fs.py --mem-size=3GB --nvm-size=2GB --caches --l3cache --cpu-type TimingSimpleCPU --hybrid-channel True --mem-type=DDR4_2400_16x4 --nvm-type=NVM_2666_1x64 --kernel=$kernel --disk-image=$diskimage

