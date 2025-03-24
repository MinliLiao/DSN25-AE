import sys
import argparse
import math

parser = argparse.ArgumentParser(description='Collect statistics')
parser.add_argument('--stat', type=str, action='store', required=True,
                    help='the name of the statstics from input')
parser.add_argument('--config', type=str, required=True,
                    choices=['1B_spec17','1Bstored_spec17','1Bbreakdown_spec17',"1Bmcpat_spec17",'100M_spec17', "100M_spec17_AE","100MslowNoC_spec17", "100MslowNoC_spec17_AE","100MstaticCov_spec17","1B_gapbs","tillEnd_parsec","NoCLat_spec17", "NoCLat_gapbs","NoCLat_parsec", "NoCLat_4pspec17"],
                    help='configs to use from input')
parser.add_argument('--agg', type=str, choices=['average','geomean'],
                    help='add aggregate stats')
parser.add_argument("--all", action='store_true',
                    help='get statistics from all lines, not just for main core')
parser.add_argument("--checker", action='store_true',
                    help='statistics of checker cores, no baseline')
parser.add_argument("--dummy", type=str, action='store',
                    help='fill in missing data with dummy')
parser.add_argument('--slowNoCEstimate', type=str, action='store',
                    help='csv files containing estimated slowNoC cycles')

args = parser.parse_args()
if args.config in ["1B_spec17",'1Bstored_spec17','1Bbreakdown_spec17',"1Bmcpat_spec17",'100M_spec17', "100M_spec17_AE","100MslowNoC_spec17", "100MslowNoC_spec17_AE","100MstaticCov_spec17","NoCLat_spec17"]:
    benchmarks = ["bwaves", "gcc", "mcf", "xalancbmk", "deepsjeng", "leela", "exchange2", "xz", "cactuBSSN", "lbm", "wrf", "cam4", "pop2", "imagick", "nab", "fotonik3d", "roms", "x264", "perlbench", "omnetpp"]
elif args.config in ["1B_gapbs", "NoCLat_gapbs"]:
    benchmarks = ["bc","bfs","cc","pr"]
elif args.config in ["NoCLat_parsec", "tillEnd_parsec"]:
    benchmarks = ["blackscholes", "bodytrack", "fluidanimate", "freqmine", "streamcluster", "swaptions"]
elif args.config in ["NoCLat_4pspec17"]:
    benchmarks = ["bwaves_gcc_mcf_deepsjeng", "leela_exchange2_xz_wrf", "xalancbmk_omnetpp_cactuBSSN_lbm", "cam4_imagick_nab_fotonik3d", "pop2_roms_perlbench_x264"]#, "bwaves_gcc_cactuBSSN_deepsjeng", "xalancbmk_omnetpp_mcf_lbm"]

if args.config in ["NoCLat_spec17"]:
    configs_dict = { # SPEC17 NoCLat calculation configs
                    "baseline_1_3GHz_stats.txt": "Baseline"
                   }
elif args.config in ["NoCLat_gapbs"]:
    configs_dict = { # GAPBS NoCLat calculation configs
                    "roi_baseline_1_3GHz_stats.txt": "Baseline"
                   }
elif args.config in ["NoCLat_parsec"]:
    configs_dict = { # PARSEC NoCLat calculation configs
                    "baseline_3_3GHz_stats.txt": "Baseline",
                    # bodytrack uses 4 cores for 2 threads
                    "baseline_4_3GHz_stats.txt": "Baseline",
                    # streamcluster uses 5 cores for 2 threads
                    "baseline_5_3GHz_stats.txt": "Baseline",
                }
elif args.config in ["NoCLat_4pspec17"]:
    configs_dict = { # 4-process 4-core SPEC NoCLat calculation configs
                    "baseline_4_3GHz_stats.txt": "Baseline",
                }

elif args.config in ["1B_spec17"]:
    configs_dict = { # SPEC17 1B result configs
                    "baseline_1_3GHz_stats.txt": "Baseline",
                    "checkedNoC_1_1_3GHz_2700MHz_X2_stats.txt" : "1*X2@2.7GHz",
                    "opportunisticNoC_1_1_3GHz_2700MHz_X2_stats.txt" : "1*X2@2.7GHz_opp",
                    "checkedNoC_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz",
                    "opportunisticNoC_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz_opp",
                    "checkedNoC_1_2_3GHz_1350MHz_X2_stats.txt": "2*X2@1.35GHz",
                    "opportunisticNoC_1_2_3GHz_1350MHz_X2_stats.txt": "2*X2@1.35GHz_opp",
                    "checkedNoC_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "opportunisticNoC_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz_opp",
                    "checkedNoC_1_4_3GHz_1600MHz_A510_stats.txt": "4*A510@1.6GHz",
                    "opportunisticNoC_1_4_3GHz_1600MHz_A510_stats.txt": "4*A510@1.6GHz_opp",
                    "checkedNoC_1_4_3GHz_1800MHz_A510_stats.txt": "4*A510@1.8GHz",
                    "opportunisticNoC_1_4_3GHz_1800MHz_A510_stats.txt": "4*A510@1.8GHz_opp",
                    "checkedNoC_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                    "opportunisticNoC_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz_opp",
                }
elif args.config in ["1Bmcpat_spec17"]:
    configs_dict = { # SPEC17 1B result configs
                    "baseline_1_3GHz_stats.txt": "Baseline",
                    "checkedNoC_1_1_3GHz_2700MHz_X2_stats.txt" : "1*X2@2.7GHz",
                    "checkedNoC_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz",
                    "checkedNoC_1_2_3GHz_1350MHz_X2_stats.txt": "2*X2@1.35GHz",
                    "checkedNoC_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "checkedNoC_1_4_3GHz_1400MHz_A510_stats.txt": "4*A510@1.4GHz",
                    "checkedNoC_1_4_3GHz_1600MHz_A510_stats.txt": "4*A510@1.6GHz",
                    "checkedNoC_1_4_3GHz_1800MHz_A510_stats.txt": "4*A510@1.8GHz",
                    "checkedNoC_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                }
elif args.config in ["1B_gapbs"]:
    configs_dict = { # GAPBS 1B result configs
                    "roi_baseline_1_3GHz_stats.txt": "Baseline",
                    "roi_checkedNoC_1_1_3GHz_1500MHz_X2_stats.txt" : "1*X2@1.5GHz",
                    "roi_checkedNoC_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "roi_checkedNoC_1_1_3GHz_2000MHz_A510_stats.txt": "1*A510@2GHz",
                    "roi_checkedNoC_1_2_3GHz_2000MHz_A510_stats.txt": "2*A510@2GHz",
                    "roi_checkedNoC_1_3_3GHz_2000MHz_A510_stats.txt": "3*A510@2GHz",
                    "roi_checkedNoC_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                }
elif args.config in ["tillEnd_parsec"]:
    configs_dict = { # PARSEC run till end result configs
                    "baseline_3_3GHz_stats.txt": "Baseline",
                    "checkedNoC_3_1_3GHz_1500MHz_X2_stats.txt": "1*X2@1.5GHz",
                    "checkedNoC_3_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "checkedNoC_3_1_3GHz_2000MHz_A510_stats.txt": "1*A510@2GHz",
                    "checkedNoC_3_2_3GHz_2000MHz_A510_stats.txt": "2*A510@2GHz",
                    "checkedNoC_3_3_3GHz_2000MHz_A510_stats.txt": "3*A510@2GHz",
                    "checkedNoC_3_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                    # bodytrack uses 4 cores for 2 threads
                    "baseline_4_3GHz_stats.txt": "Baseline",
                    "checkedNoC_4_1_3GHz_1500MHz_X2_stats.txt": "1*X2@1.5GHz",
                    "checkedNoC_4_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "checkedNoC_4_1_3GHz_2000MHz_A510_stats.txt": "1*A510@2GHz",
                    "checkedNoC_4_2_3GHz_2000MHz_A510_stats.txt": "2*A510@2GHz",
                    "checkedNoC_4_3_3GHz_2000MHz_A510_stats.txt": "3*A510@2GHz",
                    "checkedNoC_4_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                    # streamcluster uses 5 cores for 2 threads
                    "baseline_5_3GHz_stats.txt": "Baseline",
                    "checkedNoC_5_1_3GHz_1500MHz_X2_stats.txt": "1*X2@1.5GHz",
                    "checkedNoC_5_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "checkedNoC_5_1_3GHz_2000MHz_A510_stats.txt": "1*A510@2GHz",
                    "checkedNoC_5_2_3GHz_2000MHz_A510_stats.txt": "2*A510@2GHz",
                    "checkedNoC_5_3_3GHz_2000MHz_A510_stats.txt": "3*A510@2GHz",
                    "checkedNoC_5_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                }
elif args.config in ["1Bstored_spec17"]:
    configs_dict = { # SPEC17 1B stored breakdown result configs
                     # No difference between different checker core configs
                     # Values from 1 file is copied to all checker core configs
                    "baseline_1_3GHz_stats.txt": "Baseline",
                    "dummy_0" : "1*X2@2.7GHz",
                    "dummy_1" : "1*X2@3GHz",
                    "dummy_2": "2*X2@1.35GHz",
                    "dummy_3": "2*X2@1.5GHz",
                    "dummy_4": "4*A510@1.6GHz",
                    "dummy_5": "4*A510@1.8GHz",
                    "stored_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                }
elif args.config in ["1Bbreakdown_spec17"]:
    configs_dict = { # SPEC17 1B result configs without NoC latency
                    "baseline_1_3GHz_stats.txt": "Baseline",
                    "checked_1_1_3GHz_2700MHz_X2_stats.txt" : "1*X2@2.7GHz",
                    "opportunistic_1_1_3GHz_2700MHz_X2_stats.txt" : "1*X2@2.7GHz_opp",
                    "checked_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz",
                    "opportunistic_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz_opp",
                    "checked_1_2_3GHz_1350MHz_X2_stats.txt": "2*X2@1.35GHz",
                    "opportunistic_1_2_3GHz_1350MHz_X2_stats.txt": "2*X2@1.35GHz_opp",
                    "checked_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "opportunistic_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz_opp",
                    "checked_1_4_3GHz_1600MHz_A510_stats.txt": "4*A510@1.6GHz",
                    "opportunistic_1_4_3GHz_1600MHz_A510_stats.txt": "4*A510@1.6GHz_opp",
                    "checked_1_4_3GHz_1800MHz_A510_stats.txt": "4*A510@1.8GHz",
                    "opportunistic_1_4_3GHz_1800MHz_A510_stats.txt": "4*A510@1.8GHz_opp",
                    "checked_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                    "opportunistic_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz_opp",
                }
elif args.config in ["100M_spec17"]:
    configs_dict = { # SPEC17 100M checker core frequency/count sweep
                    "baseline_1_3GHz_stats.txt": "Baseline",
                    "checkedNoC_1_1_3GHz_1500MHz_X2_stats.txt" : "1*X2@1.5GHz",
                    "opportunisticNoC_1_1_3GHz_1500MHz_X2_stats.txt" : "1*X2@1.5GHz_opp",
                    "checkedNoC_1_1_3GHz_1800MHz_X2_stats.txt" : "1*X2@1.8GHz",
                    "opportunisticNoC_1_1_3GHz_1800MHz_X2_stats.txt" : "1*X2@1.8GHz_opp",
                    "checkedNoC_1_1_3GHz_2100MHz_X2_stats.txt" : "1*X2@2.1GHz",
                    "opportunisticNoC_1_1_3GHz_2100MHz_X2_stats.txt" : "1*X2@2.1GHz_opp",
                    "checkedNoC_1_1_3GHz_2400MHz_X2_stats.txt" : "1*X2@2.4GHz",
                    "opportunisticNoC_1_1_3GHz_2400MHz_X2_stats.txt" : "1*X2@2.4GHz_opp",
                    "checkedNoC_1_1_3GHz_2700MHz_X2_stats.txt" : "1*X2@2.7GHz",
                    "opportunisticNoC_1_1_3GHz_2700MHz_X2_stats.txt" : "1*X2@2.7GHz_opp",
                    "checkedNoC_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz",
                    "opportunisticNoC_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz_opp",
                    "checkedNoC_1_2_3GHz_750MHz_X2_stats.txt": "2*X2@0.75GHz",
                    "opportunisticNoC_1_2_3GHz_750MHz_X2_stats.txt": "2*X2@0.75GHz_opp",
                    "checkedNoC_1_2_3GHz_900MHz_X2_stats.txt": "2*X2@0.9GHz",
                    "opportunisticNoC_1_2_3GHz_900MHz_X2_stats.txt": "2*X2@0.9GHz_opp",
                    "checkedNoC_1_2_3GHz_1050MHz_X2_stats.txt": "2*X2@1.05GHz",
                    "opportunisticNoC_1_2_3GHz_1050MHz_X2_stats.txt": "2*X2@1.05GHz_opp",
                    "checkedNoC_1_2_3GHz_1200MHz_X2_stats.txt": "2*X2@1.2GHz",
                    "opportunisticNoC_1_2_3GHz_1200MHz_X2_stats.txt": "2*X2@1.2GHz_opp",
                    "checkedNoC_1_2_3GHz_1350MHz_X2_stats.txt": "2*X2@1.35GHz",
                    "opportunisticNoC_1_2_3GHz_1350MHz_X2_stats.txt": "2*X2@1.35GHz_opp",
                    "checkedNoC_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "opportunisticNoC_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz_opp",
                    "checkedNoC_1_4_3GHz_1000MHz_A510_stats.txt": "4*A510@1GHz",
                    "opportunisticNoC_1_4_3GHz_1000MHz_A510_stats.txt": "4*A510@1GHz_opp",
                    "checkedNoC_1_4_3GHz_1200MHz_A510_stats.txt": "4*A510@1.2GHz",
                    "opportunisticNoC_1_4_3GHz_1200MHz_A510_stats.txt": "4*A510@1.2GHz_opp",
                    "checkedNoC_1_4_3GHz_1400MHz_A510_stats.txt": "4*A510@1.4GHz",
                    "opportunisticNoC_1_4_3GHz_1400MHz_A510_stats.txt": "4*A510@1.4GHz_opp",
                    "checkedNoC_1_4_3GHz_1600MHz_A510_stats.txt": "4*A510@1.6GHz",
                    "opportunisticNoC_1_4_3GHz_1600MHz_A510_stats.txt": "4*A510@1.6GHz_opp",
                    "checkedNoC_1_4_3GHz_1800MHz_A510_stats.txt": "4*A510@1.8GHz",
                    "opportunisticNoC_1_4_3GHz_1800MHz_A510_stats.txt": "4*A510@1.8GHz_opp",
                    "checkedNoC_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                    "opportunisticNoC_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz_opp",
                }
elif args.config in ["100M_spec17_AE"]:
    configs_dict = { # SPEC17 100M checker core frequency/count sweep
                    "baseline_1_3GHz_stats.txt": "Baseline",
                    "checked_1_12_3GHz_1000MHz_DSN18A55_stats.txt" : "DSN18",
                    "checked_1_16_3GHz_1000MHz_ParadoxA55_stats.txt" : "Paradox",
                    "checkedNoC4x4o_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz",
                    "checkedNoC4x4o_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "checkedNoC4x4o_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                    "checkedNoC4x4o_1_4_3GHz_minED2P_A510_stats.txt": "4*A510_minED2P",
                    "opportunisticNoC4x4o_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz_opp",
                    "opportunisticNoC4x4o_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz_opp",
                    "opportunisticNoC4x4o_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz_opp",
    }
elif args.config in ["100MslowNoC_spec17_AE"]:
    configs_dict = { # SPEC17 100M checker core frequency/count sweep
                    "baseline_1_3GHz_stats.txt": "Baseline",
                    "checked_1_1_3GHz_3000MHz_X2_stats.txt" : "checked_1*X2@3GHz",
                    "checked_1_2_3GHz_1500MHz_X2_stats.txt": "checked_2*X2@1.5GHz",
                    "checked_1_4_3GHz_2000MHz_A510_stats.txt": "checked_4*A510@2GHz",
                    "checkedNoC4x4o_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz",
                    "checkedNoC4x4o_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "checkedNoC4x4o_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                    "checkedHashSlowNoC4x4o_1_1_3GHz_3000MHz_X2_stats.txt" : "hashed_1*X2@3GHz_slowNoC",
                    "checkedHashSlowNoC4x4o_1_2_3GHz_1500MHz_X2_stats.txt": "hashed_2*X2@1.5GHz_slowNoC",
                    "checkedHashSlowNoC4x4o_1_4_3GHz_2000MHz_A510_stats.txt": "hashed_4*A510@2GHz_slowNoC",
                    "checkedSlowNoC4x4o_1_1_3GHz_3000MHz_X2_stats.txt" : "slow_1*X2@3GHz",
                    "checkedSlowNoC4x4o_1_2_3GHz_1500MHz_X2_stats.txt": "slow_2*X2@1.5GHz",
                    "checkedSlowNoC4x4o_1_4_3GHz_2000MHz_A510_stats.txt": "slow_4*A510@2GHz",
                }
    if not args.slowNoCEstimate:
        print("slowNoC estimate file is required for slowNoC stats gathering")
        sys.exit()
elif args.config in ["100MslowNoC_spec17"]:
    configs_dict = { # SPEC17 100M checker core frequency/count sweep
                    "baseline_1_3GHz_stats.txt": "Baseline",
                    "checkedNoC_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz",
                    "checkedSlowNoC_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz_slowNoC",
                    "checkedHashSlowNoC_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz_slowNoC_hashed",
                    "checkedNoC_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz",
                    "checkedSlowNoC_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz_slowNoC",
                    "checkedHashSlowNoC_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz_slowNoC_hashed",
                    "checkedNoC_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz",
                    "checkedSlowNoC_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz_slowNoC",
                    "checkedHashSlowNoC_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz_slowNoC_hashed",
                }
    if not args.slowNoCEstimate:
        print("slowNoC estimate file is required for slowNoC stats gathering")
        sys.exit()
elif args.config in ["100MstaticCov_spec17"]:
    configs_dict = { # 100M results used for static coverage analysis (only opportunistic mode)
                    "opportunisticNoC_1_1_3GHz_1500MHz_X2_stats.txt" : "1*X2@1.5GHz_opp",
                    "opportunisticNoC_1_1_3GHz_1800MHz_X2_stats.txt" : "1*X2@1.8GHz_opp",
                    "opportunisticNoC_1_1_3GHz_2100MHz_X2_stats.txt" : "1*X2@2.1GHz_opp",
                    "opportunisticNoC_1_1_3GHz_2400MHz_X2_stats.txt" : "1*X2@2.4GHz_opp",
                    "opportunisticNoC_1_1_3GHz_2700MHz_X2_stats.txt" : "1*X2@2.7GHz_opp",
                    "opportunisticNoC_1_1_3GHz_3000MHz_X2_stats.txt" : "1*X2@3GHz_opp",
                    "opportunisticNoC_1_2_3GHz_750MHz_X2_stats.txt": "2*X2@0.75GHz_opp",
                    "opportunisticNoC_1_2_3GHz_900MHz_X2_stats.txt": "2*X2@0.9GHz_opp",
                    "opportunisticNoC_1_2_3GHz_1050MHz_X2_stats.txt": "2*X2@1.05GHz_opp",
                    "opportunisticNoC_1_2_3GHz_1200MHz_X2_stats.txt": "2*X2@1.2GHz_opp",
                    "opportunisticNoC_1_2_3GHz_1350MHz_X2_stats.txt": "2*X2@1.35GHz_opp",
                    "opportunisticNoC_1_2_3GHz_1500MHz_X2_stats.txt": "2*X2@1.5GHz_opp",
                    "opportunisticNoC_1_4_3GHz_1000MHz_A510_stats.txt": "4*A510@1GHz_opp",
                    "opportunisticNoC_1_4_3GHz_1200MHz_A510_stats.txt": "4*A510@1.2GHz_opp",
                    "opportunisticNoC_1_4_3GHz_1400MHz_A510_stats.txt": "4*A510@1.4GHz_opp",
                    "opportunisticNoC_1_4_3GHz_1600MHz_A510_stats.txt": "4*A510@1.6GHz_opp",
                    "opportunisticNoC_1_4_3GHz_1800MHz_A510_stats.txt": "4*A510@1.8GHz_opp",
                    "opportunisticNoC_1_4_3GHz_2000MHz_A510_stats.txt": "4*A510@2GHz_opp",
                }

def parse_bench_name(line):
    if line.split(":")[0].split("/")[-1] in ["stats.txt"]:
        if args.config in ["NoCLat_4pspec17"]:
            return "_".join(line.split(":")[0].split("/")[-2].split("_")[:4])
        return line.split(":")[0].split("/")[-2].split("_")[0]
    if args.config in ["NoCLat_4pspec17"]:
        return "_".join(line.split(":")[0].split("/")[-1].split("_")[:4])
    return line.split(":")[0].split("/")[-1].split("_")[0]

def parse_config_name(line):
    try:
        if line.split(":")[0].split("/")[-1] in ["stats.txt"]:
            if args.config in ["NoCLat_4pspec17"]:
                return configs_dict["_".join(line.split(":")[0].split("/")[-2].split("_")[4:])+"_stats.txt"]
            return configs_dict["_".join(line.split(":")[0].split("/")[-2].split("_")[1:])+"/stats.txt"]
        if args.config in ["NoCLat_4pspec17"]:
            return configs_dict["_".join(line.split(":")[0].split("/")[-1].split("_")[4:])]
        return configs_dict["_".join(line.split(":")[0].split("/")[-1].split("_")[1:])]
    except:
        return ""

def parse_stat_name(line):
    return line.split(":")[1].split()[0]

def parse_stat_value(line):
    return eval(line.split("#")[0].split()[1])

def parse_csv(filename):
    stats = dict()
    with open(filename,'r') as f:
        first = True
        stat_name = ''
        benchmarks = []
        for line in f:
            if first:
                stats_name = line.replace("\n","").split(",")[0]
                benchmarks = line.replace("\n","").split(",")[1:]
                for benchmark in benchmarks:
                    stats[benchmark] = dict()
                first = False
            else:
                for i in range(len(benchmarks)):
                    stats[benchmarks[i]][line.split(",")[0]] = eval(line.replace("\n","").split(",")[i+1])
    return stats

def update_slowNoC(stats):
    slowNoCEstimate_stats = parse_csv(args.slowNoCEstimate)
    for benchmark in benchmarks:
        for config in stats[benchmark].keys():
            if "checkedSlowNoC" in config:
                if "4x4o" in config:
                    if "_1_1_" in config:
                        estimate = slowNoCEstimate_stats[benchmark]["4x4o1c"]
                    elif "_1_2_" in config:
                        estimate = slowNoCEstimate_stats[benchmark]["4x4o2c"]
                    elif "_1_4_" in config:
                        estimate = slowNoCEstimate_stats[benchmark]["4x4o4c"]
                elif "X2" in config:
                    estimate = slowNoCEstimate_stats[benchmark]["X2_slowNoC"]
                else:
                    estimate = slowNoCEstimate_stats[benchmark]["A510_slowNoC"]
                diff = stats[benchmark][config]/estimate
                if benchmark in ["exchange2", "perlbench"]:
                    assert diff > 0.975, "Estimate for " + benchmark + " with " + config + " is too low, not consistent with previous results"
                else:
                    assert diff > 0.975 and diff < 1.025, "Estimate for " + benchmark + " with " + config + " is too far from sim result, not consistent with previous results"
                stats[benchmark][config] = estimate

# Gather stats
def gather_stats():
    stats = dict()
    for benchmark in benchmarks:
        stats[benchmark] = dict()
        for config in configs_dict.values():
            if args.checker and config in ["Baseline"]:
                continue
            if args.dummy:
                stats[benchmark][config] = args.dummy
            else:    
                stats[benchmark][config] = ""

    configs = list(stats[benchmarks[0]].keys())

    for l in sys.stdin:
        if not args.all:
            if parse_config_name(l) in ["Baseline"]:
                if "switch_cpus" not in parse_stat_name(l):
                    continue
            elif "switch_cpus0" not in parse_stat_name(l):
                continue
        if args.checker and parse_config_name(l) in ["Baseline"]:
            continue
        if parse_bench_name(l) not in benchmarks or parse_config_name(l) not in configs:
            continue
        stats[parse_bench_name(l)][parse_config_name(l)] = parse_stat_value(l)
        if args.config in ["1Bstored_spec17"]:
            for config in configs:
                if config not in ["Baseline", parse_config_name(l)]:
                    stats[parse_bench_name(l)][config] = parse_stat_value(l)
    return stats

# Generate csv
stats = gather_stats()
if args.config in ["100MslowNoC_spec17", "100MslowNoC_spec17_AE"]:
    update_slowNoC(stats)
stat_matrix = []
line = [args.stat]
line.extend(benchmarks)
stat_matrix.append(line)
configs = list(stats[benchmarks[0]].keys())
for config in configs:
    if args.checker and config in ["Baseline"]:
        continue
    line = [config]
    line.extend([str(stats[benchmark][config]) for benchmark in benchmarks])
    stat_matrix.append(line)
stat_csv = "\n".join(",".join(line) for line in stat_matrix)
print(stat_csv)