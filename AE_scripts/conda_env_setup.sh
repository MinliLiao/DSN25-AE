# Get the system architecture name
ARCH_NAME=$(uname -p)
# Download miniconda installer
INSTALLER_NAME=Miniconda3-latest-Linux-${ARCH_NAME}.sh
wget https://repo.anaconda.com/miniconda/${INSTALLER_NAME}
# Install miniconda (follow prompt), see documentation at https://www.anaconda.com/docs/getting-started/miniconda/install#macos-linux-installation
bash ${INSTALLER_NAME} 
# Refresh terminal (if initialization options was YES, otherwise see above documentation)
source ~/.bashrc
# Create environment, see documentation at https://docs.conda.io/projects/conda/en/stable/user-guide/getting-started.html
ENV_NAME=ParaVerserAE
conda create -n ${ENV_NAME} python=3.8 scons
# Close and reopen terminal for installation to fully take effect, shoulde see (base) in command line prompt

# To activate environment, run the command below in terminal
# conda activate ${ENV_NAME}

# Add to LD_LIBRARY_PATH (run the commands below in terminal if libpython not found)
# PREFIX=~/miniconda3 # The default, please change if you installed to a different directory
# ENV_NAME=ParaVerserAE # The default in this script, change here as well if you've named the environment differently
# export LD_LIBRARY_PATH=${PREFIX}/envs/${ENV_NAME}/lib:${LD_LIBRARY_PATH}

# Remove environment, see documentation at https://docs.conda.io/projects/conda/en/stable/user-guide/tasks/manage-environments.html#removing-an-environment
# ENV_NAME=ParaVerserAE # The default in this script, change here as well if you've named the environment differently
# conda remove --name ${ENV_NAME} --all

# To uninstall miniconda, please refer to documentation at https://www.anaconda.com/docs/getting-started/miniconda/uninstall


