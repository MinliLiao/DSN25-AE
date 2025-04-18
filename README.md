# DSN25 Artifact Evaluation for ParaVerser

This is the artifact for the artifact evaluation of DSN 2025, for the paper ``ParaVerser: Harnessing Heterogeneous Parallelism for Affordable Fault Detection in Data Centers". This artifact includes the modified gem5 simulator with our implementation of ParaVerser, scripts to compile and run simulations and scripts collect and plot results. 

## Hardware requirements
An x86-64 or ARM machine with sudo access (to install dependencies and mount images) and >16GB RAM.

## Software requirements
Linux system to run the gem5 simulator (we used Ubuntu 20.04.6 LTS or Ubuntu 24.04.2 LTS). 

An image of SPEC 2017 benchmarks (not in the artifact, **please obtain a SPEC 2017 distribution from your institution**). 

Cross compiler for aarch64 target are required to build the benchmarks with x86 machines (we used aarch64-linux-gnu-gcc/g++/gfortran 9.4.0). Native compilers are used to build the gem5 simulator (we used gcc/g++ version 9.4.0 or 12.3.0). Other miscellaneous packages we used include git (we used 2.43.0), bash (we used 5.2.21), make (we used 4.3).

## Build and install
Download the artifact and cd to the root directory of this artifact. Then to install the dependencies:
```
cd AE_scripts
bash dependencies.sh
```
Note that this script will attempt to install gcc/g++ version 12, but older versions may work as well. Also note that the default available version may not work directly for python (we used version 3.8, 3.10 did not work) and scons (we used version 4.1.0, 4.5.2 did not work). We used conda to install the older versions of python and scons, see the `AE_scripts/conda_env_setup.sh` script on installing and creating an environment with miniconda.

To build SPECspeed 2017 place your ISO file in the root directory of this artifact with filename ending in .iso, then in the `AE_scripts` directory run:
```
bash build_benchmarks.sh
```
This script also sets up the run directories.
If you are compiling SPEC 2017 with GCC 10 or later and wrf, cam4 and pop2 fail compiling, please add `-fallow-argument-mismatch` to line 204 of `spec_confs/aarch64_17.cfg` (see https://www.spec.org/cpu2017/Docs/faq.html#Build.08).
If omnetpp exits with error in application output, please compile with C++ standard 14 by inserting `620.omnetpp_s:  #lang='CXX'` and `PORTABILITY   = --std=c++14` after line 157 of `spec_confs/aarch64_17.cfg`.

## Compile and run

In the `AE_scripts` directory, compile the gem5 simulator:
```
bash build.sh
```
The script attempts to build gem5 using 4 cores with gcc/g++ version 12. These can be changed by modifying the CC/CXX variables and the -j option of the scons command on the second line of the script.

Then run the simulations, collect and plot the results with:
```
bash run_exps.sh
```
This script will attempt to run the 20 SPECspeed 2017 benchmarks in parallel if there are enough core and memory resources. Collected results and generated plots are in the `stats_and_plots` directory.

## Expectations
Due to the very long simulation time, this artifact provides a subset of results presented in the paper from Fig.6 and Fig.7 using shorter simulations (simulating 100M instructions instead of 1B). The results do not exactly match what is presented in the paper, but generally follow the same trend.

The simulation of a single benchmark with a single configuration takes less than 2 hours on our machine (Neoverse-N1), and results can be obtained within a few days assuming simulations of 4 benchmarks are run in parallel. The `run_exps.sh` script will use as many cores as the system has (up to 20 cores) to run all 20 benchmarks in parallel.

The generated plot `spec17_100M_slowdown.eps` is similar to the results shown in Fig. 6 of the paper. The geomean slowdown of DSN18 is the highest, that of 4\*A510 min_ED2P is slightly higher than 4\*A510 at 2GHz, the other configurations have lower slowdown.

The generated plot `spec17_100M_slowdown_opp.eps` is similar to the results shown in Fig. 7 of the paper, but without the range of slowdown from different checker core frequencies. The slowdown is generally very small and is less than 1\% in geomean with 2 X2 or 4 A510 checker cores. 

We also include in the `stats_and_plots` directory examples of plot and data generated using our setup in the corresponding `Example_*.eps` and `Example_*.data` files respectively.

## Evaluation customization
We provide the option to evaluate our results with extra evaluations that would take more simulation time. This is not required for the artifact evaluation.

In the `AE_scripts` directory, for results on opportunistic error detection coverage with error injection:
```
bash run_exps.sh ErrInj
```
The generated plot `spec17_errRate.eps` is similar to the results shown in Fig.8. 

Note that the scripted error injection experiment rely on pre-recorded result in `AE_scripts/detected_err.txt` on which injected errors are not masked for each benchmark to reduce simulation time. Please uncomment lines 37-48 in `AE_scripts/run_exps.sh` to generate this file for new binaries.

For NoC overhead results with slow NoC and hash mode:
```
bash run_exps.sh slowNoC
```
The generated plot `spec17_100M_slowdown_slowNoC.eps` is similar to the results shown in Fig.11.

Note that the scripted slow NoC experiment rely on pre-recorded values that are dependent on the application binary. Please refer to `AE_scripts/result_scripts/slowNoC_estimate_generation.md` for details on how to generate these values for new binaries. 

For results on simulations with 1B instructions (takes 10x longer than 100M instructions to simulate, NOT RECOMMENDED for artifact evaluation):
```
bash run_exps.sh 1B
```
The generated plot `spec17_1B_slowdown.eps` and `spec17_1B_slowdown_opp_errBar.eps` are similar to the results shown in Fig.6 and 7 respectively.
