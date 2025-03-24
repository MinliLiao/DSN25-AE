# Get input parameters
BENCHMARKSUITE=$1   # spec17, gapbs or parsec
SIMRESULT_DIR=$2    # Where to find input stats
RESULTS_DIR=$3      # Where to put gathered stats
NOCFILEDIR=$4
mkdir -p ${RESULTS_DIR}
RESULTS_PREFIX=${RESULTS_DIR}/NoCLat_${BENCHMARKSUITE}
CONFIG=NoCLat_${BENCHMARKSUITE}
NOCFILESUFFIX=${BENCHMARKSUITE}
# Grep input for NoC calculation
if [ "$BENCHMARKSUITE" == "spec17" ] || [ "$BENCHMARKSUITE" == "gapbs" ]; then
    grep "simSeconds" $SIMRESULT_DIR/* | python gather_AE.py --stat "simSeconds" --config ${CONFIG} --all > ${RESULTS_PREFIX}_simSeconds.csv && \
    grep ".dcache.overallAccesses::total" $SIMRESULT_DIR/* | python gather_AE.py --stat "L1D Acc" --config ${CONFIG} --all > ${RESULTS_PREFIX}_L1DAcc_m0.csv && \
    grep "system.l3.overallAccesses::total" $SIMRESULT_DIR/* | python gather_AE.py --stat "LLC Acc" --config ${CONFIG} --all > ${RESULTS_PREFIX}_LLCAcc_m0.csv && \
    (
        for num_checkers in 1 2 3 4
        do 
            python calculateNoCLat.py --op "4x4o" --num_mains 1 --num_checkers_per_main ${num_checkers} --csv "${RESULTS_PREFIX}_simSeconds.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv" > ${NOCFILEDIR}/NoCLat_4x4o${num_checkers}c_${NOCFILESUFFIX}.txt
        done
    )
    # python calculateNoCLat.py --op "4x4i" --num_mains 1 --csv "${RESULTS_PREFIX}_simSeconds.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv" > ${NOCFILEDIR}/NoCLat_4x4i_${NOCFILESUFFIX}.txt
    if [ "$BENCHMARKSUITE" == "spec17" ]; then    
        grep ".dcache.ReadReq.accesses::total" $SIMRESULT_DIR/* | python gather_AE.py --stat "L1D ReadAcc" --config ${CONFIG} --all > ${RESULTS_PREFIX}_L1DReadAcc_m0.csv && \
        grep ".dcache.SwapReq.accesses::total" $SIMRESULT_DIR/* | python gather_AE.py --stat "L1D SwapAcc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_L1DSwapAcc_m0.csv && \
        (
            for num_checkers in 1 2 4
            do 
                python calculateNoCLat.py --op "4x4o" --num_mains 1 --num_checkers_per_main ${num_checkers} --csv "${RESULTS_PREFIX}_simSeconds.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv" --slowNoC --slowNoCEstimate "slowNoC_estimates.csv" > ${NOCFILEDIR}/NoCLat_Slow4x4o${num_checkers}c_${NOCFILESUFFIX}.txt
                python calculateNoCLat.py --op "4x4o" --num_mains 1 --num_checkers_per_main ${num_checkers} --csv "${RESULTS_PREFIX}_simSeconds.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv,${RESULTS_PREFIX}_L1DReadAcc_m0.csv,${RESULTS_PREFIX}_L1DSwapAcc_m0.csv" --slowNoC --hashed > ${NOCFILEDIR}/NoCLat_HashSlow4x4o${num_checkers}c_${NOCFILESUFFIX}.txt
            done
        )
        # python calculateNoCLat.py --op "4x4i" --num_mains 1 --csv "${RESULTS_PREFIX}_simSeconds.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv,${RESULTS_PREFIX}_L1DReadAcc_m0.csv,${RESULTS_PREFIX}_L1DSwapAcc_m0.csv" --slowNoC --hashed > ${NOCFILEDIR}/NoCLat_HashSlow4x4i_${NOCFILESUFFIX}.txt
        # python calculateNoCLat.py --op "4x4i" --num_mains 1 --csv "${RESULTS_PREFIX}_simSeconds.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv" --slowNoC --slowNoCEstimate "slowNoC_estimates.csv" > ${NOCFILEDIR}/NoCLat_Slow4x4iX2_${NOCFILESUFFIX}.txt
        # python calculateNoCLat.py --op "4x4i" --num_mains 1 --csv "${RESULTS_PREFIX}_simSeconds.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv" --slowNoC --slowNoCEstimate "slowNoC_estimatesA510.csv" > ${NOCFILEDIR}/NoCLat_Slow4x4iA510_${NOCFILESUFFIX}.txt
    fi
elif [ "$BENCHMARKSUITE" == "4pspec17" ]; then
    grep "simSeconds" $SIMRESULT_DIR/*_baseline_*/stats.txt | python gather_AE.py --stat "simSeconds" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_simSeconds.csv
    for i in 0 1 2 3
    do
        grep "cpu${i}.dcache.overallAccesses::total" $SIMRESULT_DIR/*_baseline_*/stats.txt | python gather_AE.py --stat "L1D Acc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_L1DAcc_c${i}.csv
        grep "system.l3.overallAccesses::switch_cpus${i}.inst" $SIMRESULT_DIR/*_baseline_*/stats.txt | python gather_AE.py --stat "LLC Inst Acc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_LLCInstAcc_c${i}.csv
        grep "system.l3.overallAccesses::switch_cpus${i}.data" $SIMRESULT_DIR/*_baseline_*/stats.txt | python gather_AE.py --stat "LLC Data Acc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_LLCDataAcc_c${i}.csv
        grep "system.l3.overallAccesses::cpu${i}.dcache.prefetcher" $SIMRESULT_DIR/*_baseline_*/stats.txt | python gather_AE.py --stat "LLC L1pf Acc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_LLCL1pfAcc_c${i}.csv
        grep "system.l3.overallAccesses::cpu${i}.l2cache.prefetcher" $SIMRESULT_DIR/*_baseline_*/stats.txt | python gather_AE.py --stat "LLC L2pf Acc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_LLCL2pfAcc_c${i}.csv
        python calculate.py --stat "LLC Acc" --op "sum" --csv "${RESULTS_PREFIX}_LLCInstAcc_c${i}.csv,${RESULTS_PREFIX}_LLCDataAcc_c${i}.csv,${RESULTS_PREFIX}_LLCL1pfAcc_c${i}.csv,${RESULTS_PREFIX}_LLCL2pfAcc_c${i}.csv" > ${RESULTS_PREFIX}_LLCAcc_c${i}.csv
        mv ${RESULTS_PREFIX}_L1DAcc_c${i}.csv ${RESULTS_PREFIX}_L1DAcc_m${i}.csv
        mv ${RESULTS_PREFIX}_LLCAcc_c${i}.csv ${RESULTS_PREFIX}_LLCAcc_m${i}.csv
    done
    # python calculateNoCLat.py --op "4x4i" --num_mains 4 --csv "${RESULTS_PREFIX}_simSeconds.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv,${RESULTS_PREFIX}_L1DAcc_m1.csv,${RESULTS_PREFIX}_LLCAcc_m1.csv,${RESULTS_PREFIX}_L1DAcc_m2.csv,${RESULTS_PREFIX}_LLCAcc_m2.csv,${RESULTS_PREFIX}_L1DAcc_m3.csv,${RESULTS_PREFIX}_LLCAcc_m3.csv" > ${NOCFILEDIR}/NoCLat_4x4i_${NOCFILESUFFIX}.txt
    for num_checkers in 1 2 4
    do 
        python calculateNoCLat.py --op "4x4o" --num_mains 4 --num_checkers_per_main ${num_checkers} --csv "${RESULTS_PREFIX}_simSeconds.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv,${RESULTS_PREFIX}_L1DAcc_m1.csv,${RESULTS_PREFIX}_LLCAcc_m1.csv,${RESULTS_PREFIX}_L1DAcc_m2.csv,${RESULTS_PREFIX}_LLCAcc_m2.csv,${RESULTS_PREFIX}_L1DAcc_m3.csv,${RESULTS_PREFIX}_LLCAcc_m3.csv" > ${NOCFILEDIR}/NoCLat_4x4o${num_checkers}c_${NOCFILESUFFIX}.txt
    done
elif [ "$BENCHMARKSUITE" == "parsec" ]; then
    for i in 0 1 2 3 4
    do
        grep "cpu${i}.numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_numCycles_c${i}.csv
        grep "cpu${i}.dcache.overallAccesses::total" $SIMRESULT_DIR/* | python gather_AE.py --stat "L1D Acc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_L1DAcc_c${i}.csv
        grep "system.l3.overallAccesses::cpu${i}.inst" $SIMRESULT_DIR/* | python gather_AE.py --stat "LLC Inst Acc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_LLCInstAcc_c${i}.csv
        grep "system.l3.overallAccesses::cpu${i}.data" $SIMRESULT_DIR/* | python gather_AE.py --stat "LLC Data Acc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_LLCDataAcc_c${i}.csv
        grep "system.l3.overallAccesses::cpu${i}.dcache.prefetcher" $SIMRESULT_DIR/* | python gather_AE.py --stat "LLC L1pf Acc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_LLCL1pfAcc_c${i}.csv
        grep "system.l3.overallAccesses::cpu${i}.l2cache.prefetcher" $SIMRESULT_DIR/* | python gather_AE.py --stat "LLC L2pf Acc" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_LLCL2pfAcc_c${i}.csv
        python calculate.py --stat "LLC Acc" --op "sum" --csv "${RESULTS_PREFIX}_LLCInstAcc_c${i}.csv,${RESULTS_PREFIX}_LLCDataAcc_c${i}.csv,${RESULTS_PREFIX}_LLCL1pfAcc_c${i}.csv,${RESULTS_PREFIX}_LLCL2pfAcc_c${i}.csv" > ${RESULTS_PREFIX}_LLCAcc_c${i}.csv
    done
    python calculate.py --stat "numCycles" --op "max" --csv "${RESULTS_PREFIX}_numCycles_c0.csv,${RESULTS_PREFIX}_numCycles_c1.csv,${RESULTS_PREFIX}_numCycles_c2.csv,${RESULTS_PREFIX}_numCycles_c3.csv,${RESULTS_PREFIX}_numCycles_c4.csv" > ${RESULTS_PREFIX}_numCycles.csv
    python calculate.py --stat "L1D Acc" --op "sum" --csv "${RESULTS_PREFIX}_L1DAcc_c0.csv,${RESULTS_PREFIX}_L1DAcc_c2.csv,${RESULTS_PREFIX}_L1DAcc_c4.csv" > ${RESULTS_PREFIX}_L1DAcc_m0.csv
    python calculate.py --stat "L1D Acc" --op "sum" --csv "${RESULTS_PREFIX}_L1DAcc_c1.csv,${RESULTS_PREFIX}_L1DAcc_c3.csv" > ${RESULTS_PREFIX}_L1DAcc_m1.csv
    python calculate.py --stat "LLC Acc" --op "sum" --csv "${RESULTS_PREFIX}_LLCAcc_c0.csv,${RESULTS_PREFIX}_LLCAcc_c2.csv,${RESULTS_PREFIX}_LLCAcc_c4.csv" > ${RESULTS_PREFIX}_LLCAcc_m0.csv
    python calculate.py --stat "LLC Acc" --op "sum" --csv "${RESULTS_PREFIX}_LLCAcc_c1.csv,${RESULTS_PREFIX}_LLCAcc_c3.csv" > ${RESULTS_PREFIX}_LLCAcc_m1.csv
    # python calculateNoCLat.py --op "4x4i" --num_mains 2 --csv "${RESULTS_PREFIX}_numCycles.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv,${RESULTS_PREFIX}_L1DAcc_m1.csv,${RESULTS_PREFIX}_LLCAcc_m1.csv" > ${NOCFILEDIR}/NoCLat_4x4i_${NOCFILESUFFIX}.txt
    for num_checkers in 1 2 3 4
    do 
        python calculateNoCLat.py --op "4x4o" --num_mains 2 --num_checkers_per_main ${num_checkers} --csv "${RESULTS_PREFIX}_numCycles.csv,${RESULTS_PREFIX}_L1DAcc_m0.csv,${RESULTS_PREFIX}_LLCAcc_m0.csv,${RESULTS_PREFIX}_L1DAcc_m1.csv,${RESULTS_PREFIX}_LLCAcc_m1.csv" > ${NOCFILEDIR}/NoCLat_4x4o${num_checkers}c_${NOCFILESUFFIX}.txt
    done
else
    echo "Illegal benchmark suite option $BENCHMARKSUITE"
    echo "Valid options are SPEC17, GAPBS, PARSEC"
    exit 1
fi
