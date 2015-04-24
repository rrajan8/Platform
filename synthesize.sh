printf "Setting up Cache Executables"
g++ -DL1_CACHE -std=c++11 cache_syn.cpp -lchdl -lchdl-module -o h2_cache_syn
g++ -DIQ_CACHE -std=c++11 cache_syn.cpp -lchdl -lchdl-module -o iq_cache_syn
printf "\nBuilding L2_cache"
./cache_syn
printf "\nBuilding Iqyax L1_cache\n"
./iq_cache_syn
printf "\nBuilding Harmonica L1_cache\n"
./h2_cache_syn
printf "\nBuilding Memory Interconnect\n"
./mem
printf "\nBuilding Distributed Interconnect\n"
./interconnect
printf "\nBuilding nodes\n"
./core 0 iq
./core 1 h2
./core 2 h2
./core 3 h2
printf "\nBuilding the Top Level Module\n"
./top_level
