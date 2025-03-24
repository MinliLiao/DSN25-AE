set terminal postscript eps enhanced colour size 2.5, 1.4 font ",11"
set output RESULTS_DIR."spec17_100M_slowdown_slowNoC.eps"
#set xrange [-1.8:41.8]
#set x2range[-1.8:41.8]
set yrange [.95:2]

set xtics out nomirror

set grid x2 y

set logscale y
     set format y ""
set ytics add ("1.05" 1.05,  "1" 1, "" 1.1, "1.15" 1.15,  "1.75" 1.75, "1.25" 1.25, "1.5" 1.5, "1.35" 1.35, "2" 2, "2.5" 2.5, \
                    "3" 3, "3.5" 3.5, "4" 4, "5" 5, "6" 6, "7" 7)

set ylabel "Slowdown"

set key top left font ",9" title ""
set boxwidth 1
set style data errorbars 
set style fill solid border -1 
set rmargin 1
set multiplot
set xtics out nomirror rotate by 45 right 1, 4, 9 format ""
set x2tics out scale 0 format "" 2+1, 4, 195
plot RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' u ($0*4+1.0):(0):xtic(1) with boxes notitle ,\
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+0):11 with boxes fill pattern 6 lc rgb '#252525' notitle, \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+1):12 with boxes fill pattern 6 lc rgb '#252525' notitle, \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+2):13 with boxes fill pattern 6 lc rgb '#252525' notitle, \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+0):8 with boxes lc rgb '#969696' notitle, \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+1):9 with boxes lc rgb '#969696' notitle, \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+2):10 with boxes lc rgb '#969696' notitle, \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+0):5 with boxes lc rgb '#252525' notitle, \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+1):6 with boxes lc rgb '#252525' notitle, \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+2):7 with boxes lc rgb '#252525' notitle, \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+0):2 with boxes lc rgb '#b2df8a' title "Homogeneous", \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+1):3 with boxes lc rgb '#33a02c' title "2*X2 \\@ 1.5GHz", \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+2):4 with boxes lc rgb '#fb9a99' title " 4*A510 \\@ 2GHz", \
                                     1  lc rgb '#444444' linetype 1 title ""
unset xtics
unset ytics
unset y2tics
unset ylabel
unset grid
unset border                                     
 set key top right font ",9" title ""
 plot  RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+0):(0) with boxes fill pattern 6 lc rgb '#252525' title "slowNoC", \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+1):(0) with boxes lc rgb '#969696' title "hashed slowNoC", \
 RESULTS_DIR.'spec17_100MslowNoC_slowdown.data' using ($0*4+1):(0) with boxes lc rgb '#252525' title "NoC overhead", \
