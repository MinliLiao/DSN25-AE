

import sys
import argparse

parser = argparse.ArgumentParser(description='generate gnuplot file')
parser.add_argument('--filename', type=str, action='store',required=True,
                    help='the filename of the csv file to transform')
parser.add_argument('--stat', type=str, action='store',required=True,
                    help='the name of the statistics')
parser.add_argument('--plottype', type=str,
                    choices=["Bar","Line"], required=True,
                    help='the name of the statistics')

args = parser.parse_args()
out_filename = ".".join(args.filename.split(".")[:-1]) + ".eps"

series = []
data_min = ""
data_max = ""
num_data_lines = 0
with open(args.filename) as f:
    first_line = True
    for line in f:
        if first_line:
            series = line.replace("\n","").split(",")[1:]
            first_line = False
        else:
            if line not in ["\n"]:
                num_data_lines += 1
            for val in line.split(" ")[1:]:
                if data_min in [""] or eval(val) < data_min:
                    data_min = eval(val)
                if data_max in [""] or eval(val) > data_max:
                    data_max = eval(val)

colors_box = ["#a6cee3", "#1f78b4", "#b2df8a", "#33a02c", "#fb9a99", "#e31a1c", "#fdbf6f"]
colors_line = ["#1b9e77", "#d95f02", "#7570b3", "#e7298a", "#66a61e", "#e6ab02", "#a6761d"]
pointTypes = ["1", "2", "4"]
dashTypes = ["2", "3", "4"]

gp_lines = []
gp_lines.append("set terminal postscript eps enhanced colour size 2.5, 1.4 font \",11\"")
gp_lines.append("set output \"" + out_filename + "\"")

gp_lines.append("#set xrange [-1.8:41.8]")
gp_lines.append("#set x2range[-1.8:41.8]")
gp_lines.append("set yrange [" + str(data_min * 0.9) + ":" + str(data_max + data_min * 0.1) + "]")
gp_lines.append("")
gp_lines.append("set xtics out nomirror")
gp_lines.append("")
gp_lines.append("set grid x2 y")
gp_lines.append("")
gp_lines.append("set ylabel \"" + args.stat + "\"")
gp_lines.append("")
gp_lines.append("set key above title \"\"")
gp_lines.append("set boxwidth 1")
gp_lines.append("set style data errorbars ")
gp_lines.append("set style fill solid border -1 ")
gp_lines.append("set rmargin 1")
gp_lines.append("")
if args.plottype in ["Bar"]:
    gp_lines.append("set xtics out nomirror rotate by 35 right 1, 4, 9 format \"\"")
    gp_lines.append("set x2tics out scale 0 format \"\" " + str(len(series) - 0.5 + len(series)*(1/0.7-1)/2) + ", "+ str(len(series)/0.7) +", 195")
    gp_lines.append("plot \'" + args.filename + "\' u ($0*" + str(len(series)/0.7) + "+" + str(len(series)/2 - 0.5) + "):(0):xtic(1) with boxes notitle ,\\")    
    for i in range(len(series)):
        gp_lines.append(" \'" + args.filename + "\' using ($0*" + str(len(series)/0.7) + "+" + str(i) + "):" + str(i+2) + " with boxes lc rgb \'" + colors_box[i + (4 if len(series) < 4 and "A510" in series[i] else 0)] + "\' title \"" + series[i].replace("@"," \\\\@ ").replace("_", "\\\\_") + "\", \\")   
    gp_lines.append("                                     1  lc rgb \'#444444\' linetype 1 title \"\"")
elif args.plottype in ["Line"]:
    gp_lines[0] = "set terminal postscript eps enhanced colour size 5, 1.4 font \",11\""
    gp_lines.append("set xrange [-0.5:" + str(num_data_lines - 0.5) + "]")
    gp_lines.append("set x2tics out scale 0 format \"\" -1, 20, 40")
    gp_lines.append("set multiplot layout 1,1")
    gp_lines.append("    set xtics out nomirror rotate by 35 right format \"\"")
    gp_lines.append("    plot \'" + args.filename + "\' using 2:xtic(1) title \'" + series[0].replace("@"," \\\\@ ").replace("_", "\\\\_") + "\' with linespoints lc rgb \'" + colors_line[0] + "\' pt " + pointTypes[0] + " dt " + dashTypes[0] + ",\\")    
    for i in range(len(series)-1):
        if series[i+1] in ["geomean"]:
            gp_lines.append("\t     \'" + args.filename + "\' using " + str(i+3) + " title \'" + series[i+1].replace("@"," \\\\@ ").replace("_", "\\\\_") + "\' with lines lc rgb \'#ff0000\' lw 3, \\")
        else:
            gp_lines.append("\t     \'" + args.filename + "\' using " + str(i+3) + " title \'" + series[i+1].replace("@"," \\\\@ ").replace("_", "\\\\_") + "\' with linespoints lc rgb \'" + colors_line[(i+1)%7] + "\' pt " + pointTypes[(i+1)//7] + " dt " + dashTypes[(i+1)%3] + ", \\")
    gp_lines.append("\t                                     1  lc rgb \'#444444\' linetype 1 title \"\"")
    gp_lines.append("    set x2tics (\"1*X2\" -0.5, \"2*X2\" 5.5, \"4*A510\" 11.5, \"\" 17.5) offset 10,-5")
    gp_lines.append("    set origin 0,0 # This is not needed in version 5.0")
    gp_lines.append("    replot")
    gp_lines.append("unset multiplot")
print("\n".join(gp_lines))