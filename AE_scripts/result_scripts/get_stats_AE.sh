# Check args first
if [ "$#" -ne 2 ]; then
    echo "Wrong number of input arguments, should have 2"
    echo "./script config output_dir"
    echo "Valid config options are: 1B_spec17, 1Bstored_spec17, 1Bbreakdown_spec17, 100M_spec17, 100MstaticCov_spec17, 1B_gapbs, tillEnd_parsec"
    exit 1
fi
CONFIG=$1
RESULTS_DIR=$2
mkdir -p $RESULTS_DIR
if [ "$CONFIG" == "1B_spec17" ]; then # 1B spec17 results
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/1000000000
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_1B
    # 1B spec17 performance
    grep "numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} > ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "Normalized numCycles" --op "normalize" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_norm_numCycles.csv && \
    python calculate.py --stat "numCycles" --op "average" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_average.csv && \
    mv ${RESULTS_PREFIX}_numCycles_average.csv ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv "${RESULTS_PREFIX}_norm_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_geomean.csv && \
    mv ${RESULTS_PREFIX}_numCycles_geomean.csv ${RESULTS_PREFIX}_norm_numCycles.csv 
    # 1B spec17 coverage
    grep "uncheckedCommittedInsts" $SIMRESULT_DIR/* | python gather_AE.py --stat "uncheckedCommittedInsts" --config ${CONFIG} > ${RESULTS_PREFIX}_uncheckedCommittedInsts.csv && \
    grep "committedInsts" $SIMRESULT_DIR/* | python gather_AE.py --stat "committedInsts" --config ${CONFIG} > ${RESULTS_PREFIX}_committedInsts.csv && \
    python calculate.py --stat "checkedCommittedInsts" --op "sub" --csv "${RESULTS_PREFIX}_committedInsts.csv,${RESULTS_PREFIX}_uncheckedCommittedInsts.csv" > ${RESULTS_PREFIX}_checkedCommittedInsts.csv && \
    python calculate.py --stat "Ratio of checked committed instructions" --op "div" --csv "${RESULTS_PREFIX}_checkedCommittedInsts.csv,${RESULTS_PREFIX}_committedInsts.csv" > ${RESULTS_PREFIX}_checkedCommittedInstsRatio.csv && \
    python calculate.py --stat "Ratio of checked committed instructions" --op "geomean" --csv "${RESULTS_PREFIX}_checkedCommittedInstsRatio.csv" > ${RESULTS_PREFIX}_checkedCommittedInstsRatio_geomean.csv && \
    mv ${RESULTS_PREFIX}_checkedCommittedInstsRatio_geomean.csv ${RESULTS_PREFIX}_checkedCommittedInstsRatio.csv
    # 1B spec17 opportunistic performance with error bar
    grep "numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} > ${RESULTS_PREFIX}_norm_numCycles_opp.csv && \
    NON_OPP_CONFIGS=$(grep -v "_opp" ${RESULTS_PREFIX}_norm_numCycles_opp.csv | awk -F',' '{print $1}' | xargs | awk '{$1=""; print}' | xargs | awk -v OFS=',' '$1=$1') && \
    python calculate.py --stat "slowdown" --op "normalize" --csv "${RESULTS_PREFIX}_norm_numCycles_opp.csv" --exclude ${NON_OPP_CONFIGS} > ${RESULTS_PREFIX}_norm_numCycles_opp_tmp.csv && \
    mv ${RESULTS_PREFIX}_norm_numCycles_opp_tmp.csv ${RESULTS_PREFIX}_norm_numCycles_opp.csv && \
    X2_CONFIGS=$(grep "X2" ${RESULTS_PREFIX}_norm_numCycles_opp.csv | awk -F',' '{print $1}' | xargs | awk -v OFS="," '$1=$1') && \
    A510_CONFIGS=$(grep "A510" ${RESULTS_PREFIX}_norm_numCycles_opp.csv | awk -F',' '{print $1}' | xargs | awk -v OFS="," '$1=$1') && \
    python calculate.py --stat "slowdown" --op "crossGeomean" --csv "${RESULTS_PREFIX}_norm_numCycles_opp.csv" --include "X2_,${X2_CONFIGS}" > ${RESULTS_PREFIX}_norm_numCycles_errBar.csv && \
    python calculate.py --stat "slowdown" --op "low" --csv "${RESULTS_PREFIX}_norm_numCycles_errBar.csv" --include "X2_,${X2_CONFIGS}" > ${RESULTS_PREFIX}_norm_numCycles_errBar_tmp.csv && \
    mv ${RESULTS_PREFIX}_norm_numCycles_errBar_tmp.csv ${RESULTS_PREFIX}_norm_numCycles_errBar.csv && \
    python calculate.py --stat "slowdown" --op "high" --csv "${RESULTS_PREFIX}_norm_numCycles_errBar.csv" --include "X2_,${X2_CONFIGS}" --exclude "${X2_CONFIGS}" > ${RESULTS_PREFIX}_norm_numCycles_errBar_tmp.csv && \
    mv ${RESULTS_PREFIX}_norm_numCycles_errBar_tmp.csv ${RESULTS_PREFIX}_norm_numCycles_errBar.csv && \
    python calculate.py --stat "slowdown" --op "crossGeomean" --csv "${RESULTS_PREFIX}_norm_numCycles_errBar.csv" --include "A510_,${A510_CONFIGS}" > ${RESULTS_PREFIX}_norm_numCycles_errBar_tmp.csv && \
    mv ${RESULTS_PREFIX}_norm_numCycles_errBar_tmp.csv ${RESULTS_PREFIX}_norm_numCycles_errBar.csv && \
    python calculate.py --stat "slowdown" --op "low" --csv "${RESULTS_PREFIX}_norm_numCycles_errBar.csv" --include "A510_,${A510_CONFIGS}" > ${RESULTS_PREFIX}_norm_numCycles_errBar_tmp.csv && \
    mv ${RESULTS_PREFIX}_norm_numCycles_errBar_tmp.csv ${RESULTS_PREFIX}_norm_numCycles_errBar.csv && \
    python calculate.py --stat "slowdown" --op "high" --csv "${RESULTS_PREFIX}_norm_numCycles_errBar.csv" --include "A510_,${A510_CONFIGS}" --exclude "${A510_CONFIGS}" > ${RESULTS_PREFIX}_norm_numCycles_errBar_tmp.csv && \
    mv ${RESULTS_PREFIX}_norm_numCycles_errBar_tmp.csv ${RESULTS_PREFIX}_norm_numCycles_errBar.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv "${RESULTS_PREFIX}_norm_numCycles_errBar.csv" > ${RESULTS_PREFIX}_norm_numCycles_errBar_geomean.csv && \
    mv ${RESULTS_PREFIX}_norm_numCycles_errBar_geomean.csv ${RESULTS_PREFIX}_norm_numCycles_errBar.csv 
    # 1B spec17 gnuplot data output
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase > ${RESULTS_PREFIX}_slowdown.data
    # python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase --filterByCore X2 > ${RESULTS_PREFIX}_X2_slowdown.data
    # python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase --filterByCore A510 > ${RESULTS_PREFIX}_A510_slowdown.data
    # python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode With --noBase > ${RESULTS_PREFIX}_slowdown_opp.data
    # python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode With --noBase --filterByCore X2 > ${RESULTS_PREFIX}_X2_slowdown_opp.data
    # python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode With --noBase --filterByCore A510 > ${RESULTS_PREFIX}_A510_slowdown_opp.data
    # python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_checkedCommittedInstsRatio.csv --oppMode With --noBase > ${RESULTS_PREFIX}_coverage.data
    # python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_checkedCommittedInstsRatio.csv --oppMode With --noBase --filterByCore X2 > ${RESULTS_PREFIX}_X2_coverage.data
    # python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_checkedCommittedInstsRatio.csv --oppMode With --noBase --filterByCore A510 > ${RESULTS_PREFIX}_A510_coverage.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles_errBar.csv --oppMode Both --noBase > ${RESULTS_PREFIX}_slowdown_opp_errBar.data
elif [ "$CONFIG" == "1Bstored_spec17" ]; then # 1B spec17 stored not checked results
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/1000000000
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_1Bstored
    grep "numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} > ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "Normalized numCycles" --op "normalize" --csv ${RESULTS_PREFIX}_numCycles.csv > ${RESULTS_PREFIX}_norm_numCycles.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv ${RESULTS_PREFIX}_norm_numCycles.csv > ${RESULTS_PREFIX}_numCycles_geomean.csv && \
    mv ${RESULTS_PREFIX}_numCycles_geomean.csv ${RESULTS_PREFIX}_norm_numCycles.csv
    # 1B spec17 stored gnuplot data output
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without > ${RESULTS_PREFIX}_slowdown.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase --filterByCore X2 > ${RESULTS_PREFIX}_X2_slowdown.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase --filterByCore A510 > ${RESULTS_PREFIX}_A510_slowdown.data
elif [ "$CONFIG" == "1Bbreakdown_spec17" ]; then # 1B spec17 overhead breakdown (no NoC extra latency) results
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/1000000000
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_1Bbreakdown
    grep "numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} > ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "Normalized numCycles" --op "normalize" --csv ${RESULTS_PREFIX}_numCycles.csv > ${RESULTS_PREFIX}_norm_numCycles.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv ${RESULTS_PREFIX}_norm_numCycles.csv > ${RESULTS_PREFIX}_numCycles_geomean.csv && \
    mv ${RESULTS_PREFIX}_numCycles_geomean.csv ${RESULTS_PREFIX}_norm_numCycles.csv
    # 1B spec17 stored gnuplot data output
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without > ${RESULTS_PREFIX}_slowdown.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase --filterByCore X2 > ${RESULTS_PREFIX}_X2_slowdown.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase --filterByCore A510 > ${RESULTS_PREFIX}_A510_slowdown.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode With --noBase > ${RESULTS_PREFIX}_slowdown_opp.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode With --noBase --filterByCore X2 > ${RESULTS_PREFIX}_X2_slowdown_opp.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode With --noBase --filterByCore A510 > ${RESULTS_PREFIX}_A510_slowdown_opp.data
elif [ "$CONFIG" == "1Bmcpat_spec17" ]; then # 1B mcpat power and energy estimation
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/1000000000
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_1Bmcpat
    grep "numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} > ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "Normalized numCycles" --op "normalize" --csv ${RESULTS_PREFIX}_numCycles.csv > ${RESULTS_PREFIX}_norm_numCycles.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv ${RESULTS_PREFIX}_norm_numCycles.csv > ${RESULTS_PREFIX}_numCycles_geomean.csv && \
    mv ${RESULTS_PREFIX}_numCycles_geomean.csv ${RESULTS_PREFIX}_norm_numCycles.csv
elif [ "$CONFIG" == "1B_gapbs" ]; then # 1B gapbs results
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/1000000000
    RESULTS_PREFIX=${RESULTS_DIR}/gapbs_1B
    # 1B gapbs performance
    grep "numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} > ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "Normalized numCycles" --op "normalize" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_norm_numCycles.csv && \
    python calculate.py --stat "numCycles" --op "average" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_average.csv && \
    mv ${RESULTS_PREFIX}_numCycles_average.csv ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv "${RESULTS_PREFIX}_norm_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_geomean.csv && \
    mv ${RESULTS_PREFIX}_numCycles_geomean.csv ${RESULTS_PREFIX}_norm_numCycles.csv 
    # 1B gapbs gnuplot data output
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase > ${RESULTS_PREFIX}_slowdown.data
elif [ "$CONFIG" == "100M_spec17" ]; then # 100M spec17 results
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/100000000
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_100M
    # 100M spec17 performance
    grep "numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} > ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "Normalized numCycles" --op "normalize" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_norm_numCycles.csv && \
    python calculate.py --stat "numCycles" --op "average" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_average.csv && \
    mv ${RESULTS_PREFIX}_numCycles_average.csv ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv "${RESULTS_PREFIX}_norm_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_geomean.csv && \
    mv ${RESULTS_PREFIX}_numCycles_geomean.csv ${RESULTS_PREFIX}_norm_numCycles.csv
    # 100M spec17 coverage
    grep "uncheckedCommittedInsts" $SIMRESULT_DIR/* | python gather_AE.py --stat "uncheckedCommittedInsts" --config ${CONFIG} > ${RESULTS_PREFIX}_uncheckedCommittedInsts.csv && \
    grep "committedInsts" $SIMRESULT_DIR/* | python gather_AE.py --stat "committedInsts" --config ${CONFIG} > ${RESULTS_PREFIX}_committedInsts.csv && \
    python calculate.py --stat "checkedCommittedInsts" --op "sub" --csv "${RESULTS_PREFIX}_committedInsts.csv,${RESULTS_PREFIX}_uncheckedCommittedInsts.csv" > ${RESULTS_PREFIX}_checkedCommittedInsts.csv && \
    python calculate.py --stat "Ratio of checked committed instructions" --op "div" --csv "${RESULTS_PREFIX}_checkedCommittedInsts.csv,${RESULTS_PREFIX}_committedInsts.csv" > ${RESULTS_PREFIX}_checkedCommittedInstsRatio.csv && \
    python calculate.py --stat "Ratio of checked committed instructions" --op "geomean" --csv "${RESULTS_PREFIX}_checkedCommittedInstsRatio.csv" > ${RESULTS_PREFIX}_checkedCommittedInstsRatio_geomean.csv && \
    mv ${RESULTS_PREFIX}_checkedCommittedInstsRatio_geomean.csv ${RESULTS_PREFIX}_checkedCommittedInstsRatio.csv
    # 100M gnuplot data output
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noTrans --noBase --groupByConfig > ${RESULTS_PREFIX}_slowdown.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode With --noTrans --noBase --groupByConfig > ${RESULTS_PREFIX}_slowdown_opp.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_checkedCommittedInstsRatio.csv --oppMode With --noTrans --noBase --groupByConfig > ${RESULTS_PREFIX}_coverage.data
elif [ "$CONFIG" == "100M_spec17_AE" ]; then # 100M spec17 results
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/100000000
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_100M
    for bench in bwaves roms
    do 
        cp $SIMRESULT_DIR/${bench}_checkedNoC4x4o_1_4_3GHz_2000MHz_A510_stats.txt $SIMRESULT_DIR/${bench}_checkedNoC4x4o_1_4_3GHz_minED2P_A510_stats.txt
    done
    for bench in gcc wrf cam4
    do
        cp $SIMRESULT_DIR/${bench}_checkedNoC4x4o_1_4_3GHz_1800MHz_A510_stats.txt $SIMRESULT_DIR/${bench}_checkedNoC4x4o_1_4_3GHz_minED2P_A510_stats.txt
    done
    for bench in fotonik3d cactuBSSN 
    do
        cp $SIMRESULT_DIR/${bench}_checkedNoC4x4o_1_4_3GHz_1600MHz_A510_stats.txt $SIMRESULT_DIR/${bench}_checkedNoC4x4o_1_4_3GHz_minED2P_A510_stats.txt
    done 
    for bench in mcf deepsjeng leela exchange2 xz pop2 imagick nab perlbench x264 xalancbmk omnetpp lbm
    do
        cp $SIMRESULT_DIR/${bench}_checkedNoC4x4o_1_4_3GHz_1400MHz_A510_stats.txt $SIMRESULT_DIR/${bench}_checkedNoC4x4o_1_4_3GHz_minED2P_A510_stats.txt
    done
    # 100M spec17 performance
    grep "numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} > ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "Normalized numCycles" --op "normalize" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_norm_numCycles.csv && \
    python calculate.py --stat "numCycles" --op "average" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_average.csv && \
    mv ${RESULTS_PREFIX}_numCycles_average.csv ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv "${RESULTS_PREFIX}_norm_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_geomean.csv && \
    mv ${RESULTS_PREFIX}_numCycles_geomean.csv ${RESULTS_PREFIX}_norm_numCycles.csv
    # 100M gnuplot data output
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase > ${RESULTS_PREFIX}_slowdown.data
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode With --noBase > ${RESULTS_PREFIX}_slowdown_opp.data
elif [ "$CONFIG" == "100MslowNoC_spec17_AE" ]; then # 100M spec17 results
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/100000000
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_100MslowNoC
    # 100M spec17 performance
    grep "numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} --slowNoCEstimate "slowNoC_estimates.csv" > ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "Normalized numCycles" --op "normalize" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_norm_numCycles.csv && \
    python calculate.py --stat "numCycles" --op "average" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_average.csv && \
    mv ${RESULTS_PREFIX}_numCycles_average.csv ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv "${RESULTS_PREFIX}_norm_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_geomean.csv && \
    mv ${RESULTS_PREFIX}_numCycles_geomean.csv ${RESULTS_PREFIX}_norm_numCycles.csv
    # 100M gnuplot data output
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase > ${RESULTS_PREFIX}_slowdown.data
    cp ${RESULTS_PREFIX}_slowdown.data ${RESULTS_DIR}/spec17_100M_slowdown_slowNoC.data
elif [ "$CONFIG" == "100MslowNoC_spec17" ]; then # 100M spec17 results
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/100000000
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_100MslowNoC
    # 100M spec17 performance
    grep "numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} --slowNoCEstimate "slowNoC_estimates.csv" > ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "Normalized numCycles" --op "normalize" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_norm_numCycles.csv && \
    python calculate.py --stat "numCycles" --op "average" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_average.csv && \
    mv ${RESULTS_PREFIX}_numCycles_average.csv ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv "${RESULTS_PREFIX}_norm_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_geomean.csv && \
    mv ${RESULTS_PREFIX}_numCycles_geomean.csv ${RESULTS_PREFIX}_norm_numCycles.csv
    # 100M gnuplot data output
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase > ${RESULTS_PREFIX}_slowdown.data
elif [ "$CONFIG" == "100MstaticCov_spec17" ]; then # 100M spec17 static coverage
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/100000000
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_100M
    grep "committedPCs" ${SIMRESULT_DIR}/* | python gather_AE.py --stat "committedPCs" --config ${CONFIG} > ${RESULTS_PREFIX}_committedPCs.csv && \
    grep "uncheckedCommittedPCs" ${SIMRESULT_DIR}/* | python gather_AE.py --stat "uncheckedCommittedPCs" --config ${CONFIG} > ${RESULTS_PREFIX}_uncheckedCommittedPCs.csv && \
    python calculate.py --stat "checked committed PCs" --op "sub" --csv "${RESULTS_PREFIX}_committedPCs.csv,${RESULTS_PREFIX}_uncheckedCommittedPCs.csv" > ${RESULTS_PREFIX}_checkedCommittedPCs.csv && \
    python calculate.py --stat "Ratio of checked committed PCs" --op "div" --csv "${RESULTS_PREFIX}_checkedCommittedPCs.csv,${RESULTS_PREFIX}_committedPCs.csv" > ${RESULTS_PREFIX}_checkedCommittedPCsRatio.csv && \
    python calculate.py --stat "Ratio of checked committed static instructions" --op "geomean" --csv "${RESULTS_PREFIX}_checkedCommittedPCsRatio.csv" > ${RESULTS_PREFIX}_checkedCommittedPCsRatio_geomean.csv && \
    mv ${RESULTS_PREFIX}_checkedCommittedPCsRatio_geomean.csv ${RESULTS_PREFIX}_checkedCommittedPCsRatio.csv
    # 100M spec17 static coverage gnuplot data output
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_checkedCommittedPCsRatio.csv --oppMode With --noTrans --noBase --groupByConfig > ${RESULTS_PREFIX}_staticCov.data
elif [ "$CONFIG" == "tillEnd_parsec" ]; then # parsec results
    echo $CONFIG
    SIMRESULT_DIR=../../simresults/0
    RESULTS_PREFIX=${RESULTS_DIR}/parsec
    # parsec performance
    for i in 0 1 2 3 4
    do
        grep "cpu${i}.numCycles" $SIMRESULT_DIR/* | python gather_AE.py --stat "numCycles" --config ${CONFIG} --all --dummy 0 > ${RESULTS_PREFIX}_numCycles_c${i}.csv
    done
    python calculate.py --stat "numCycles" --op "max" --csv "${RESULTS_PREFIX}_numCycles_c0.csv,${RESULTS_PREFIX}_numCycles_c1.csv,${RESULTS_PREFIX}_numCycles_c2.csv,${RESULTS_PREFIX}_numCycles_c3.csv,${RESULTS_PREFIX}_numCycles_c4.csv" > ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "Normalized numCycles" --op "normalize" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_norm_numCycles.csv && \
    python calculate.py --stat "numCycles" --op "average" --csv "${RESULTS_PREFIX}_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_average.csv && \
    mv ${RESULTS_PREFIX}_numCycles_average.csv ${RESULTS_PREFIX}_numCycles.csv && \
    python calculate.py --stat "slowdown" --op "geomean" --csv "${RESULTS_PREFIX}_norm_numCycles.csv" > ${RESULTS_PREFIX}_numCycles_geomean.csv && \
    mv ${RESULTS_PREFIX}_numCycles_geomean.csv ${RESULTS_PREFIX}_norm_numCycles.csv 
    # parsec gnuplot data output
    python csv2gnuplot_data.py --filename ${RESULTS_PREFIX}_norm_numCycles.csv --oppMode Without --noBase > ${RESULTS_PREFIX}_slowdown.data
else
    echo "Invalid config input argument"
    echo "Valid config options are: 1B_spec17, 1Bstored_spec17, 1Bbreakdown_spec17, 100M_spec17, 100MstaticCov_spec17, 1B_gapbs, tillEnd_parsec"
    exit 1
fi

