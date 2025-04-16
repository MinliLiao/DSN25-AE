M5OUTBASE=$(pwd)/../
STATSNPLOTS=$(pwd)/../stats_and_plots
NUMINST10B=10000000000
NUMINST100M=100000000
mkdir -p $(pwd)/../simresults
mkdir -p ${STATSNPLOTS}

EXTRA_OPT=$1
if [ "$EXTRA_OPT" == "" ]; then
    # Create checkpoints for SPEC17 benchmarks
    ./run_spec17_params_classicMem.sh checkpoint fastforward 1 ${NUMINST10B} ${M5OUTBASE}
    # Baseline runs
    ./run_spec17_params_classicMem.sh baseline restore 1 3GHz ${NUMINST100M} ${M5OUTBASE}
    # Get NoC induced latency from baseline runs
    cd result_scripts
    ./get_NoCLat.sh spec17 ../../simresults/${NUMINST100M} ${STATSNPLOTS} ../../spec_confs
    cd ..
    # SPEC17 100M runs
    ./run_spec17_params_classicMem.sh checked restore 1 12 3GHz 1000MHz DSN18A55 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checked restore 1 16 3GHz 1000MHz ParadoxA55 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 4 3GHz 2000MHz A510 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 4 3GHz 1800MHz A510 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 4 3GHz 1600MHz A510 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 4 3GHz 1400MHz A510 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 2 3GHz 1500MHz X2 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 1 3GHz 3000MHz X2 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh opportunisticNoC4x4o restore 1 4 3GHz 2000MHz A510 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh opportunisticNoC4x4o restore 1 2 3GHz 1500MHz X2 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh opportunisticNoC4x4o restore 1 1 3GHz 3000MHz X2 ${NUMINST100M} ${M5OUTBASE}
    cd result_scripts
    ./get_stats_AE.sh 100M_spec17_AE ${STATSNPLOTS}
    gnuplot -e "RESULTS_DIR='${STATSNPLOTS}'" spec17_100M_slowdown.gp
    gnuplot -e "RESULTS_DIR='${STATSNPLOTS}'" spec17_100M_slowdown_opp.gp
    cd ..
elif [ "$EXTRA_OPT" == "ErrInj" ]; then
    # SPEC17 error injection runs
    # NUMINST10M=10000000
    # TOTALINJS=${M5OUTBASE}/m5out_${NUMINST10M}/injected
    # DETECTINJS=${M5OUTBASE}/m5out_${NUMINST10M}/detected
    # MASKEDINJS=${M5OUTBASE}/m5out_${NUMINST10M}/masked
    # for i in $(seq 1 $(grep "^[1-9].*:" FUdest_err.txt | wc -l))
    # do
    #     ./run_spec17_params_classicMem.sh checkedNoC4x4oErr restore 1 1 3GHz 2000MHz A510 ${NUMINST10M} ${M5OUTBASE} ${i}
    # done
    # grep "total_injections\: [^0]" ${M5OUTBASE}/m5out_${NUMINST10M}/*/error_log.txt > ${TOTALINJS}
    # grep "Detect" ${M5OUTBASE}/m5out_${NUMINST10M}/*/simout > ${DETECTINJS}
    # grep "missed_errors\: [^0]" ${M5OUTBASE}/m5out_${NUMINST10M}/*/error_log.txt > ${MASKEDINJS}
    # python result_scripts/get_effective_errInj.py --detect_file ${DETECTINJS} --inject_file ${TOTALINJS} --masked_file ${MASKEDINJS} > detected_err.txt
    for i in $(seq 1 $(grep "^[1-9].*:" detected_err.txt | wc -l))
    do
        ./run_spec17_params_classicMem.sh opportunisticNoC4x4oErr restore 1 2 3GHz 2000MHz A510 ${NUMINST100M} ${M5OUTBASE} ${i}
        ./run_spec17_params_classicMem.sh opportunisticNoC4x4oErr restore 1 1 3GHz 2000MHz A510 ${NUMINST100M} ${M5OUTBASE} ${i}
        ./run_spec17_params_classicMem.sh opportunisticNoC4x4oErr restore 1 1 3GHz 1000MHz A510 ${NUMINST100M} ${M5OUTBASE} ${i}
        ./run_spec17_params_classicMem.sh opportunisticNoC4x4oErr restore 1 1 3GHz 500MHz A510 ${NUMINST100M} ${M5OUTBASE} ${i}
    done
    cd result_scripts
    ./errInj_count.sh ../detected_err.txt ${M5OUTBASE}/m5out_${NUMINST100M} > ${STATSNPLOTS}/spec17_errCount.csv
    python calculate.py --stat "ErrRate" --op "normalize" --csv "${STATSNPLOTS}/spec17_errCount.csv" > ${STATSNPLOTS}/spec17_errRate.csv
    python csv2gnuplot_data.py --filename ${STATSNPLOTS}/spec17_errRate.csv --oppMode Without --noBase > ${STATSNPLOTS}/spec17_errRate.data
    gnuplot -e "RESULTS_DIR='${STATSNPLOTS}'" spec17_errRate.gp
    cd ..
elif [ "$EXTRA_OPT" == "slowNoC" ]; then
    # slowNoC SPEC17 100M runs
    ./run_spec17_params_classicMem.sh checked restore 1 4 3GHz 2000MHz A510 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checked restore 1 2 3GHz 1500MHz X2 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checked restore 1 1 3GHz 3000MHz X2 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedSlowNoC4x4o restore 1 4 3GHz 2000MHz A510 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedSlowNoC4x4o restore 1 2 3GHz 1500MHz X2 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedSlowNoC4x4o restore 1 1 3GHz 3000MHz X2 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedHashSlowNoC4x4o restore 1 4 3GHz 2000MHz A510 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedHashSlowNoC4x4o restore 1 2 3GHz 1500MHz X2 ${NUMINST100M} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedHashSlowNoC4x4o restore 1 1 3GHz 3000MHz X2 ${NUMINST100M} ${M5OUTBASE}
    cd result_scripts
    ./get_stats_AE.sh 100MslowNoC_spec17_AE ${STATSNPLOTS}
    gnuplot -e "RESULTS_DIR='${STATSNPLOTS}'" spec17_100M_slowdown_slowNoC.gp
    cd ..
elif [ "$EXTRA_OPT" == "1B" ]; then
    NUMINST1B=1000000000
    # Baseline runs
    ./run_spec17_params_classicMem.sh baseline restore 1 3GHz ${NUMINST1B} ${M5OUTBASE}
    # Get NoC induced latency from baseline runs
    cd result_scripts
    ./get_NoCLat.sh spec17 ../../simresults/${NUMINST1B} ${STATSNPLOTS} ../../spec_confs
    cd ..
    # SPEC17 1B runs
    ./run_spec17_params_classicMem.sh checked restore 1 12 3GHz 1000MHz DSN18A55 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checked restore 1 16 3GHz 1000MHz ParadoxA55 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 4 3GHz 2000MHz A510 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 4 3GHz 1800MHz A510 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 4 3GHz 1600MHz A510 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 4 3GHz 1400MHz A510 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 2 3GHz 1500MHz X2 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh checkedNoC4x4o restore 1 1 3GHz 3000MHz X2 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh opportunisticNoC4x4o restore 1 4 3GHz 2000MHz A510 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh opportunisticNoC4x4o restore 1 4 3GHz 1800MHz A510 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh opportunisticNoC4x4o restore 1 4 3GHz 1600MHz A510 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh opportunisticNoC4x4o restore 1 2 3GHz 1500MHz X2 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh opportunisticNoC4x4o restore 1 2 3GHz 1350MHz X2 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh opportunisticNoC4x4o restore 1 1 3GHz 3000MHz X2 ${NUMINST1B} ${M5OUTBASE}
    ./run_spec17_params_classicMem.sh opportunisticNoC4x4o restore 1 1 3GHz 2700MHz X2 ${NUMINST1B} ${M5OUTBASE}
    cd result_scripts
    ./get_stats_AE.sh 1B_spec17 ${STATSNPLOTS}
    gnuplot -e "RESULTS_DIR='${STATSNPLOTS}'" spec17_1B_slowdown.gp
    gnuplot -e "RESULTS_DIR='${STATSNPLOTS}'" spec17_1B_slowdown_opp_errBar.gp
    cd ..
fi