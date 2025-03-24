from ruby import CHI_config

# CustomMesh parameters for a 3x5 mesh. Routers will have the following layout:
#
# 0 --- 1 --- 2 --- 3 --- 4
# |     |     |     |     |
# 5 --- 6 --- 7 --- 8 --- 9
# |     |     |     |     |
# 10 -- 11 -- 12 -- 13 -- 14
#
# Default parameter are configs/ruby/CHI_config.py
#

# Run with these options
# -n 6 --num-main-cores=2 --num-dirs 4 (4 MC)
# --topology=CustomMesh --chi-config=configs/example/noc_config/3x5_2_o3_4_checker.py

class NoC_Params(CHI_config.NoC_Params):
    num_rows = 3
    num_cols = 5

# Specialization of nodes to define bindings for each CHI node type
# needed by CustomMesh.
# The default types are defined in CHI_Node and their derivatives in
# configs/ruby/CHI_config.py

class CHI_RNF(CHI_config.CHI_RNF):
    class NoC_Params(CHI_config.CHI_RNF.NoC_Params):
        # O3 at routers 2/6
        router_list = [2, 6]
        num_nodes_per_router = 1
        create_router = False

class CHI_HNF(CHI_config.CHI_HNF):
    class NoC_Params(CHI_config.CHI_HNF.NoC_Params):
        router_list = [7]
        num_nodes_per_router = 1

class CHI_MN(CHI_config.CHI_MN):
    class NoC_Params(CHI_config.CHI_MN.NoC_Params):
        router_list = [3]
        num_nodes_per_router = 1

class CHI_SNF_MainMem(CHI_config.CHI_SNF_MainMem):
    class NoC_Params(CHI_config.CHI_SNF_MainMem.NoC_Params):
        router_list = [0, 4, 11, 14]
        num_nodes_per_router = 1

class CHI_SNF_BootMem(CHI_config.CHI_SNF_BootMem):
    class NoC_Params(CHI_config.CHI_SNF_BootMem.NoC_Params):
        router_list = [10]
        num_nodes_per_router = 1

class CHI_RNI_DMA(CHI_config.CHI_RNI_DMA):
    class NoC_Params(CHI_config.CHI_RNI_DMA.NoC_Params):
        router_list = [9]
        num_nodes_per_router = 1

class CHI_RNI_IO(CHI_config.CHI_RNI_IO):
    class NoC_Params(CHI_config.CHI_RNI_IO.NoC_Params):
        router_list = [5]
        num_nodes_per_router = 1
