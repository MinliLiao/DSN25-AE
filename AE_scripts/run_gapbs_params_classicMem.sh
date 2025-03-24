# First check if all params are received
if [ "$#" -lt 1 ]; then
    echo "Illegal number of parameters"
    echo "./script.sh experiment_type cpt_type <experiment_args...>"
    echo "e.g., ./script.sh checked restore 1 4 3GHz 500MHz A510 1000000000 ../"
    exit 1
fi

EXPTYPE=$1
CPTTYPE=$2
NOCLATTYPE=none

if [ "$EXPTYPE" == "checkpoint" ]; then
  SCRIPTFILE=run_checkpoint_params_classicMem.sh
  if [ "$CPTTYPE" == "fastforward" ]; then
    if [ "$#" -ne 5 ]; then
        echo "Illegal number of parameters"
        echo "./script.sh checkpoint fastforward num_o3 max_insts m5out_base"
        echo "e.g., ./script.sh checkpoint fastforward 1 1000000000 ../"
        exit 1
    fi
  elif [ "$CPTTYPE" == "roi" ]; then
    if [ "$#" -ne 4 ]; then
        echo "Illegal number of parameters"
        echo "./script.sh checkpoint roi num_o3 m5out_base"
        echo "e.g., ./script.sh checkpoint roi 1 ../"
        exit 1
    fi
  fi
elif [ "$EXPTYPE" == "baseline" ]; then
  if [ "$#" -ne 6 ]; then
      echo "Illegal number of parameters"
      echo "./script.sh baseline cpt_type num_o3 freq_o3 max_insts m5out_base"
      echo "e.g., ./script.sh baseline restore 1 3GHz 1000000000 ../"
      exit 1
  fi
  SCRIPTFILE=run_baseline_params_classicMem.sh
elif [ "$EXPTYPE" == "stored" ] || [ "$EXPTYPE" == "checked" ] || [ "$EXPTYPE" == "opportunistic" ]; then
  if [ "$#" -ne 9 ]; then
      echo "Illegal number of parameters"
      echo "./script.sh stored cpt_type num_o3 num_checkers freq_o3 freq_checkers checker_type max_insts m5out_base"
      echo "e.g., ./script.sh stored restore 1 4 3GHz 500MHz A510 1000000000 ../"
      exit 1
  fi
  SCRIPTFILE=run_paramedic_params_classicMem.sh
elif [ "$EXPTYPE" == "checkedNoC" ] || [ "$EXPTYPE" == "opportunisticNoC" ]; then
  if [ "$#" -ne 9 ]; then
      echo "Illegal number of parameters"
      echo "./script.sh checked cpt_type num_o3 num_checkers freq_o3 freq_checkers checker_type max_insts m5out_base"
      echo "e.g., ./script.sh checked restore 1 4 3GHz 500MHz A510 1000000000 ../"
      exit 1
  fi
  SCRIPTFILE=run_paramedic_params_classicMem.sh
  NOCLATTYPE="$7"
else 
  echo "Illegal experiment_type parameter"
  echo "Valid options are: checkpoint, baseline, stored, checked, checkedNoC, opportunistic, opportunisticNoC"
  exit 1
fi

cd ..
set -u
export BASE=$(pwd)
export GAPBS=$(pwd)/gapbs
N=$(grep ^cpu\\scores /proc/cpuinfo | uniq |  awk '{print $4}')
M=$(grep MemTotal /proc/meminfo | awk '{print $2}')
G=$(expr $M / 8192000)
P=$((G<N ? G : N))
i=0
for bench in bc_roi bfs_roi cc_roi pr_roi
do
  ((i=i%P)); ((i++==0)) && wait
  (
  cd $GAPBS
  IN=$(grep $bench $BASE/gapbs_confs/input_gapbs.txt | awk -F':' '{print $2}'| xargs)
  BIN=$(grep $bench $BASE/gapbs_confs/binaries_gapbs.txt | awk -F':' '{print $2}' | xargs)
  BINA=./$(echo $BIN)
  echo $BINA
  ARGS=$(grep $bench $BASE/gapbs_confs/args_gapbs.txt | awk -F':' '{print $2}'| xargs)
  ENVFILE=$(grep $bench $BASE/gapbs_confs/env_gapbs.txt | awk -F':' '{print $2}'| xargs)
  if [ "$NOCLATTYPE" == "A510" ] || [ "$NOCLATTYPE" == "X2" ]; then
    NOCLAT=$(grep $bench $BASE/gapbs_confs/NoCLat_${NOCLATTYPE}_gapbs.txt | awk -F':' '{print $2}'| xargs)
  else
    NOCLAT=0
  fi
  $BASE/AE_scripts/gem5_scripts/${SCRIPTFILE} "$ENVFILE" "$BINA" "$ARGS" "$IN" "$bench" "${@:2}" "$NOCLAT" "$EXPTYPE"
  ) & 
done
wait
cd $BASE/AE_scripts


