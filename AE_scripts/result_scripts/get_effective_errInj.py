import argparse

parser = argparse.ArgumentParser(description='Get error injection inputs that were effective (injected detected masked or missed error)')
parser.add_argument('--detect_file', type=str, action='store',required=True,
                    help='the filename of list of files with detected errors')
parser.add_argument('--masked_file', type=str, action='store',
                    help='the filename of list of files with non-zero masked errors')
parser.add_argument('--missed_file', type=str, action='store',
                    help='the filename of list of files with non-zero missed errors')
parser.add_argument('--inject_file', type=str, action='store',required=True,
                    help='the filename of list of files with non-zero error injections')
parser.add_argument('--max_cores', type=int, action='store', default=80,
                    help='the maximum number of cores to run simulations in parallel')

args = parser.parse_args()

def parse_bench_errInj(line):
    bench = line.split("/")[-2].split("_")[0]
    errInjType = ""
    if "FUdest" in line:
        errInjType = "FUdest"
    errInjStr = line.split("/")[-2].split("_")[2]
    opClass = errInjStr.split("b")[0][1:]
    bitPos = errInjStr.split("b")[1].split("s")[0]
    stuckAt1 = errInjStr.split("s")[1]
    errInjStr = opClass + " " + bitPos + " " + stuckAt1
    return bench, errInjType, errInjStr

benchmarks = ["bwaves", "gcc", "mcf", "deepsjeng", "leela", "exchange2", "xz", "wrf", "cam4", "pop2", "imagick", "nab", "fotonik3d", "roms", "perlbench", "x264", "xalancbmk", "omnetpp", "cactuBSSN", "lbm"]
errInjDict = dict()
for bench in benchmarks:
    errInjDict[bench] = dict()
    errInjDict[bench]["Detect"] = set()
    errInjDict[bench]["Masked"] = set()
    errInjDict[bench]["Unchanged"] = set()
    errInjDict[bench]["Missed"] = set()
    errInjDict[bench]["Injected"] = set()

errInjSet_detected = dict()
errInjSet_detected["FUdest"] = set()

FUdest_err = dict()
FUdest_err["1"] = "2 29 1"
FUdest_err["2"] = "168 10 0"
FUdest_err["3"] = "16 717 1"
FUdest_err["4"] = "3 29 1"
FUdest_err["5"] = "56 13 0"
FUdest_err["6"] = "16 782 1"
FUdest_err["7"] = "164 19 1"
FUdest_err["8"] = "17 115 1"
FUdest_err["9"] = "34 1195 1"
FUdest_err["10"] = "72 1565 1"
FUdest_err["11"] = "4 49 0"
FUdest_err["12"] = "4 6 0"
FUdest_err["13"] = "21 1438 1"
FUdest_err["14"] = "3 6 0"
FUdest_err["15"] = "16 1324 1"
FUdest_err["16"] = "3 34 0"
FUdest_err["17"] = "2 50 1"
FUdest_err["18"] = "110 57 0"
FUdest_err["19"] = "115 39 1"
FUdest_err["20"] = "3 51 0"
FUdest_err["21"] = "30 1351 0"
FUdest_err["22"] = "56 16 0"
FUdest_err["23"] = "16 494 1"
FUdest_err["24"] = "126 41 0"
FUdest_err["25"] = "164 31 1"
FUdest_err["26"] = "3 40 0"
FUdest_err["27"] = "110 1 0"
FUdest_err["28"] = "15 78 1"
FUdest_err["29"] = "59 99 0"
FUdest_err["30"] = "3 30 0"
FUdest_err["31"] = "2 46 0"
FUdest_err["32"] = "3 10 0"
FUdest_err["33"] = "21 8 0"
FUdest_err["34"] = "3 13 1"
FUdest_err["35"] = "3 22 1"
FUdest_err["36"] = "3 9 1"
FUdest_err["37"] = "3 55 0"
FUdest_err["38"] = "5 61 1"
FUdest_err["39"] = "3 29 0"
FUdest_err["40"] = "3 60 1"
FUdest_err["41"] = "21 1253 0"
FUdest_err["42"] = "9 27 1"
FUdest_err["43"] = "65 1319 0"
FUdest_err["44"] = "30 1890 0"
FUdest_err["45"] = "2 31 0"
FUdest_err["46"] = "60 12 1"
FUdest_err["47"] = "5 21 0"
FUdest_err["48"] = "35 877 1"
FUdest_err["49"] = "14 377 0"
FUdest_err["50"] = "164 29 0"
FUdest_err["51"] = "56 42 0"
FUdest_err["52"] = "17 794 1"
FUdest_err["53"] = "110 1 1"
FUdest_err["54"] = "34 1109 0"
FUdest_err["55"] = "2 17 0"
FUdest_err["56"] = "110 47 0"
FUdest_err["57"] = "3 35 0"
FUdest_err["58"] = "110 17 0"
FUdest_err["59"] = "28 714 0"
FUdest_err["60"] = "164 27 1"
FUdest_err["61"] = "164 28 1"
FUdest_err["62"] = "56 3 1"
FUdest_err["63"] = "14 1354 0"
FUdest_err["64"] = "8 32 1"
FUdest_err["65"] = "3 21 1"
FUdest_err["66"] = "35 1699 0"
FUdest_err["67"] = "29 58 1"
FUdest_err["68"] = "3 18 1"
FUdest_err["69"] = "3 26 0"
FUdest_err["70"] = "15 929 0"
FUdest_err["71"] = "30 76 1"
FUdest_err["72"] = "3 58 0"
FUdest_err["73"] = "16 1068 1"
FUdest_err["74"] = "35 1960 0"
FUdest_err["75"] = "110 3 0"
FUdest_err["76"] = "110 33 0"
FUdest_err["77"] = "2 32 1"
FUdest_err["78"] = "64 760 0"
FUdest_err["79"] = "164 15 0"
FUdest_err["80"] = "21 104 0"
FUdest_err["81"] = "30 1258 1"
FUdest_err["82"] = "13 926 0"
FUdest_err["83"] = "64 306 1"
FUdest_err["84"] = "110 58 1"
FUdest_err["85"] = "56 26 1"
FUdest_err["86"] = "16 1173 0"
FUdest_err["87"] = "86 1957 0"
FUdest_err["88"] = "64 1612 1"
FUdest_err["89"] = "65 794 0"
FUdest_err["90"] = "17 1875 0"
FUdest_err["91"] = "86 1025 1"
FUdest_err["92"] = "63 740 1"
FUdest_err["93"] = "30 1230 1"
FUdest_err["94"] = "35 72 1"
FUdest_err["95"] = "64 873 0"
FUdest_err["96"] = "2 58 1"
FUdest_err["97"] = "56 19 0"
FUdest_err["98"] = "17 1574 1"
FUdest_err["99"] = "28 70 0"
FUdest_err["100"] = "9 55 0"
FUdest_err["101"] = "115 50 1"
FUdest_err["102"] = "10 2 1"
FUdest_err["103"] = "5 38 1"
FUdest_err["104"] = "14 1934 0"
FUdest_err["105"] = "3 16 0"
FUdest_err["106"] = "110 5 1"
FUdest_err["107"] = "115 25 1"
FUdest_err["108"] = "64 117 0"
FUdest_err["109"] = "34 1020 0"
FUdest_err["110"] = "164 34 0"
FUdest_err["111"] = "110 15 1"
FUdest_err["112"] = "60 36 0"
FUdest_err["113"] = "110 14 0"
FUdest_err["114"] = "126 39 1"
FUdest_err["115"] = "2 9 1"
FUdest_err["116"] = "30 433 0"
FUdest_err["117"] = "63 1408 1"
FUdest_err["118"] = "4 40 1"
FUdest_err["119"] = "29 22 1"
FUdest_err["120"] = "14 1209 1"
FUdest_err["121"] = "5 52 0"
FUdest_err["122"] = "3 40 1"
FUdest_err["123"] = "2 56 1"
FUdest_err["124"] = "34 1730 1"
FUdest_err["125"] = "5 7 0"
FUdest_err["126"] = "17 1301 0"
FUdest_err["127"] = "2 41 1"
FUdest_err["128"] = "3 56 1"
FUdest_err["129"] = "110 53 1"
FUdest_err["130"] = "35 1687 0"
FUdest_err["131"] = "29 0 0"
FUdest_err["132"] = "21 1355 0"
FUdest_err["133"] = "3 48 1"
FUdest_err["134"] = "15 510 0"
FUdest_err["135"] = "72 31 0"
FUdest_err["136"] = "164 30 1"
FUdest_err["137"] = "4 29 0"
FUdest_err["138"] = "110 22 1"
FUdest_err["139"] = "110 44 1"
FUdest_err["140"] = "14 1083 0"
FUdest_err["141"] = "14 1750 1"
FUdest_err["142"] = "4 53 1"
FUdest_err["143"] = "72 744 0"
FUdest_err["144"] = "164 61 1"
FUdest_err["145"] = "21 1516 1"
FUdest_err["146"] = "35 0 1"
FUdest_err["147"] = "7 129 0"
FUdest_err["148"] = "2 20 0"
FUdest_err["149"] = "3 52 1"
FUdest_err["150"] = "3 43 0"
FUdest_err["151"] = "5 10 1"
FUdest_err["152"] = "86 1934 0"
FUdest_err["153"] = "2 45 1"
FUdest_err["154"] = "3 13 0"
FUdest_err["155"] = "29 48 1"
FUdest_err["156"] = "164 40 1"
FUdest_err["157"] = "56 17 1"
FUdest_err["158"] = "126 59 1"
FUdest_err["159"] = "110 33 1"
FUdest_err["160"] = "164 5 1"
FUdest_err["161"] = "15 679 0"
FUdest_err["162"] = "11 229 0"
FUdest_err["163"] = "34 417 0"
FUdest_err["164"] = "164 52 1"
FUdest_err["165"] = "83 735 0"
FUdest_err["166"] = "86 1314 0"
FUdest_err["167"] = "126 54 0"
FUdest_err["168"] = "222 22 1"
FUdest_err["169"] = "10 16 1"
FUdest_err["170"] = "10 37 1"
FUdest_err["171"] = "13 906 0"
FUdest_err["172"] = "164 33 1"
FUdest_err["173"] = "3 11 0"
FUdest_err["174"] = "56 39 0"
FUdest_err["175"] = "56 20 1"
FUdest_err["176"] = "30 1018 0"
FUdest_err["177"] = "86 13 1"
FUdest_err["178"] = "13 1732 1"
FUdest_err["179"] = "17 392 1"
FUdest_err["180"] = "276 1782 0"
FUdest_err["181"] = "3 37 1"
FUdest_err["182"] = "65 1058 1"
FUdest_err["183"] = "3 8 1"
FUdest_err["184"] = "2 30 1"
FUdest_err["185"] = "3 1 0"
FUdest_err["186"] = "110 23 1"
FUdest_err["187"] = "164 16 1"
FUdest_err["188"] = "3 45 1"
FUdest_err["189"] = "3 8 0"
FUdest_err["190"] = "16 153 0"
FUdest_err["191"] = "126 53 0"
FUdest_err["192"] = "28 988 0"
FUdest_err["193"] = "17 170 1"
FUdest_err["194"] = "64 1548 0"
FUdest_err["195"] = "3 46 0"
FUdest_err["196"] = "115 35 0"
FUdest_err["197"] = "3 59 0"
FUdest_err["198"] = "16 1994 0"
FUdest_err["199"] = "65 1491 1"
FUdest_err["200"] = "2 18 1"
FUdest_err["201"] = "3 62 0"
FUdest_err["202"] = "164 30 0"
FUdest_err["203"] = "62 1685 1"
FUdest_err["204"] = "59 1069 1"
FUdest_err["205"] = "3 53 0"
FUdest_err["206"] = "3 39 1"
FUdest_err["207"] = "3 46 1"
FUdest_err["208"] = "17 1240 0"
FUdest_err["209"] = "16 929 0"
FUdest_err["210"] = "222 17 0"
FUdest_err["211"] = "110 49 0"
FUdest_err["212"] = "17 742 1"
FUdest_err["213"] = "3 33 1"
FUdest_err["214"] = "28 1992 1"
FUdest_err["215"] = "110 7 1"
FUdest_err["216"] = "2 37 0"
FUdest_err["217"] = "3 19 0"
FUdest_err["218"] = "3 51 1"
FUdest_err["219"] = "64 1412 0"
FUdest_err["220"] = "3 15 1"
FUdest_err["221"] = "2 10 0"
FUdest_err["222"] = "17 217 0"
FUdest_err["223"] = "16 902 0"
FUdest_err["224"] = "2 55 1"
FUdest_err["225"] = "9 27 0"
FUdest_err["226"] = "21 1909 0"
FUdest_err["227"] = "28 1749 0"
FUdest_err["228"] = "3 25 1"
FUdest_err["229"] = "56 27 1"
FUdest_err["230"] = "17 482 1"
FUdest_err["231"] = "110 4 1"
FUdest_err["232"] = "35 1694 1"
FUdest_err["233"] = "29 50 1"
FUdest_err["234"] = "64 164 1"
FUdest_err["235"] = "3 38 0"
FUdest_err["236"] = "28 1652 1"
FUdest_err["237"] = "2 52 0"
FUdest_err["238"] = "3 16 1"
FUdest_err["239"] = "2 25 0"
FUdest_err["240"] = "110 0 0"
FUdest_err["241"] = "3 38 1"
FUdest_err["242"] = "56 28 1"
FUdest_err["243"] = "3 34 1"
FUdest_err["244"] = "5 49 0"
FUdest_err["245"] = "56 15 0"
FUdest_err["246"] = "15 1854 0"
FUdest_err["247"] = "164 39 1"
FUdest_err["248"] = "110 57 1"
FUdest_err["249"] = "3 28 0"
FUdest_err["250"] = "126 57 0"
FUdest_err["251"] = "164 1 1"
FUdest_err["252"] = "16 847 1"
FUdest_err["253"] = "62 1177 0"
FUdest_err["254"] = "3 41 0"
FUdest_err["255"] = "3 25 0"
FUdest_err["256"] = "3 58 1"
FUdest_err["257"] = "56 29 1"
FUdest_err["258"] = "35 184 0"
FUdest_err["259"] = "164 0 0"
FUdest_err["260"] = "28 1045 0"
FUdest_err["261"] = "56 0 1"
FUdest_err["262"] = "15 1046 1"
FUdest_err["263"] = "110 50 0"
FUdest_err["264"] = "3 24 0"
FUdest_err["265"] = "28 1389 0"
FUdest_err["266"] = "10 32 0"
FUdest_err["267"] = "2 47 0"
FUdest_err["268"] = "15 1740 1"
FUdest_err["269"] = "29 43 1"
FUdest_err["270"] = "21 273 0"
FUdest_err["271"] = "3 61 0"
FUdest_err["272"] = "32 51 0"
FUdest_err["273"] = "3 19 1"
FUdest_err["274"] = "34 868 1"
FUdest_err["275"] = "56 5 0"
FUdest_err["276"] = "5 4 0"
FUdest_err["277"] = "63 1848 1"
FUdest_err["278"] = "164 21 0"
FUdest_err["279"] = "2 42 1"
FUdest_err["280"] = "15 1170 1"
FUdest_err["281"] = "7 788 1"
FUdest_err["282"] = "16 873 1"
FUdest_err["283"] = "15 1719 1"
FUdest_err["284"] = "34 895 1"
FUdest_err["285"] = "56 32 1"
FUdest_err["286"] = "110 19 1"
FUdest_err["287"] = "3 31 1"
FUdest_err["288"] = "30 1269 1"
FUdest_err["289"] = "17 692 0"
FUdest_err["290"] = "10 4 0"
FUdest_err["291"] = "21 605 1"
FUdest_err["292"] = "34 50 1"
FUdest_err["293"] = "13 1017 1"
FUdest_err["294"] = "60 52 0"
FUdest_err["295"] = "3 27 0"
FUdest_err["296"] = "3 54 1"
FUdest_err["297"] = "7 64 0"
FUdest_err["298"] = "126 1 0"
FUdest_err["299"] = "61 1450 0"
FUdest_err["300"] = "3 44 1"
FUdest_err["301"] = "3 49 1"
FUdest_err["302"] = "34 1825 0"
FUdest_err["303"] = "3 26 1"
FUdest_err["304"] = "164 13 1"
FUdest_err["305"] = "35 556 0"
FUdest_err["306"] = "2 60 0"
FUdest_err["307"] = "10 13 1"
FUdest_err["308"] = "34 1876 1"
FUdest_err["309"] = "276 1143 0"
FUdest_err["310"] = "110 61 0"
FUdest_err["311"] = "56 27 0"
FUdest_err["312"] = "34 517 1"
FUdest_err["313"] = "72 350 1"
FUdest_err["314"] = "56 63 1"
FUdest_err["315"] = "3 39 0"
FUdest_err["316"] = "16 708 0"
FUdest_err["317"] = "2 14 1"
FUdest_err["318"] = "3 2 0"
FUdest_err["319"] = "30 1009 0"
FUdest_err["320"] = "35 1851 1"
FUdest_err["321"] = "110 50 1"
FUdest_err["322"] = "5 44 0"
FUdest_err["323"] = "2 63 0"
FUdest_err["324"] = "2 0 0"
FUdest_err["325"] = "63 1658 1"
FUdest_err["326"] = "126 38 1"
FUdest_err["327"] = "4 19 1"
FUdest_err["328"] = "28 1231 1"
FUdest_err["329"] = "56 16 1"
FUdest_err["330"] = "168 45 0"
FUdest_err["331"] = "3 14 0"
FUdest_err["332"] = "2 45 0"
FUdest_err["333"] = "17 869 0"
FUdest_err["334"] = "56 48 1"
FUdest_err["335"] = "222 10 1"
FUdest_err["336"] = "10 10 1"
FUdest_err["337"] = "62 762 1"
FUdest_err["338"] = "35 1317 1"
FUdest_err["339"] = "63 1378 0"
FUdest_err["340"] = "164 9 0"
FUdest_err["341"] = "126 40 1"
FUdest_err["342"] = "13 2040 0"
FUdest_err["343"] = "7 1054 0"
FUdest_err["344"] = "29 53 0"
FUdest_err["345"] = "9 44 0"
FUdest_err["346"] = "21 1134 0"
FUdest_err["347"] = "2 37 1"
FUdest_err["348"] = "2 28 1"
FUdest_err["349"] = "3 35 1"
FUdest_err["350"] = "30 205 0"
FUdest_err["351"] = "17 880 0"
FUdest_err["352"] = "4 12 1"
FUdest_err["353"] = "35 555 1"
FUdest_err["354"] = "64 979 1"
FUdest_err["355"] = "3 37 0"
FUdest_err["356"] = "110 49 1"
FUdest_err["357"] = "115 50 0"
FUdest_err["358"] = "3 27 1"
FUdest_err["359"] = "164 44 1"
FUdest_err["360"] = "56 22 0"
FUdest_err["361"] = "13 1276 0"
FUdest_err["362"] = "28 1707 1"
FUdest_err["363"] = "110 36 1"
FUdest_err["364"] = "48 51 0"
FUdest_err["365"] = "48 21 0"
FUdest_err["366"] = "49 32 0"
FUdest_err["367"] = "49 55 1"
FUdest_err["368"] = "49 58 0"
FUdest_err["369"] = "49 40 1"
FUdest_err["370"] = "48 15 1"
FUdest_err["371"] = "48 2 1"
FUdest_err["372"] = "49 61 0"
FUdest_err["373"] = "49 28 0"
FUdest_err["374"] = "49 16 0"
FUdest_err["375"] = "48 17 1"
FUdest_err["376"] = "49 32 1"
FUdest_err["377"] = "48 60 0"
FUdest_err["378"] = "48 52 0"
FUdest_err["379"] = "49 39 0"
FUdest_err["380"] = "49 2 1"
FUdest_err["381"] = "49 27 0"
FUdest_err["382"] = "49 44 0"
FUdest_err["383"] = "48 42 0"
FUdest_err["384"] = "48 3 1"
FUdest_err["385"] = "48 54 1"
FUdest_err["386"] = "48 43 0"
FUdest_err["387"] = "49 40 0"
FUdest_err["388"] = "48 12 1"
FUdest_err["389"] = "48 11 1"
FUdest_err["390"] = "48 33 1"
FUdest_err["391"] = "48 55 0"
FUdest_err["392"] = "48 53 1"
FUdest_err["393"] = "49 8 0"
FUdest_err["394"] = "48 62 1"
FUdest_err["395"] = "49 3 0"
FUdest_err["396"] = "49 2 0"
FUdest_err["397"] = "49 43 0"
FUdest_err["398"] = "49 9 0"
FUdest_err["399"] = "49 58 1"
FUdest_err["400"] = "48 22 1"
FUdest_err["401"] = "48 27 1"
FUdest_err["402"] = "49 13 1"
FUdest_err["403"] = "49 56 0"
FUdest_err["404"] = "49 19 1"
FUdest_err["405"] = "48 57 0"

with open(args.detect_file) as f:
    for line in f:
        bench, errInjType, errInjStr = parse_bench_errInj(line)
        errInjSet_detected[errInjType].add(errInjStr)
        errInjDict[bench]["Detect"].add(errInjStr)

with open(args.inject_file) as f:
    for line in f:
        bench, errInjType, errInjStr = parse_bench_errInj(line)
        errInjDict[bench]["Injected"].add(errInjStr)

if args.missed_file:
    with open(args.missed_file) as f:
        for line in f:
            bench, errInjType, errInjStr = parse_bench_errInj(line)
            errInjDict[bench]["Missed"].add(errInjStr)
    inject_set_accross_benchs = set()
    for bench in benchmarks:
        inject_set_accross_benchs = inject_set_accross_benchs | errInjDict[bench]["Injected"]
        assert(errInjDict[bench]["Injected"] >= errInjDict[bench]["Detect"] and errInjDict[bench]["Injected"] >= errInjDict[bench]["Missed"])
        errInjDict[bench]["ToRun"] = errInjDict[bench]["Injected"] - errInjDict[bench]["Detect"] - errInjDict[bench]["Missed"]
    inject_list_accross_benchs = sorted(list(inject_set_accross_benchs))
    errStr2Id_dict = dict()
    for id,s in enumerate(inject_list_accross_benchs):
        print(str(id+1) + ": FUdest " + s)
        errStr2Id_dict[s] = id+1
    errInjDict_ID = dict()
    for bench in benchmarks:
        errInjDict_ID[bench] = sorted(list(errStr2Id_dict[s] for s in errInjDict[bench]["ToRun"]))
    print("\n".join([bench + ": " + "".join([(str(id) + " ") for id in errInjDict_ID[bench]]) for bench in benchmarks]))
    count_total = 0
    for i in range(len(inject_list_accross_benchs)):
        local_total = 0
        for bench in benchmarks:
            if i+1 in errInjDict_ID[bench]:
                local_total += 1
        if count_total + local_total >= 80:
            print((count_total),i)
            count_total = local_total
        else:
            count_total += local_total
    print((count_total),len(inject_list_accross_benchs))
    print(sum([len(errInjDict[bench]["ToRun"]) for bench in benchmarks]))
elif args.masked_file:
    with open(args.masked_file) as f:
        for line in f:
            bench, errInjType, errInjStr = parse_bench_errInj(line)
            errInjDict[bench]["Masked"].add(errInjStr)

    for benchmark in benchmarks:
        errInjDict[bench]["Unchanged"] = errInjDict[bench]["Injected"] - errInjDict[bench]["Masked"]

    errInjList_detected = dict()
    errInjList_detected["FUdest"] = sorted(list(errInjSet_detected["FUdest"]))
    inject_set_accross_benchs = set()
    for bench in benchmarks:
        inject_set_accross_benchs = inject_set_accross_benchs | errInjDict[bench]["Injected"]
        assert(errInjDict[bench]["Injected"] >= errInjDict[bench]["Detect"])
    errInjList_injected = dict()
    errInjList_injected["FUdest"] = sorted(list(inject_set_accross_benchs))

    for errType, errList in errInjList_injected.items():
        print("\n".join([str(i+1) + ": " + errType + " " + errStr for i,errStr in enumerate(errList)]))
        total_runs = 0
        for bench in benchmarks:
            print(bench + ": " + "".join([((str(i+1) + " ") if errStr in errInjDict[bench]["Detect"] else "") for i,errStr in enumerate(errList)]))
            total_runs += len(errInjDict[bench]["Detect"])
        count_total = 0
        for i in range(len(errList)):
            local_total = 0
            for bench in benchmarks:
                if errList[i] in errInjDict[bench]["Detect"]:
                    local_total += 1
            if count_total + local_total >= args.max_cores:
                print((count_total),i)
                count_total = local_total
            else:
                count_total += local_total
        print((count_total),len(errList))