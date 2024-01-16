#ifndef EvaLLVM_h
#define EvaLLVM_h

#include <alloca.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <regex>
#include <system_error>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"

#include "src/Environment.h"
#include "src/parser/EvaParser.h"

using syntax::EvaParser;

using Env = std::shared_ptr<Environment>;

// Generic binary operator:
#define GEN_BINARY_OP(Op, varName) \
do { \
    auto op1 = gen(exp.list[1], env); \
    auto op2 = gen(exp.list[2], env); \
    return builder->Op(op1, op2, varName); \
} while (false)

class EvaLLVM {
	public:
		EvaLLVM() : parser(std::make_unique<EvaParser>()) {
			moduleInit();
			setupExternFunctions();
      setupGlobalEnvironment();
		}

		void exec(const std::string& program){
	
			auto ast = parser->parse("(begin" + program + ")");

			compile(ast);
			//print generated code.
			module->print(llvm::outs(), nullptr);

			//save module IR to file:
			saveModuleToFile("./out.ll");
		}
	private:
	
		void compile(const Exp& ast){
			fn = createFunction("main", llvm::FunctionType::get(builder->getInt32Ty(), false), globalEnv);
			createGlobalVar("VERSION", builder->getInt32(43));

			gen(ast, globalEnv);

			builder->CreateRet(builder->getInt32(0));
			
		}

		llvm::Value* gen(const Exp& exp, Env env){
			// return builder->getInt32(43);
			switch (exp.type) {
			/**
			*
			* Numbers.
			*/
			case ExpType::NUMBER:
				return builder->getInt32(exp.number);
			/**
			*
			* Strings.
			*/
			case ExpType::STRING:{
				// Unescape special chars. TODO: support all chars or handle in parser.
				auto re = std:: regex("\\\\n");
				auto str = std:: regex_replace(exp.string, re, "\n");
				return builder->CreateGlobalStringPtr(str);						 
			}
				
			case ExpType::SYMBOL:
				
				if(exp.string == "true" || exp.string == "false") {
					return builder->getInt1(exp.string == "true" ? true	: false);
				}else{
					//variable
          auto varName = exp.string;
          auto value = env->lookup(varName);

          //1.Local variable:
          if (auto localVar = llvm::dyn_cast<llvm::AllocaInst>(value)) {
            return builder->CreateLoad(localVar->getAllocatedType(), localVar, varName.c_str());
          }
            // 2. Global vars:
          else if (auto globalVar = llvm::dyn_cast<llvm::GlobalVariable>(value)) {
            return builder->CreateLoad (globalVar->getInitializer()->getType(), globalVar, varName.c_str());
          }
				}

			case ExpType::LIST:
				
				auto tag = exp.list[0];
				/**
				*
				* Special cases.
				*/
				if (tag.type == ExpType::SYMBOL) {
			
					auto op = tag.string;

          //binary math operations:
          if(op == "+"){
            GEN_BINARY_OP(CreateAdd, "tmpadd");
          }
          else if (op == "-") {
            GEN_BINARY_OP(CreateSub, "tmpsub");
          }
          else if (op == "*") {
            GEN_BINARY_OP(CreateMul, "tmpmul");
          }
          else if (op == "/") {
            GEN_BINARY_OP(CreateSDiv, "tmpdiv");
          }
          //compare operations:
          //
          //UGT- unsigned greater than

          else if (op == ">") {
            GEN_BINARY_OP(CreateICmpUGT, "tmpcmp");
          }
          else if (op == "<") {
            GEN_BINARY_OP(CreateICmpULT, "tmpcmp");
          }
          else if (op == "==") {
            GEN_BINARY_OP(CreateICmpEQ, "tmpcmp");
          }
          else if (op == "!=") {
            GEN_BINARY_OP(CreateICmpNE, "tmpcmp");
          }
          else if (op == ">=") {
            GEN_BINARY_OP(CreateICmpUGE, "tmpcmp");
          }
          else if (op == "<=") {
            GEN_BINARY_OP(CreateICmpULE, "tmpcmp");
          }
          else if (op == "if") {
            //compile <condition>:
            auto cond = gen(exp.list[1],env);
            // Then block:
            auto thenBlock = createBB("then", fn);
            // to handle nested if-expressions:
            auto elseBlock = createBB("else", fn);
            auto ifEndBlock = createBB("ifend", fn) ;
            // Condition branch:
            builder->CreateCondBr(cond, thenBlock, elseBlock);
            // Then branch:
            builder->SetInsertPoint(thenBlock) ;
            auto thenRes = gen(exp.list[2], env);
            builder->CreateBr(ifEndBlock);
            // Restore the block to handle nested if-expressions,
            thenBlock = builder->GetInsertBlock();

            // Else branch:
            elseBlock->moveAfter(thenBlock);
            builder->SetInsertPoint(elseBlock);
            auto elseRes = gen(exp.list[3], env);
            builder->CreateBr(ifEndBlock);

            // If-end block:
            ifEndBlock->moveAfter(elseBlock);
            builder->SetInsertPoint(ifEndBlock);
            // Result of the if expression is phi:
            auto phi = builder->CreatePHI(builder->getInt32Ty(), 2, "tmpif");

            phi->addIncoming(thenRes, thenBlock);
            phi->addIncoming(elseRes, elseBlock);


            return phi;
            
          }

					// (printf "Value: %d" 42)
					//
					if (op == "var"){
            auto varNameDecl = exp.list[1];
						auto varName = extractVarName(varNameDecl);
            
            //initializer:
						auto init = gen(exp.list[2],env);

            //type:
            auto varTy = extractVarType(varNameDecl);

            //variable:
            auto varBinding = allocVar(varName,varTy,env);

            //set Value:
            return builder->CreateStore(init, varBinding);
					}
          else if (op == "set") {
            
            auto value = gen(exp.list[2],env);
            auto varName = exp.list[1].string;
            auto varBinding = env->lookup(varName);
            builder->CreateStore(value, varBinding);
            return value;

          }
					else if(op == "begin"){
            //block scope:
            auto blockEnv = std::make_shared<Environment>(std::map<std::string, llvm::Value*>{},env);

						//blockRes = block relation
						llvm::Value* blockRes;
						for(auto i = 1; i<exp.list.size(); i++){
							blockRes = gen(exp.list[i],blockEnv);
						}
						return blockRes;
					}
					else if (op == "printf") {
						auto printfFn = module->getFunction("printf");
						std::vector<llvm::Value*> args{};

						for (auto i = 1; i < exp. list.size(); i++) {
							args.push_back(gen(exp.list[i],env));
						}

					return builder->CreateCall(printfFn, args);
					}	
				
				}

			}
			return builder->getInt32(0);
		}	

    
    std::string extractVarName(const Exp& exp){
    
      return exp.type == ExpType::LIST ? exp.list[0].string : exp.string; 
    
    }

    llvm::Type* extractVarType(const Exp& exp){
      return exp.type == ExpType::LIST ? getTypeFromString(exp.list[1].string) : builder->getInt32Ty();
    }
    
    llvm::Type* getTypeFromString(const std::string&  type_){

      // number -> 132
      if (type_ == "number") {
        return builder->getInt32Ty();
      }
      // string 18* (aka char*)
      if (type_ == "string") {
        return builder->getInt8Ty()->getPointerTo();
      } 
      // default:
      return builder->getInt32Ty();
    }

    llvm::Value* allocVar(const std::string& name, llvm::Type* type_, Env env){
      varsBuilder->SetInsertPoint(&fn->getEntryBlock());
      auto varAlloc = varsBuilder->CreateAlloca(type_, 0, name.c_str());
      env->define(name, varAlloc);
      return varAlloc;
    
    
    }

		/**
		* Creates a global variable.
		*/	
		llvm:: GlobalVariable* createGlobalVar (const std::string& name,llvm:: Constant* init) {
			module->getOrInsertGlobal (name, init->getType());
			auto variable = module->getNamedGlobal (name);
			variable->setAlignment (llvm::MaybeAlign(4));
			variable->setConstant(false);
			variable->setInitializer (init);
			return variable;
		}
		void setupExternFunctions(){

			auto bytePtrTy = builder->getInt8Ty()->getPointerTo();

			module->getOrInsertFunction("printf", llvm::FunctionType::get(builder->getInt32Ty(), bytePtrTy, true));

		}

		llvm::Function* createFunction(const std::string& fnName, llvm::FunctionType* fnType, Env env){
			auto fn = module->getFunction(fnName);
			if (fn == nullptr){
				fn = createFunctionProto(fnName, fnType, env);
			}
			createFunctionBlock(fn);
			return fn;
		}

		llvm::Function* createFunctionProto(const std::string& fnName, llvm::FunctionType* fnType,Env env){

			auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage, fnName, *module);
      
			verifyFunction(*fn);
      env->define(fnName, fn);
			return fn;
		}

		void createFunctionBlock(llvm::Function* fn){
			auto entry = createBB("entry", fn);
			builder->SetInsertPoint(entry);
		}
		
		llvm::BasicBlock* createBB(std::string name, llvm::Function* fn = nullptr){
			return llvm::BasicBlock::Create(*ctx, name, fn);	
		}

		void saveModuleToFile(const std::string& fileName) {
			std::error_code errorCode;
			llvm::raw_fd_ostream outLL(fileName, errorCode);
			module->print(outLL, nullptr);
		}

		void moduleInit(){
			ctx = std::make_unique<llvm::LLVMContext>();

			module = std::make_unique<llvm::Module>("EvaLLVM", *ctx);

			builder = std::make_unique<llvm::IRBuilder<>>(*ctx);

      varsBuilder = std::make_unique<llvm::IRBuilder<>>(*ctx);
		}
    
    void setupGlobalEnvironment() {
      std::map<std::string, llvm::Value*> globalObject{
          {"VERSION", builder->getInt32(42)},
      };

      std::map<std::string, llvm:: Value*> globalRec{};

      for (auto& entry : globalObject) {
        globalRec[entry.first] =
        createGlobalVar(entry.first, (llvm::Constant*) entry.second);
      }
      globalEnv = std::make_shared<Environment>(globalRec, nullptr);
    }
		
	std::unique_ptr<EvaParser> parser;
  
  std::shared_ptr<Environment> globalEnv;

	llvm::Function* fn;

	std::unique_ptr<llvm::LLVMContext> ctx;

  std::unique_ptr<llvm::IRBuilder<>> varsBuilder;

	std::unique_ptr<llvm::IRBuilder<>> builder;

	std::unique_ptr<llvm::Module> module;
};
#endif
