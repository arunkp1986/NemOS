# NemOS
NemOS: A Framework for OS-Architecture Study of Hybrid Memory

an open-source framework, NemOS, based on gem5 and gemOS for hybrid memory exploration in the architecture and operating systems layers. NemOS provides a quick way to study hybrid memory systems 

## Build gem5 using instructions 
=====================================

1> The gem5 folder contains **ssp** modifications, build gem5 by following

https://www.gem5.org/documentation/learning_gem5/part1/building/

## Build gemOS 
==============================

1> run **make** inside gemOS

2> user space code is writte in **user/init.c**

3> we have provided sample init.c codes for users to build upon in **user** folder

## Run NemOS
======================================

1> uncompress **gemOS.img** disk image in **bench_diskimages**

2> use **run_NemOS.sh** script as below to start **NemOS**,**run_NemOS.sh** takes **output** folder as argument to save gem5 stats 

**run_NemOS.sh path_to_output**

3> Now **gem5** starts and you need to connect to gem5 using telnet in a different terminal

**telnet localhost 3456**, the port number is printed by gem5 as " Listening for connections on port 3456"

4>the terminal after connecting to "telnet" will show gemOS prompt as below

**GemOS#**

5> To run your user program in **init.c**, type **init** command on **GemOS# ** as below

**GemOS# init**

6> The init program executes and you can see output

7> To exit **gemOS** run, **exit** command as below on **GemOS# **

**GemOS# exit**

## Run SSP prototype
=======================================

1> The SSP userspace benchmark **init_ssp_benchmark.c**  is present in **gemOS/user** folder

2> Disk images for SSP benchmarks are present in **bench_diskimages**

3> unzip the disk image and pass it as argument to **--disk-image** argument in **run_NemOS.sh**
