BASE=$(pwd)/../../
DIRPREFIX=$(pwd)/
INSTATDIR=${BASE}/simresults/1000000000
OUTXMLDIR=${DIRPREFIX}/mcpat_xml
OUTSTATDIR=${BASE}/simresults/mcpat_stat
MCPATDIR=${BASE}/mcpat/
BENCHMARKS=(bwaves gcc mcf xalancbmk deepsjeng leela exchange2 xz cactuBSSN lbm wrf cam4 pop2 imagick nab fotonik3d roms x264 perlbench omnetpp)
CONFIGS=(baseline_1_3GHz checkedNoC_1_1_3GHz_3000MHz_X2 checkedNoC_1_1_3GHz_2700MHz_X2 checkedNoC_1_2_3GHz_1500MHz_X2 checkedNoC_1_2_3GHz_1350MHz_X2 checkedNoC_1_4_3GHz_2000MHz_A510 checkedNoC_1_4_3GHz_1800MHz_A510 checkedNoC_1_4_3GHz_1600MHz_A510 checkedNoC_1_4_3GHz_1400MHz_A510)

# Create results folder if it does not exist
mkdir -p $OUTXMLDIR
mkdir -p $OUTSTATDIR

for BENCH in ${BENCHMARKS[@]}
do
    for CONFIG in ${CONFIGS[@]}
    do
        echo "${BENCH}_${CONFIG}_stats.txt"
        NUMCORES=1
        if [ "${CONFIG}" == "checkedNoC_1_1_3GHz_3000MHz_X2" ] || [ "${CONFIG}" == "checkedNoC_1_1_3GHz_2700MHz_X2" ]; then
            NUMCORES=2
        elif [ "${CONFIG}" == "checkedNoC_1_2_3GHz_1500MHz_X2" ] || [ "${CONFIG}" == "checkedNoC_1_2_3GHz_1350MHz_X2" ]; then
            NUMCORES=3
        elif [ "${CONFIG}" == "checkedNoC_1_4_3GHz_2000MHz_A510" ] || [ "${CONFIG}" == "checkedNoC_1_4_3GHz_1800MHz_A510" ] || [ "${CONFIG}" == "checkedNoC_1_4_3GHz_1600MHz_A510" ] || [ "${CONFIG}" == "checkedNoC_1_4_3GHz_1400MHz_A510" ]; then
            NUMCORES=5
        fi
        python3 ${DIRPREFIX}/mcpat_stat.py --statFile ${INSTATDIR}/${BENCH}_${CONFIG}_stats.txt --templateFile ${DIRPREFIX}/ARM_${CONFIG}.xml --numCores ${NUMCORES} > ${OUTXMLDIR}/${BENCH}_${CONFIG}.xml && \
        ${MCPATDIR}/mcpat -infile ${OUTXMLDIR}/${BENCH}_${CONFIG}.xml -print_level 1 > ${OUTSTATDIR}/${BENCH}_${CONFIG}.txt
    done
done
# grep results
cd ${OUTSTATDIR}
echo "TotalLeakage," > leakage.csv
echo "RuntimeDynamic," > dynamic.csv
for BENCH in ${BENCHMARKS[@]}
do
    echo -n "${BENCH}" >> leakage.csv
    echo -n "${BENCH}" >> dynamic.csv
    if [ "${BENCH}" == "${BENCHMARKS[-1]}" ]; then
    	echo "" >> leakage.csv
    	echo "" >> dynamic.csv
    else
        echo -n "," >> leakage.csv
        echo -n "," >> dynamic.csv
    fi
done
for CONFIG in ${CONFIGS[@]}
do
    if [ "${CONFIG}" == "baseline_1_3GHz" ]; then
        echo -n "Baseline," >> leakage.csv
        echo -n "Baseline," >> dynamic.csv
    elif [ "${CONFIG}" == "checkedNoC_1_1_3GHz_3000MHz_X2" ]; then
        echo -n "1*X2@3GHz," >> leakage.csv
        echo -n "1*X2@3GHz," >> dynamic.csv
    elif [ "${CONFIG}" == "checkedNoC_1_1_3GHz_2700MHz_X2" ]; then
        echo -n "1*X2@2.7GHz," >> leakage.csv
        echo -n "1*X2@2.7GHz," >> dynamic.csv
    elif [ "${CONFIG}" == "checkedNoC_1_2_3GHz_1500MHz_X2" ]; then
        echo -n "2*X2@1.5GHz," >> leakage.csv
        echo -n "2*X2@1.5GHz," >> dynamic.csv
    elif [ "${CONFIG}" == "checkedNoC_1_2_3GHz_1350MHz_X2" ]; then
        echo -n "2*X2@1.35GHz," >> leakage.csv
        echo -n "2*X2@1.35GHz," >> dynamic.csv
    elif [ "${CONFIG}" == "checkedNoC_1_4_3GHz_2000MHz_A510" ]; then
        echo -n "4*A510@2GHz," >> leakage.csv
        echo -n "4*A510@2GHz," >> dynamic.csv
    elif [ "${CONFIG}" == "checkedNoC_1_4_3GHz_1800MHz_A510" ]; then
        echo -n "4*A510@1.8GHz," >> leakage.csv
        echo -n "4*A510@1.8GHz," >> dynamic.csv
    elif [ "${CONFIG}" == "checkedNoC_1_4_3GHz_1600MHz_A510" ]; then
        echo -n "4*A510@1.6GHz," >> leakage.csv
        echo -n "4*A510@1.6GHz," >> dynamic.csv
    elif [ "${CONFIG}" == "checkedNoC_1_4_3GHz_1400MHz_A510" ]; then
        echo -n "4*A510@1.4GHz," >> leakage.csv
        echo -n "4*A510@1.4GHz," >> dynamic.csv
    fi
    for BENCH in ${BENCHMARKS[@]}
    do
        grep "^  Total Leakage = " ${BENCH}_${CONFIG}.txt | awk '{printf "%s", $4}' >> leakage.csv
        grep "^  Runtime Dynamic = " ${BENCH}_${CONFIG}.txt | awk '{printf "%s", $4}' >> dynamic.csv
        if [ "${BENCH}" == "${BENCHMARKS[-1]}" ]; then
    	    echo "" >> leakage.csv
    	    echo "" >> dynamic.csv
        else
            echo -n "," >> leakage.csv
            echo -n "," >> dynamic.csv
        fi
    done
done
cd ${DIRPREFIX}