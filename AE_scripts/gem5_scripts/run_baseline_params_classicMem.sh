# Check args first
if [ "$#" -ne 12 ]; then
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
FREQ_O3=$8
MAXINSTS=$9
M5OUTBASE=${10}
NOCLAT=${11}
EXPTYPE=${12}

OUTPUT=${BASE}/simresults/${MAXINSTS}
OUTPUTM5OUT=${M5OUTBASE}/m5out_${MAXINSTS}
OUTPREFIX=${BENCH}_${EXPTYPE}_${NUM_O3}_${FREQ_O3}

# Create results folder if it does not exist
mkdir -p $OUTPUT
mkdir -p $OUTPUTM5OUT

CPTRESTOREARGS=()
if [ "$CPTRESTORE" == "restore" ]; then
    CPTRESTOREARGS=(-r 1 --checkpoint-dir=${M5OUTBASE}/m5out_cpt/cpt_baseline_${BENCH}_${NUM_O3} --restore-with-cpu=TimingSimpleCPU)
elif [ "$CPTRESTORE" == "restore_atomic" ]; then
    CPTRESTOREARGS=(-r 1 --checkpoint-dir=${M5OUTBASE}/m5out_cpt/cpt_${BENCH}_${NUM_O3} --restore-with-cpu=AtomicSimpleCPU)
elif [ "$CPTRESTORE" != "" ]; then
    echo "Invalid checkpoint type option $CPTRESTORE"
    exit 1
fi
MAXINSTSOPTION=()
if [ "$MAXINSTS" != "0" ]; then
    MAXINSTSOPTION=(--maxinsts=${MAXINSTS})
    if [ $((NUM_O3)) > 1 ]; then
        MAXINSTSOPTION+=(--allMainMaxinsts)
    fi
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
    -n "$((NUM_O3))"  --num-main-cores="$NUM_O3" \
    -e "$ENVFILE" -c "$BINA" -o="$ARGS" -i "$IN" \
    --cpu-type=X2 --cpu2-type=X2 \
    --cpu-clock "$FREQ_O3" \
    --pl2sl3cache --l3_extraNoCLat="$NOCLAT" \
    "${MAXINSTSOPTION[@]}" --mem-size="$MEMSIZE" \
    "${CPTRESTOREARGS[@]}" && \
cp ${OUTPUTM5OUT}/${OUTPREFIX}/stats.txt $OUTPUT/${OUTPREFIX}_stats.txt 
