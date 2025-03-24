set terminal postscript eps enhanced colour size 2.5, 1.275 font ",11"
set output RESULTS_DIR."/spec17_errRate.eps"
#set xrange [-1.8:41.8]
#set x2range[-1.8:41.8]
set yrange [0.85:1]

set xtics out nomirror

set grid x2 y

set ylabel "Error Detection Coverage"

set key above title ""
set boxwidth 1
set style data errorbars 
set style fill solid border -1 
set rmargin 1

set xtics out nomirror rotate by 45 right 1, 4, 9 format ""
set x2tics out scale 0 format "" 1, 2, 195
plot RESULTS_DIR.'/spec17_errRate.data' u ($0*2):(0):xtic(1) with boxes notitle ,\
 RESULTS_DIR.'/spec17_errRate.data' using ($0*2):5 with boxes lc rgb '#a6cee3' title "2*A510 \\@ 2GHz", \
 RESULTS_DIR.'/spec17_errRate.data' using ($0*2):4 with boxes lc rgb '#1f78b4' title "1*A510 \\@ 2GHz", \
 RESULTS_DIR.'/spec17_errRate.data' using ($0*2):3 with boxes lc rgb '#b2df8a' title "1*A510 \\@ 1GHz", \
 RESULTS_DIR.'/spec17_errRate.data' using ($0*2):2 with boxes lc rgb '#33a02c' title "1*A510 \\@ 0.5GHz", \
                                     1  lc rgb '#444444' linetype 1 title ""
