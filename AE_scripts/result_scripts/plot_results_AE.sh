# Check args first
if [ "$#" -ne 2 ]; then
    echo "Wrong number of input arguments, should have 3"
    echo "./script config input_dir"
    echo "Valid config options are: 1B_spec17, 100M_spec17, gapbs, parsec"
    exit 1
fi
CONFIG=$1
RESULTS_DIR=$2
if [ "$CONFIG" == "1B_spec17" ]; then # SPEC17 1B plots
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_1B
    python gen_gnuplot.py --filename ${RESULTS_PREFIX}_A510_slowdown.data --stat Slowdown --plottype Bar > ${RESULTS_PREFIX}_A510_slowdown.gp && \
    gnuplot ${RESULTS_PREFIX}_A510_slowdown.gp
    python gen_gnuplot.py --filename ${RESULTS_PREFIX}_X2_slowdown.data --stat Slowdown --plottype Bar > ${RESULTS_PREFIX}_X2_slowdown.gp && \
    gnuplot ${RESULTS_PREFIX}_X2_slowdown.gp
    python gen_gnuplot.py --filename ${RESULTS_PREFIX}_A510_slowdown_opp.data --stat Slowdown --plottype Bar > ${RESULTS_PREFIX}_A510_slowdown_opp.gp && \
    gnuplot ${RESULTS_PREFIX}_A510_slowdown_opp.gp
    python gen_gnuplot.py --filename ${RESULTS_PREFIX}_X2_slowdown_opp.data --stat Slowdown --plottype Bar > ${RESULTS_PREFIX}_X2_slowdown_opp.gp && \
    gnuplot ${RESULTS_PREFIX}_X2_slowdown_opp.gp
    python gen_gnuplot.py --filename ${RESULTS_PREFIX}_A510_coverage.data --stat "Dynamic Instruction Coverage" --plottype Bar > ${RESULTS_PREFIX}_A510_coverage.gp && \
    gnuplot ${RESULTS_PREFIX}_A510_coverage.gp
    python gen_gnuplot.py --filename ${RESULTS_PREFIX}_X2_coverage.data --stat "Dynamic Instruction Coverage" --plottype Bar > ${RESULTS_PREFIX}_X2_coverage.gp && \
    gnuplot ${RESULTS_PREFIX}_X2_coverage.gp
    gnuplot -e "RESULTS_DIR='${RESULTS_DIR}'" spec17_1B_X2_slowdown_breakdown.gp
    gnuplot -e "RESULTS_DIR='${RESULTS_DIR}'" spec17_1B_A510_slowdown_breakdown.gp
    gnuplot -e "RESULTS_DIR='${RESULTS_DIR}'" spec17_1B_slowdown_opp_errBar.gp
elif [ "$CONFIG" == "gapbs" ]; then # GAPBS 1B plots
    RESULTS_PREFIX=${RESULTS_DIR}/gapbs_1B
    python gen_gnuplot.py --filename ${RESULTS_PREFIX}_slowdown.data --stat Slowdown --plottype Bar > ${RESULTS_PREFIX}_slowdown.gp && \
    gnuplot ${RESULTS_PREFIX}_slowdown.gp
elif [ "$CONFIG" == "100M_spec17" ]; then # SPEC17 100M results
    RESULTS_PREFIX=${RESULTS_DIR}/spec17_100M
    python gen_gnuplot.py --filename ${RESULTS_PREFIX}_slowdown.data --stat Slowdown --plottype Line > ${RESULTS_PREFIX}_slowdown.gp && \
    gnuplot ${RESULTS_PREFIX}_slowdown.gp
    python gen_gnuplot.py --filename ${RESULTS_PREFIX}_coverage.data --stat "Dynamic Instruction Coverage" --plottype Line > ${RESULTS_PREFIX}_coverage.gp && \
    gnuplot ${RESULTS_PREFIX}_coverage.gp
    gnuplot -e "RESULTS_DIR='${RESULTS_DIR}'" spec17_100M_slowdown_slowNoC.gp
elif [ "$CONFIG" == "parsec" ]; then # PARSEC results
    RESULTS_PREFIX=${RESULTS_DIR}/parsec
    python gen_gnuplot.py --filename ${RESULTS_PREFIX}_slowdown.data --stat Slowdown --plottype Bar > ${RESULTS_PREFIX}_slowdown.gp && \
    gnuplot ${RESULTS_PREFIX}_slowdown.gp
else
    echo "Invalid config input argument"
    echo "Valid config options are: 1B_spec17, 100M_spec17, gapbs, parsec"
    exit 1
fi