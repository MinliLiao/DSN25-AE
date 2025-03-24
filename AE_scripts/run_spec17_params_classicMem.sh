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
elif [ "$EXPTYPE" == "checkedNoC" ] || [ "$EXPTYPE" == "checkedNoC4x4o" ] || [ "$EXPTYPE" == "opportunisticNoC" ] || [ "$EXPTYPE" == "opportunisticNoC4x4o" ]; then
  if [ "$#" -ne 9 ]; then
      echo "Illegal number of parameters"
      echo "./script.sh checked cpt_type num_o3 num_checkers freq_o3 freq_checkers checker_type max_insts m5out_base"
      echo "e.g., ./script.sh checked restore 1 4 3GHz 500MHz A510 1000000000 ../"
      exit 1
  fi
  SCRIPTFILE=run_paramedic_params_classicMem.sh
  NOCLATTYPE="$7"
  if [ "$EXPTYPE" == "checkedNoC4x4o" ]; then
    NOCLATTYPE="4x4o"
  elif [ "$EXPTYPE" == "opportunisticNoC4x4o" ]; then
    NOCLATTYPE="4x4o"
  fi
elif [ "$EXPTYPE" == "checkedSlowNoC" ] || [ "$EXPTYPE" == "checkedSlowNoC4x4o" ]; then
    if [ "$#" -ne 9 ]; then
      echo "Illegal number of parameters"
      echo "./script.sh checked cpt_type num_o3 num_checkers freq_o3 freq_checkers checker_type max_insts m5out_base"
      echo "e.g., ./script.sh checked restore 1 4 3GHz 500MHz A510 1000000000 ../"
      exit 1
  fi
  SCRIPTFILE=run_paramedic_params_classicMem.sh
  NOCLATTYPE="$7"Slow
  if [ "$EXPTYPE" == "checkedSlowNoC4x4o" ]; then
    NOCLATTYPE="Slow4x4o"
  fi
elif [ "$EXPTYPE" == "checkedHashSlowNoC" ] || [ "$EXPTYPE" == "checkedHashSlowNoC4x4o" ]; then
    if [ "$#" -ne 9 ]; then
      echo "Illegal number of parameters"
      echo "./script.sh checked cpt_type num_o3 num_checkers freq_o3 freq_checkers checker_type max_insts m5out_base"
      echo "e.g., ./script.sh checked restore 1 4 3GHz 500MHz A510 1000000000 ../"
      exit 1
  fi
  SCRIPTFILE=run_paramedic_params_classicMem.sh
  NOCLATTYPE="$7"HashSlow
  if [ "$EXPTYPE" == "checkedHashSlowNoC4x4o" ]; then
    NOCLATTYPE="HashSlow4x4o"
  fi
elif [ "$EXPTYPE" == "checkedNoCErr" ] || [ "$EXPTYPE" == "opportunisticNoCErr" ] || [ "$EXPTYPE" == "checkedNoC4x4oErr" ] || [ "$EXPTYPE" == "opportunisticNoC4x4oErr" ]; then
  if [ "$#" -ne 10 ]; then
      echo "Illegal number of parameters"
      echo "./script.sh checked cpt_type num_o3 num_checkers freq_o3 freq_checkers checker_type max_insts m5out_base err_set"
      echo "e.g., ./script.sh checked restore 1 4 3GHz 500MHz A510 1000000000 ../ 2"
      exit 1
  fi
  SCRIPTFILE=run_errInject_params_classicMem.sh
  NOCLATTYPE="$7"
  if [ "$EXPTYPE" == "checkedNoC4x4iErr" ] || [ "$EXPTYPE" == "opportunisticNoC4x4iErr" ]; then
    NOCLATTYPE="4x4i"
  elif [ "$EXPTYPE" == "checkedNoC4x4oErr" ] || [ "$EXPTYPE" == "opportunisticNoC4x4oErr" ]; then
    NOCLATTYPE="4x4o"
  fi
  ERRSETID="${10}"
else 
  echo "Illegal experiment_type parameter"
  echo "Valid options are: checkpoint, baseline, stored, checked, checkedNoC, opportunistic, opportunisticNoC"
  exit 1
fi

cd ..
set -u
export BASE=$(pwd)
export SPEC=$(pwd)/SPEC17/benchspec/CPU/
N=$(grep ^cpu\\scores /proc/cpuinfo | uniq |  awk '{print $4}')
M=$(grep MemTotal /proc/meminfo | awk '{print $2}')
G=$(expr $M / 8192000)
P=$((G<N ? G : N))
i=0
for bench in bwaves gcc mcf deepsjeng leela exchange2 xz wrf cam4 pop2 imagick nab fotonik3d roms perlbench x264 xalancbmk omnetpp cactuBSSN lbm
do
  if [ "$EXPTYPE" == "checkedNoC4x4o" ] && [ "$7" == "A510" ]; then
    if [ "$6" == "1800MHz" ]; then
      if [ "$bench" != "cam4" ] && [ "$bench" != "gcc" ] && [ "$bench" != "wrf" ]; then
        continue
      fi
    elif [ "$6" == "1600MHz" ]; then
      if [ "$bench" != "cactuBSSN" ] && [ "$bench" != "fotonik3d" ]; then
        continue
      fi
    elif [ "$6" == "1400MHz" ]; then
      if [ "$bench" == "bwaves" ] || [ "$bench" == "roms" ] || [ "$bench" == "cam4" ] || [ "$bench" == "gcc" ] || [ "$bench" == "wrf" ] || [ "$bench" == "cactuBSSN" ] || [ "$bench" == "fotonik3d" ]; then
        continue
      fi
    fi
  fi
  ((i=i%P)); ((i++==0)) && wait
  (
  cd $SPEC
  IN=$(grep $bench $BASE/spec_confs/input_2017.txt | awk -F':' '{print $2}'| xargs)
  BIN=$(grep $bench $BASE/spec_confs/binaries_2017.txt | awk -F':' '{print $2}' | xargs)
  BINA=./$(echo $BIN)"_base.mytest-64"
  echo $BINA
  ARGS=$(grep $bench $BASE/spec_confs/args_2017.txt | awk -F':' '{print $2}'| xargs)
  ENVFILE=$(grep $bench $BASE/spec_confs/env_2017.txt | awk -F':' '{print $2}'| xargs)
  if [ "$NOCLATTYPE" == "A510" ] || [ "$NOCLATTYPE" == "X2" ] || [ "$NOCLATTYPE" == "A510Slow" ] || [ "$NOCLATTYPE" == "X2Slow" ] || [ "$NOCLATTYPE" == "A510HashSlow" ] || [ "$NOCLATTYPE" == "X2HashSlow" ]; then
    NOCLAT=$(grep $bench $BASE/spec_confs/NoCLat_${NOCLATTYPE}_2017.txt | awk -F':' '{print $2}'| xargs)
  elif [ "$NOCLATTYPE" == "4x4o" ] || [ "$NOCLATTYPE" == "HashSlow4x4o" ] || [ "$NOCLATTYPE" == "Slow4x4o" ]; then
    NOCLAT=$(grep $bench $BASE/spec_confs/NoCLat_${NOCLATTYPE}${4}c_spec17.txt | awk -F':' '{print $2}'| xargs)
  else
    NOCLAT=0
  fi
  cd *$(echo $bench)_s/run/run_base_refspeed_mytest-64.0000
  if [ "$bench" == "omnetpp" ]; then
    ARGS="$ARGS -n $(pwd)/ned/"
  fi
  if [ "$EXPTYPE" == "checkedNoCErr" ] || [ "$EXPTYPE" == "checkedNoC4x4oErr" ]; then
    ERRTYPE=$(grep "^$ERRSETID:" $BASE/AE_scripts/FUdest_err.txt | awk -F':' '{print $2}' | awk -F' ' '{print $1}'| xargs)
    ERRPLACE=$(grep "^$ERRSETID:" $BASE/AE_scripts/FUdest_err.txt | awk -F':' '{print $2}' | awk -F' ' '{print $2}'| xargs)
    ERRBIT=$(grep "^$ERRSETID:" $BASE/AE_scripts/FUdest_err.txt | awk -F':' '{print $2}' | awk -F' ' '{print $3}'| xargs)
    ERRVAL=$(grep "^$ERRSETID:" $BASE/AE_scripts/FUdest_err.txt | awk -F':' '{print $2}' | awk -F' ' '{print $4}'| xargs)
    $BASE/AE_scripts/gem5_scripts/${SCRIPTFILE} "$ENVFILE" "$BINA" "$ARGS" "$IN" "$bench" "${@:2:8}" "$ERRTYPE" "$ERRPLACE" "$ERRBIT" "$ERRVAL" "$NOCLAT" "$EXPTYPE"
  elif [ "$EXPTYPE" == "opportunisticNoCErr" ] || [ "$EXPTYPE" == "opportunisticNoC4x4oErr" ]; then
    if grep "^$bench:" $BASE/AE_scripts/detected_err.txt | grep -q " ${ERRSETID} "; then
      ERRTYPE=$(grep "^$ERRSETID:" $BASE/AE_scripts/detected_err.txt | awk -F':' '{print $2}' | awk -F' ' '{print $1}'| xargs)
      ERRPLACE=$(grep "^$ERRSETID:" $BASE/AE_scripts/detected_err.txt | awk -F':' '{print $2}' | awk -F' ' '{print $2}'| xargs)
      ERRBIT=$(grep "^$ERRSETID:" $BASE/AE_scripts/detected_err.txt | awk -F':' '{print $2}' | awk -F' ' '{print $3}'| xargs)
      ERRVAL=$(grep "^$ERRSETID:" $BASE/AE_scripts/detected_err.txt | awk -F':' '{print $2}' | awk -F' ' '{print $4}'| xargs)
      # echo "$bench $ERRSETID"
      $BASE/AE_scripts/gem5_scripts/${SCRIPTFILE} "$ENVFILE" "$BINA" "$ARGS" "$IN" "$bench" "${@:2:8}" "$ERRTYPE" "$ERRPLACE" "$ERRBIT" "$ERRVAL" "$NOCLAT" "$EXPTYPE"
    fi
  else
    $BASE/AE_scripts/gem5_scripts/${SCRIPTFILE} "$ENVFILE" "$BINA" "$ARGS" "$IN" "$bench" "${@:2}" "$NOCLAT" "$EXPTYPE"
  fi
  ) & 
done
wait
cd $BASE/AE_scripts
