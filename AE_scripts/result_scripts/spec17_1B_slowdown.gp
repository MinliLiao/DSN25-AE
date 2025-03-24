set terminal postscript eps enhanced colour size 5, 1.4 font ",11"
set output RESULTS_DIR."/spec17_1B_slowdown.eps"
#set xrange [-1.8:41.8]
#set x2range[-1.8:41.8]
set yrange [.95:1.75]

set xtics out nomirror

set grid x2 y

set ylabel "Slowdown"
set logscale y
     set format y ""
set ytics add ("1.05" 1.05,  "1" 1, "" 1.1, "1.15" 1.15,  "1.75" 1.75, "1.25" 1.25, "1.5" 1.5, "1.35" 1.35, "2.5" 2.5, \
                    "3" 3, "3.5" 3.5, "4" 4, "5" 5, "6" 6, "7" 7)

#set key right above width -3 vertical maxrows 3 font ",9" title ""
set key top left vertical maxrows 2 font ",9" title ""
set boxwidth 1
set style data errorbars 
set style fill solid border -1 
set rmargin 1

set xtics out nomirror rotate by 45 right 1, 4, 9 format ""
set x2tics out scale 0 format "" 6, 7, 195

plot RESULTS_DIR.'/spec17_1B_slowdown.data' u ($0*7+2.5):(0):xtic(1) with boxes notitle ,\
 RESULTS_DIR.'/spec17_1B_slowdown.data' using ($0*7+0):2 with boxes lc rgb '#a6cee3' title "DSN18", \
 RESULTS_DIR.'/spec17_1B_slowdown.data' using ($0*7+1):3 with boxes lc rgb '#1f78b4' title "Paradox", \
 RESULTS_DIR.'/spec17_1B_slowdown.data' using ($0*7+2):4 with boxes lc rgb '#b2df8a' title "Homogeneous", \
 RESULTS_DIR.'/spec17_1B_slowdown.data' using ($0*7+3):5 with boxes lc rgb '#33a02c' title "2*X2 \\@ 1.5GHz", \
 RESULTS_DIR.'/spec17_1B_slowdown.data' using ($0*7+4):6 with boxes lc rgb '#fb9a99' title "4*A510 \\@ 2GHz", \
 RESULTS_DIR.'/spec17_1B_slowdown.data' using ($0*7+5):7 with boxes lc rgb '#fdbf6f' title "4*A510 min ED2P", \
                                     1  lc rgb '#444444' linetype 1 title ""
