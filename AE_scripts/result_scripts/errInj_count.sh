echo -n "numDetected"
for bench in bwaves gcc mcf deepsjeng leela exchange2 xz wrf cam4 pop2 imagick nab fotonik3d roms perlbench x264 xalancbmk omnetpp cactuBSSN lbm
do
    echo -n ",$bench"
done
echo ""
DETECTINJS=$1
echo -n "Baseline"
for bench in bwaves gcc mcf deepsjeng leela exchange2 xz wrf cam4 pop2 imagick nab fotonik3d roms perlbench x264 xalancbmk omnetpp cactuBSSN lbm
do
    NUMDETECT=$(grep ${bench} ${DETECTINJS} | awk -F':' '{print $2}' | awk '{print NF}')
    echo -n ",${NUMDETECT}"
done
echo ""
M5OUTBASE=$2
for config in 1_1_3GHz_500MHz_A510 1_1_3GHz_1000MHz_A510 1_1_3GHz_2000MHz_A510 1_2_3GHz_2000MHz_A510
do
    DETECTINJS=${M5OUTBASE}/${config}_detect
    grep "Detect" ${M5OUTBASE}/*Err*${config}*/simout > ${DETECTINJS}
    if [ "$config" == "1_1_3GHz_500MHz_A510" ]; then
        echo -n "1*A510@0.5GHz"
    elif [ "$config" == "1_1_3GHz_1000MHz_A510" ]; then
        echo -n "1*A510@1GHz"
    elif [ "$config" == "1_1_3GHz_2000MHz_A510" ]; then
        echo -n "1*A510@2GHz"
    elif [ "$config" == "1_2_3GHz_2000MHz_A510" ]; then
        echo -n "2*A510@2GHz"
    fi
    for bench in bwaves gcc mcf deepsjeng leela exchange2 xz wrf cam4 pop2 imagick nab fotonik3d roms perlbench x264 xalancbmk omnetpp cactuBSSN lbm
    do
        NUMDETECT=$(grep ${bench} ${DETECTINJS} | wc -l)
        echo -n ",${NUMDETECT}"
    done
    echo ""
done

