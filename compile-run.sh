# Compile mail:
clang++-17 -o eva-llvm `llvm-config-17 --cxxflags --ldflags --system-libs --libs core` -fcxx-exceptions eva-llvm.cpp

#run main:
./eva-llvm

#Execute generated IR:
lli-17 ./out.ll

echo $?

printf "\n"
