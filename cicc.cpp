#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/InstrTypes.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <nvvm.h>

#include <dlfcn.h>
#include <iostream>
#include <cstdio>
#include <string>

using namespace llvm;
using namespace std;

#define LIBNVVM "libnvvm.so"

static void* libnvvm = NULL;

#define bind_lib(lib) \
if (!libnvvm) \
{ \
	libnvvm = dlopen(lib, RTLD_NOW | RTLD_GLOBAL); \
	if (!libnvvm) \
	{ \
		fprintf(stderr, "Error loading %s: %s\n", lib, dlerror()); \
		abort(); \
	} \
}

#define bind_sym(handle, sym, retty, ...) \
typedef retty (*sym##_func_t)(__VA_ARGS__); \
static sym##_func_t sym##_real = NULL; \
if (!sym##_real) \
{ \
	sym##_real = (sym##_func_t)dlsym(handle, #sym); \
	if (!sym##_real) \
	{ \
		fprintf(stderr, "Error loading %s: %s\n", #sym, dlerror()); \
		abort(); \
	} \
}

Module* initial_module = NULL;

void modifyModule(Module* module)
{
	if (!module) return;

	cout << "Module " << endl;

	// Add suffix to function name, for example.
	for (Module::iterator f = module->begin(), fe = module->end(); f != fe; f++)
	{
		cout << "Function " << f->getNameStr() << endl;

		for (Function::iterator bb = f->begin(), bbe = f->end(); bb != bbe; bb++)
		{
			cout << "BasicBlock " << bb->getNameStr() << " instructions " << bb->size() << endl;

			for (BasicBlock::iterator i = bb->begin(), ie = bb->end(); i != ie; i++)
			{
				unsigned int opcode = i->getOpcode();
				unsigned int numOperands = i->getNumOperands();

				cout << "Instruction opcode " << opcode << " opcode name " << i->getOpcodeName()
				     << " operands " << numOperands;
				for (unsigned int opIndex = 0; opIndex < numOperands; opIndex++)
				{
					Value* operand = i->getOperand(opIndex);
					cout << " " << operand->getNameStr();
				}
			    cout << endl;
				outs() << *i;
				cout << endl;

				//
				if (opcode == 9)
				{
					Instruction* newInstruction = BinaryOperator::Create(Instruction::Mul, i->getOperand(0), i->getOperand(1));
					cout << "newInstruction opcode " << newInstruction->getOpcode() << " opcode name " << newInstruction->getOpcodeName()
						     << " operands " << newInstruction->getNumOperands();
					for (unsigned int opIndex = 0; opIndex < newInstruction->getNumOperands(); opIndex++)
					{
						cout << " " << newInstruction->getOperand(opIndex)->getNameStr();
					}
					cout << endl;
					outs() << *newInstruction;
					cout << endl;

					//BasicBlock::iterator ii(i);
					//ReplaceInstWithInst(i->getParent()->getInstList(), ii, newInstruction);
					//ReplaceInstWithInst(i, newInstruction);
				}
			}
		}
	}
	
	//cout << module.getFunctionList() << endl;
}

bool called_compile = false;

nvvmResult nvvmAddModuleToProgram(nvvmProgram prog, const char *bitcode, size_t size, const char *name)
{
	bind_lib(LIBNVVM);
	bind_sym(libnvvm, nvvmAddModuleToProgram, nvvmResult, nvvmProgram, const char*, size_t, const char*);

	// Load module from bitcode.
	if (getenv("CICC_MODIFY_UNOPT_MODULE") && !initial_module)
	{
		string source = "";
		source.reserve(size);
		source.assign(bitcode, bitcode + size);
		MemoryBuffer *input = MemoryBuffer::getMemBuffer(source);
		string err;
		LLVMContext &context = getGlobalContext();
		initial_module = ParseBitcodeFile(input, context, &err);
		if (!initial_module)
			cerr << "Error parsing module bitcode : " << err;

		modifyModule(initial_module);

		printf("\n===========================\n");
		printf("MODULE BEFORE OPTIMIZATIONS\n");
		printf("===========================\n\n");		

//		outs() << *initial_module;

		// Save module back into bitcode.
		SmallVector<char, 128> output;
		raw_svector_ostream outputStream(output);
		WriteBitcodeToFile(initial_module, outputStream);
		outputStream.flush();

		// Call real nvvmAddModuleToProgram
		return nvvmAddModuleToProgram_real(prog, output.data(), output.size(), name);
	}

	called_compile = true;

	// Call real nvvmAddModuleToProgram
	return nvvmAddModuleToProgram_real(prog, bitcode, size, name);	
}

#undef bind_lib
#undef bind_sym

#define LIBC "libc.so.6"

static void* libc = NULL;

#define bind_lib(lib) \
if (!libc) \
{ \
	libc = dlopen(lib, RTLD_NOW | RTLD_GLOBAL); \
	if (!libc) \
	{ \
		cerr << "Error loading " << lib << ": " << dlerror() << endl; \
		abort(); \
	} \
}

#define bind_sym(handle, sym, retty, ...) \
typedef retty (*sym##_func_t)(__VA_ARGS__); \
static sym##_func_t sym##_real = NULL; \
if (!sym##_real) \
{ \
	sym##_real = (sym##_func_t)dlsym(handle, #sym); \
	if (!sym##_real) \
	{ \
		cerr << "Error loading " << #sym << ": " << dlerror() << endl; \
		abort(); \
	} \
}

static Module* optimized_module = NULL;

struct tm *localtime(const time_t *timep)
{
	static bool localtime_first_call = true;

	bind_lib(LIBC);
	bind_sym(libc, localtime, struct tm*, const time_t*);

	if (getenv("CICC_MODIFY_OPT_MODULE") && called_compile && localtime_first_call)
	{
		localtime_first_call = false;

		modifyModule(optimized_module);

		printf("\n==========================\n");
		printf("MODULE AFTER OPTIMIZATIONS\n");
		printf("==========================\n\n");

		cout << endl << endl << "------" << endl << "llvm code" << endl << "------" << endl << endl;

		if (optimized_module)
			outs() << *optimized_module;
	}
	
	return localtime_real(timep);
}

#include <unistd.h>

#define MAX_SBRKS 16

struct sbrk_t { void* address; size_t size; };
static sbrk_t sbrks[MAX_SBRKS];
static int nsbrks = 0;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

extern "C" void* malloc(size_t size)
{
	if (!size) return NULL;

	static bool __thread inside_malloc = false;
	
	if (!inside_malloc)
	{
		inside_malloc = true;

		bind_lib(LIBC);
		bind_sym(libc, malloc, void*, size_t);
		
		inside_malloc = false;

		void* result = malloc_real(size);

		if (called_compile && !optimized_module)
		{
			if (size == sizeof(Module))
				optimized_module = (Module*)result;
		}

		return result;
	}

	void* result = sbrk(size);
	if (nsbrks == MAX_SBRKS)
	{
		fprintf(stderr, "Out of sbrk tracking pool space\n");
		pthread_mutex_unlock(&mutex);
		abort();
	}
	pthread_mutex_lock(&mutex);
	sbrk_t s; s.address = result; s.size = size;
	sbrks[nsbrks++] = s;
	pthread_mutex_unlock(&mutex);

	return result;
}

extern "C" void* realloc(void* ptr, size_t size)
{
	bind_lib(LIBC);
	bind_sym(libc, realloc, void*, void*, size_t);
	
	for (int i = 0; i < nsbrks; i++)
		if (ptr == sbrks[i].address)
		{
			void* result = malloc(size);
#define MIN(a,b) (a) < (b) ? (a) : (b)
			memcpy(result, ptr, MIN(size, sbrks[i].size));
			return result;
		}
	
	return realloc_real(ptr, size);
}

extern "C" void free(void* ptr)
{
	bind_lib(LIBC);
	bind_sym(libc, free, void, void*);

	pthread_mutex_lock(&mutex);
	for (int i = 0; i < nsbrks; i++)
		if (ptr == sbrks[i].address) return;
	pthread_mutex_unlock(&mutex);
	
	free_real(ptr);
}

