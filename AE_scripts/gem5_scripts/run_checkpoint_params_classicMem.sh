# Check args first
if [ "$#" -lt 8 ]; then
    echo "Final script, incorrect arguments"
    exit 1
fi

ENVFILE=$1
BINA=$2
ARGS=$3
IN=$4
BENCH=$5
CPTTYPE=$6
NUM_O3=$7

APPOUT=app.out
APPERR=app.err
NUMPROC=$(echo $BINA | grep -o ";" | wc -l)
for i in `seq 1 $NUMPROC`
do
  APPOUT="$APPOUT;app$i.out"
  APPERR="$APPERR;app$i.err"
done
MEMSIZE="$((16 + 16 * NUMPROC))GB"

if [ "$CPTTYPE" == "fastforward" ]; then
  MAXINSTS=$8
  M5OUTBASE=$9
  OUTPUTM5OUT=$M5OUTBASE/m5out_cpt
  CPTOPTIONS=(--cpu-type=TimingSimpleCPU --fast-forward=${MAXINSTS} --maxinsts=1000000 --checkpoint-at-end --checkpoint-dir=${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3})
elif [ "$CPTTYPE" == "roi" ]; then
  M5OUTBASE=$8
  OUTPUTM5OUT=$M5OUTBASE/m5out_cpt
  CPTOPTIONS=(--cpu-type=TimingSimpleCPU --fast-forward=0 --maxinsts=1000000 --work-item-id=0 --work-begin-exit-count=1 --checkpoint-at-end --checkpoint-dir=${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3})
elif [ "$CPTTYPE" == "roi_mt" ]; then
  M5OUTBASE=$8
  OUTPUTM5OUT=$M5OUTBASE/m5out_cpt
  CPTOPTIONS=(--cpu-type=AtomicSimpleCPU --work-item-id=0 --work-begin-exit-count=1 --checkpoint-at-end --checkpoint-dir=${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3})
  if [ "$BENCH" == "bfs_mt" ]; then
    CPTOPTIONS[2]="--work-begin-exit-count=2"
  fi
else
  echo "Illegal checkpoint option $CPTTYPE"
  exit 1
fi

# Create results folder if it does not exist
mkdir -p $OUTPUTM5OUT

$BASE/build/ARM/gem5.opt \
    --outdir ${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3} \
    --redirect-stdout --redirect-stderr \
    $BASE/configs/example/se.py \
    --output="$APPOUT" --errout="$APPERR" \
    -n "$((NUM_O3))"  --num-main-cores="$NUM_O3"  --mem-size="$MEMSIZE"  \
    -e "$ENVFILE" -c "$BINA" -o="$ARGS" -i "$IN" \
    "${CPTOPTIONS[@]}" && \
SRCPHYSMEMFILE=$(ls ${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3}/cpt.*/system.physmem.store0.pmem) && \
cp -r ${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3} ${OUTPUTM5OUT}/cpt_baseline_${BENCH}_${NUM_O3} && \
DSTPHYSMEMFILE=$(ls ${OUTPUTM5OUT}/cpt_baseline_${BENCH}_${NUM_O3}/cpt.*/system.physmem.store0.pmem) && \
rm $DSTPHYSMEMFILE && \
ln -s $SRCPHYSMEMFILE $DSTPHYSMEMFILE && \
sed -i 's/system\.switch_cpus/system\.cpu/g' ${OUTPUTM5OUT}/cpt_baseline_${BENCH}_${NUM_O3}/cpt.*/m5.cpt && \
(
  if ((NUM_O3 == 1)); then
    cp -r ${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3} ${OUTPUTM5OUT}/cpt_paramedic_${BENCH}_${NUM_O3} && \
    DSTPHYSMEMFILE=$(ls ${OUTPUTM5OUT}/cpt_paramedic_${BENCH}_${NUM_O3}/cpt.*/system.physmem.store0.pmem) && \
    rm $DSTPHYSMEMFILE && \
    ln -s $SRCPHYSMEMFILE $DSTPHYSMEMFILE && \
    sed -i.bk 's/system\.switch_cpus/system\.cpu0/g' ${OUTPUTM5OUT}/cpt_paramedic_${BENCH}_${NUM_O3}/cpt.*/m5.cpt && \
    sed -i 's/system\.cpu\./system\.cpu0\./g' ${OUTPUTM5OUT}/cpt_paramedic_${BENCH}_${NUM_O3}/cpt.*/m5.cpt && \
    sed -i 's/system\.cpu]/system\.cpu0]/g' ${OUTPUTM5OUT}/cpt_paramedic_${BENCH}_${NUM_O3}/cpt.*/m5.cpt
  else
    ln -s cpt_baseline_${BENCH}_${NUM_O3} ${OUTPUTM5OUT}/cpt_paramedic_${BENCH}_${NUM_O3}
  fi
) && \
(
  if ((NUM_O3 == 1)); then
    cp -r ${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3} ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3} && \
    DSTPHYSMEMFILE=$(ls ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3}/cpt.*/system.physmem.store0.pmem) && \
    rm $DSTPHYSMEMFILE && \
    ln -s $SRCPHYSMEMFILE $DSTPHYSMEMFILE && \
    sed -i.bk 's/system\.switch_cpus/system\.cpu00/g' ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3}/cpt.*/m5.cpt && \
    sed -i 's/system\.cpu\./system\.cpu00\./g' ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3}/cpt.*/m5.cpt && \
    sed -i 's/system\.cpu]/system\.cpu00]/g' ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3}/cpt.*/m5.cpt
  elif ((NUM_O3 > 10)); then
    ln -s cpt_baseline_${BENCH}_${NUM_O3} ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3}
  else 
    cp -r ${OUTPUTM5OUT}/cpt_${BENCH}_${NUM_O3} ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3} && \
    DSTPHYSMEMFILE=$(ls ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3}/cpt.*/system.physmem.store0.pmem) && \
    rm $DSTPHYSMEMFILE && \
    ln -s $SRCPHYSMEMFILE $DSTPHYSMEMFILE && \
    sed -i 's/system\.switch_cpus/system\.cpu0/g' ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3}/cpt.*/m5.cpt && \
    sed -i 's/system\.cpu\([0-9]\)\./system\.cpu0\1\./g' ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3}/cpt.*/m5.cpt && \
    sed -i 's/system\.cpu\([0-9]\)]/system\.cpu0\1]/g' ${OUTPUTM5OUT}/cpt_paradox_${BENCH}_${NUM_O3}/cpt.*/m5.cpt
  fi
) && \
exit 0