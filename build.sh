# Install folders of SDSL - Shannon
inc_sdsl=<SDSL folder>/include
lib_sdsl=<SDSL folder>/lib

# Input  : List of factors
# Output : Number of factors + depth + list of factors + Permutation P + Offset
#          array O
echo "[Adapt list of factors] Compiling .. (sample 64)"
g++ -O3 -std=c++11 -DDENS=64 -o adapt_factors adapt_factors.cpp \
-I$inc_sdsl -L$lib_sdsl -lsdsl 

# Input  : List of factors
# Output : Decompressed text
echo "[Sequential decompression algorithm] Compiling .. "
g++ -O3 -std=c++11 -o seq_decompress sequential_decompress.cpp

# Input  : Number of factors + depth + list of factors + Permutation P + Offset
#          array O
# Output : Decompressed text
echo "[Parallel decompression framework Compiling .. (sample 64)"
g++ -O3 -std=c++11 -DDENS=64 -o par_framework \
parallel_framework.cpp -I$inc_sdsl -L$lib_sdsl -fcilkplus -lcilkrts \
-lsdsl
