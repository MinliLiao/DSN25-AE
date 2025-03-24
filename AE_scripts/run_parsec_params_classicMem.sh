# First check if all params are received
if [ "$#" -lt 1 ]; then
    echo "Illegal number of parameters"
    echo "./script.sh experiment_type cpt_type <experiment_args...>"
    echo "e.g., ./script.sh checked restore 1 4 3GHz 500MHz A510 1000000000 ../"
    exit 1
fi

EXPTYPE=$1
CPTTYPE=$2
NUMTHREADS=$3
NOCLATTYPE=none

if [ "$EXPTYPE" == "baseline" ]; then
  if [ "$#" -ne 6 ]; then
      echo "Illegal number of parameters"
      echo "./script.sh baseline cpt_type num_o3 freq_o3 max_insts m5out_base"
      echo "e.g., ./script.sh baseline restore 1 3GHz 1000000000 ../"
      exit 1
  fi
  SCRIPTFILE=run_baseline_params_classicMem.sh
  RUNFILESUFFIX=threads${NUMTHREADS}_baseline
elif [ "$EXPTYPE" == "stored" ] || [ "$EXPTYPE" == "checked" ] || [ "$EXPTYPE" == "opportunistic" ]; then
  if [ "$#" -ne 9 ]; then
      echo "Illegal number of parameters"
      echo "./script.sh stored cpt_type num_o3 num_checkers freq_o3 freq_checkers checker_type max_insts m5out_base"
      echo "e.g., ./script.sh stored restore 1 4 3GHz 500MHz A510 1000000000 ../"
      exit 1
  fi
  SCRIPTFILE=run_paramedic_params_classicMem.sh
  RUNFILESUFFIX=threads${NUMTHREADS}_${4}x${6}${7}_${EXPTYPE}
elif [ "$EXPTYPE" == "checkedNoC" ] || [ "$EXPTYPE" == "opportunisticNoC" ]; then
  if [ "$#" -ne 9 ]; then
      echo "Illegal number of parameters"
      echo "./script.sh checkedNoC cpt_type num_o3 num_checkers freq_o3 freq_checkers checker_type max_insts m5out_base"
      echo "e.g., ./script.sh checkedNoC restore 1 4 3GHz 500MHz A510 1000000000 ../"
      exit 1
  fi
  SCRIPTFILE=run_paramedic_params_classicMem.sh
  RUNFILESUFFIX=threads${NUMTHREADS}_${4}x${6}${7}_${EXPTYPE}
  NOCLATTYPE="$7"
else 
  echo "Illegal experiment_type parameter"
  echo "Valid options are: baseline, stored, checked, checkedNoC, opportunistic, opportunisticNoC"
  exit 1
fi

cd ..
set -u
export BASE=$(pwd)
export PARSEC=$(pwd)/parsec
mkdir -p $PARSEC/run_dir
mkdir -p $PARSEC/run_dir/${RUNFILESUFFIX}
N=$(grep ^cpu\\scores /proc/cpuinfo | uniq |  awk '{print $4}')
M=$(grep MemTotal /proc/meminfo | awk '{print $2}')
G=$(expr $M / 8192000)
P=$((G<N ? G : N))
i=0
for bench in blackscholes bodytrack fluidanimate freqmine swaptions streamcluster
do
  ((i=i%P)); ((i++==0)) && wait
  (
  if [ "$bench" == "bodytrack" ]; then
    NUM_O3=$((NUMTHREADS + 2))
  elif [ "$bench" == "streamcluster" ]; then
    NUM_O3=$((2 * NUMTHREADS + 1))
  else
    NUM_O3=$((NUMTHREADS + 1))
  fi
  if [ "$bench" == "streamcluster" ]; then
    BENCHDIR="/pkgs/kernels/$bench/"
  else
    BENCHDIR="/pkgs/apps/$bench/"
  fi
  cd $PARSEC
  set +u
  source ./env.sh
  set -u
  parsecmgmt -a run -p "$bench" -i simmedium -n "$NUMTHREADS" -d "$PARSEC/run_dir/${RUNFILESUFFIX}"
  cd $PARSEC/run_dir/${RUNFILESUFFIX}/${BENCHDIR}/run
  echo "OMP_NUM_THREADS=$NUMTHREADS" > env.sh
  IN=$(grep $bench $BASE/parsec_confs/input_parsec.txt | awk -F':' '{print $2}'| xargs) 
  BIN=$(grep $bench $BASE/parsec_confs/binaries_parsec.txt | awk -F':' '{print $2}' | xargs) 
  BINA=$PARSEC/$(echo $BIN)
  echo $BINA
  ARGS=$(grep $bench $BASE/parsec_confs/args_parsec.txt | awk -F':' '{print $2}'| xargs) 
  ARGS=${ARGS//NUMTHREADS/$NUMTHREADS}
  ENVFILE=$(grep $bench $BASE/parsec_confs/env_parsec.txt | awk -F':' '{print $2}'| xargs)
  if [ "$NOCLATTYPE" == "A510" ] || [ "$NOCLATTYPE" == "X2" ]; then
    NOCLAT=$(grep $bench $BASE/parsec_confs/NoCLat_parsec.txt | awk -F':' '{print $2}'| xargs)
  else
    NOCLAT=0
  fi
  $BASE/AE_scripts/gem5_scripts/${SCRIPTFILE} "$ENVFILE" "$BINA" "$ARGS" "$IN" "$bench" "$CPTTYPE" "$NUM_O3" "${@:4}" "$NOCLAT" "$EXPTYPE"
  ) & 
done
wait
cd $BASE/AE_scripts


