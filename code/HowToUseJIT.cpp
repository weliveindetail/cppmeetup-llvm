// source: https://github.com/weliveindetail/cppmeetup-llvm/blob/master/code/HowToUseJIT.cpp
// based on: examples/HowToUseJIT/HowToUseJIT.cpp

#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"

llvm::LLVMContext Ctx;
llvm::IRBuilder<> Builder(Ctx);

// ---------------------------------------------------------- Declare functions

llvm::Function* declareFunctionAddConst(llvm::Module* module)
{
  using namespace llvm;

  auto* name = "addConst";
  auto* returnTy = Type::getInt32Ty(Ctx);
  auto* argumentTy = Type::getInt32Ty(Ctx);
  auto* signature = FunctionType::get(returnTy, { argumentTy }, false);
  auto  linkage = Function::PrivateLinkage;

  auto* function = Function::Create(signature, linkage, name, module);

  // set argument name to make IR more readable
  Argument* firstArg = &*function->arg_begin();
  firstArg->setName("Increment");

  return function;
}

llvm::Function* declareFunctionFoo(llvm::Module* module)
{
  using namespace llvm;

  auto* name = "foo";
  auto* returnTy = Type::getInt32Ty(Ctx);
  auto* signature = FunctionType::get(returnTy, false);
  auto  linkage = Function::PrivateLinkage;

  return Function::Create(signature, linkage, name, module);
}

// ------------------------------------------------------------------ Emit code

void emitCodeAddConst(llvm::Function* decl, int incr)
{
  Builder.SetInsertPoint(
    llvm::BasicBlock::Create(Ctx, "addConstBlock", decl));

  // params for add instruction
  llvm::Value* firstArg = &*decl->arg_begin();
  llvm::Value* increment = llvm::ConstantInt::get(firstArg->getType(), incr);

  llvm::Value* addInstr = Builder.CreateAdd(increment, firstArg);
  Builder.CreateRet(addInstr);
}

void emitCodeFoo(llvm::Function* decl, llvm::Function* addConst)
{
  Builder.SetInsertPoint(
    llvm::BasicBlock::Create(Ctx, "fooBlock", decl));

  // insert call to the addConst function
  llvm::Type* tenTy = llvm::Type::getInt32Ty(Ctx);
  llvm::Value* tenVal = llvm::ConstantInt::get(tenTy, 10);
  llvm::CallInst* returnVal = Builder.CreateCall(addConst, tenVal);

  // set hint for tail optimization
  returnVal->setTailCall(true);

  Builder.CreateRet(returnVal);
}

// --------------------------------------------------------------------- Driver

llvm::Function* codegenIR(llvm::Module* module, int constIncrement)
{
  llvm::Function* fnFoo = declareFunctionFoo(module);
  llvm::Function* fnAddConst = declareFunctionAddConst(module);

  emitCodeAddConst(fnAddConst, constIncrement);
  emitCodeFoo(fnFoo, fnAddConst);

  return fnFoo;
}

int main(int argc, char** argv)
{
  llvm::InitializeNativeTarget();
  int runtimeConst = (argc > 1) ? atoi(argv[1]) : 1;

  auto module = std::make_unique<llvm::Module>("test", Ctx);
  auto function = codegenIR(module.get(), runtimeConst);

  llvm::outs() << "We just constructed this LLVM module:\n\n";
  llvm::outs() << *module.get() << "\n\n";

  {
    llvm::ExecutionEngine* EE = llvm::EngineBuilder(std::move(module)).create();
    llvm::GenericValue result = EE->runFunction(function, { /* noargs */ });

    llvm::outs() << "Running foo returns: ";
    llvm::outs() << result.IntVal << "\n";
    delete EE;
  }

  llvm::llvm_shutdown();
  llvm::outs().flush();
  return 0;
}
