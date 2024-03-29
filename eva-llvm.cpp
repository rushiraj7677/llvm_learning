#include <string>
#include "./EvaLLVM.h"

int main(int argc, char const *argv[]){

	std::string program = R"(

    // (var z 42)
    // (var x (+ z 10))
    // (if (!= x 42)
    //   (set x 100)
    //   (set x 200))
    // (printf "X: %d \n")

    //   (var x 10)
    // 
    //   (while (> x 0) 
    //   (begin
    //     (set x (- x 1 ))
    //     (printf "%d " x)
    //   )
    //   )
  
    (def square (x) (* x x))

    (def sum ((a number) (b number)) -> number ( + a b))  

    (printf "Value: %d \n" ( + (sum 2 3) (square 2)))

    0
  
	)";
	EvaLLVM vm;
	vm.exec(program);
	return 0;
}
