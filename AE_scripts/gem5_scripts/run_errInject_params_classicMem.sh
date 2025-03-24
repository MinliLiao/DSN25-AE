# Check args first
if [ "$#" -ne 19 ]; then
    echo "Final script, incorrect arguments"
    exit 1
fi

ENVFILE=$1
BINA=$2
ARGS=$3
IN=$4
BENCH=$5
CPTRESTORE=$6
NUM_O3=$7
NUM_CHECKERS=$8
FREQ_O3=$9
FREQ_CHECKERS=${10}
CHECKER_TYPE=${11}
MAXINSTS=${12}
M5OUTBASE=${13}
ERRTYPE=${14}
ERRPLACE=${15}
ERRBIT=${16}
ERRVAL=${17}
NOCLAT=${18}
EXPTYPE=${19}

OUTPUT=${BASE}/simresults/${MAXINSTS}
OUTPUTM5OUT=${M5OUTBASE}/m5out_${MAXINSTS}
OUTPREFIX=${BENCH}_${EXPTYPE}${ERRTYPE}_o${ERRPLACE}b${ERRBIT}s${ERRVAL}_${NUM_O3}_${NUM_CHECKERS}_${FREQ_O3}_${FREQ_CHECKERS}_${CHECKER_TYPE}

# Create results folder if it does not exist
mkdir -p $OUTPUT
mkdir -p $OUTPUTM5OUT

CPTRESTOREARGS=()
if [ "$CPTRESTORE" == "restore" ]; then
    CPTRESTOREARGS=(-r 1 --checkpoint-dir=${M5OUTBASE}/m5out_cpt/cpt_paramedic_${BENCH}_${NUM_O3} --restore-with-cpu=TimingSimpleCPU)
elif [ "$CPTRESTORE" == "restore_atomic" ]; then
    CPTRESTOREARGS=(-r 1 --checkpoint-dir=${M5OUTBASE}/m5out_cpt/cpt_${BENCH}_${NUM_O3} --restore-with-cpu=AtomicSimpleCPU)
elif [ "$CPTRESTORE" != "" ]; then
    echo "Invalid checkpoint type option $CPTRESTORE"
    echo "Valid checkpoint type options are restore or \"\""
    exit 1
fi
MAXINSTSOPTION=()
if [ "$MAXINSTS" != "0" ]; then
    MAXINSTSOPTION=(--maxinsts=${MAXINSTS})
fi
CHECKOPTION=(--stored --sleepguard) # stored-only mode, to estimate overhead
if [ "$EXPTYPE" == "checked" ] || [ "$EXPTYPE" == "checkedNoC" ] || [ "$EXPTYPE" == "checkedNoCErr" ] || [ "$EXPTYPE" == "checkedNoC4x4oErr" ]; then # Full-coverage mode
    CHECKOPTION+=(--checked)
elif [ "$EXPTYPE" == "blocking" ] || [ "$EXPTYPE" == "blockingNoC" ]; then # Full-coverage mode
    CHECKOPTION+=(--checked)
    CHECKOPTION+=(--blocking)
elif [ "$EXPTYPE" == "opportunistic" ] || [ "$EXPTYPE" == "opportunisticNoC" ] || [ "$EXPTYPE" == "opportunisticNoCErr" ] || [ "$EXPTYPE" == "opportunisticNoC4x4oErr" ]; then # Opportunistic mode
    CHECKOPTION+=(--checked)
    CHECKOPTION+=(--opportunistic)
elif [ "$EXPTYPE" != "stored" ]; then 
    echo "Invalid check type option $EXPTYPE"
    echo "Valid check types are checked, checkedNoC, checkedNoCErr, blocking, opportunistic, opportunisticNoC, opportunisticNoCErr, stored"
    exit 1
fi
if [ "$EXPTYPE" == "checkedNoCErr" ] || [ "$EXPTYPE" == "checkedNoC4x4oErr" ] || [ "$EXPTYPE" == "opportunisticNoCErr" ] || [ "$EXPTYPE" == "opportunisticNoC4x4oErr" ]; then # Error injection
    CHECKOPTION+=(--hardErrorCore=1 --hardErrorInjectionType=${ERRTYPE} --hardErrorInjectionPoint=${ERRPLACE} --hardErrorBit=${ERRBIT} --hardErrorStuckAt=${ERRVAL} --exit_on_error)
fi

$BASE/build/ARM/gem5.opt \
    --outdir ${OUTPUTM5OUT}/${OUTPREFIX} \
    --redirect-stdout --redirect-stderr \
    $BASE/configs/example/se.py \
    --output=app.out --errout=app.err \
    -n "$((NUM_O3 + NUM_O3 * NUM_CHECKERS))"  --num-main-cores="$NUM_O3" \
    -e "$ENVFILE" -c "$BINA" -o="$ARGS" -i "$IN" \
    --cpu-type=X2 --cpu2-type="$CHECKER_TYPE" \
    --cpu-clock "$FREQ_O3" --cpu2-clock "$FREQ_CHECKERS" \
    --pl2sl3cache --l3_extraNoCLat="$NOCLAT" \
    "${MAXINSTSOPTION[@]}" --mem-size=16GB \
    "${CPTRESTOREARGS[@]}" \
    "${CHECKOPTION[@]}" && \
cp ${OUTPUTM5OUT}/${OUTPREFIX}/stats.txt $OUTPUT/${OUTPREFIX}_stats.txt
