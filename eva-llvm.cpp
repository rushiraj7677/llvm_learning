#include <string>
#include "./EvaLLVM.h"

int main(int argc, char const *argv[]){

	std::string program = R"(

    (var x 42)
    (if (== x 42)
      (if (> x 42)
        1
        3)
    2)
	)";
	EvaLLVM vm;
	vm.exec(program);
	return 0;
}
