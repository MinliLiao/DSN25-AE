set terminal postscript eps enhanced colour size 2.5, 1.4 font ",11"
set output RESULTS_DIR."/spec17_100M_slowdown_opp.eps"
#set xrange [-1.8:41.8]
#set x2range[-1.8:41.8]
set yrange [.995:1.06]

set xtics out nomirror

set grid x2 y

set ylabel "Slowdown"

set key top right font ",8" title ""
set boxwidth .8
set style data errorbars 
set style fill solid border -1 
# set rmargin 1

set xtics out nomirror rotate by 45 right 1, 4, 9 format ""
set x2tics out scale 0 format "" 3, 4, 195
plot RESULTS_DIR.'/spec17_100M_slowdown_opp.data' u ($0*4+1):(0):xtic(1) with boxes notitle ,\
 RESULTS_DIR.'/spec17_100M_slowdown_opp.data' using ($0*4+0):2 with boxes lc rgb '#b2df8a' title "Homogeneous", \
 RESULTS_DIR.'/spec17_100M_slowdown_opp.data' using ($0*4+1):3 with boxes lc rgb '#33a02c' title "2*X2 \\@ 1.5GHz", \
 RESULTS_DIR.'/spec17_100M_slowdown_opp.data' using ($0*4+2):4 with boxes lc rgb '#fdbf6f' title "4*A510 \\@ 2GHz", \
                                     1  lc rgb '#444444' linetype 1 title ""
