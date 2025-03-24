# Check args first
if [ "$#" -ne 15 ]; then
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
NOCLAT=${14}
EXPTYPE=${15}

OUTPUT=${BASE}/simresults/${MAXINSTS}
OUTPUTM5OUT=${M5OUTBASE}/m5out_${MAXINSTS}
OUTPREFIX=${BENCH}_${EXPTYPE}_${NUM_O3}_${NUM_CHECKERS}_${FREQ_O3}_${FREQ_CHECKERS}_${CHECKER_TYPE}

# Create results folder if it does not exist
mkdir -p $OUTPUT
mkdir -p $OUTPUTM5OUT

CPTRESTOREARGS=()
if [ "$CPTRESTORE" == "restore" ]; then
    CPTRESTOREARGS=(-r 1 --checkpoint-dir=${M5OUTBASE}/m5out_cpt/cpt_paramedic_${BENCH}_${NUM_O3} --restore-with-cpu=TimingSimpleCPU)
    if ((NUM_O3 + NUM_O3 * NUM_CHECKERS > 10)); then
        # CPU numbering is 2 digit for paradox checkpoints (cpu00, cpu01, ...)
        CPTRESTOREARGS[2]="--checkpoint-dir=${M5OUTBASE}/m5out_cpt/cpt_paradox_${BENCH}_${NUM_O3}"
    fi
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
    if [ $((NUM_O3)) > 1 ]; then
        MAXINSTSOPTION+=(--allMainMaxinsts)
    fi
fi
CHECKOPTION=(--stored --sleepguard) # stored-only mode, to estimate overhead
if [ "$EXPTYPE" == "checked" ] || [ "$EXPTYPE" == "checkedNoC" ] || [ "$EXPTYPE" == "checkedNoC4x4o" ] || [ "$EXPTYPE" == "checkedSlowNoC" ] || [ "$EXPTYPE" == "checkedSlowNoC4x4o" ] || [ "$EXPTYPE" == "checkedHashSlowNoC" ] || [ "$EXPTYPE" == "checkedHashSlowNoC4x4o" ]; then # Full-coverage mode
    CHECKOPTION+=(--checked)
elif [ "$EXPTYPE" == "opportunistic" ] || [ "$EXPTYPE" == "opportunisticNoC" ] || [ "$EXPTYPE" == "opportunisticNoC4x4o" ]; then # Opportunistic mode
    CHECKOPTION+=(--checked)
    CHECKOPTION+=(--opportunistic)
elif [ "$EXPTYPE" != "stored" ]; then 
    echo "Invalid check type option $EXPTYPE"
    echo "Valid check types are checked, checkedNoC, blocking, opportunistic, opportunisticNoC, stored"
    exit 1
fi

if [ "$CHECKER_TYPE" == "ParadoxMinorCPU" ] || [ "$CHECKER_TYPE" == "ParadoxEx5LITTLE" ] || [ "$CHECKER_TYPE" == "ParadoxA55" ] || [ "$CHECKER_TYPE" == "ParadoxA510" ]; then
    CHECKOPTION+=(--lslSize=384)
elif [ "$CHECKER_TYPE" == "DSN18MinorCPU" ] || [ "$CHECKER_TYPE" == "DSN18Ex5LITTLE" ]|| [ "$CHECKER_TYPE" == "DSN18A55" ] || [ "$CHECKER_TYPE" == "DSN18A510" ]; then
    CHECKOPTION+=(--lslSize=192)
fi

APPOUT=app.out
APPERR=app.err
NUMPROC=$(echo $BINA | grep -o ";" | wc -l)
for i in `seq 1 $NUMPROC`
do
  APPOUT="$APPOUT;app$i.out"
  APPERR="$APPERR;app$i.err"
done
MEMSIZE="$((16 + 16 * NUMPROC))GB"

$BASE/build/ARM/gem5.opt \
    --outdir ${OUTPUTM5OUT}/${OUTPREFIX} \
    --redirect-stdout --redirect-stderr \
    $BASE/configs/example/se.py \
    --output="$APPOUT" --errout="$APPERR" \
    -n "$((NUM_O3 + NUM_O3 * NUM_CHECKERS))"  --num-main-cores="$NUM_O3" \
    -e "$ENVFILE" -c "$BINA" -o="$ARGS" -i "$IN" \
    --cpu-type=X2 --cpu2-type="$CHECKER_TYPE" \
    --cpu-clock "$FREQ_O3" --cpu2-clock "$FREQ_CHECKERS" \
    --pl2sl3cache --l3_extraNoCLat="$NOCLAT" \
    "${MAXINSTSOPTION[@]}" --mem-size="$MEMSIZE" \
    "${CPTRESTOREARGS[@]}" \
    "${CHECKOPTION[@]}" && \
cp ${OUTPUTM5OUT}/${OUTPREFIX}/stats.txt $OUTPUT/${OUTPREFIX}_stats.txt
