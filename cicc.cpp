#include <llvm/Module.h>
#include <llvm/LLVMContext.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/InstrTypes.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <nvvm.h>

#include <cstdio>
#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

using namespace llvm;
using namespace std;

// Switch resulting module printing on/off.
bool print_module = true;

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

static bool memory_hooks = true;

static void modifyModule(const char *bitcode, size_t size, string& output)
{
	// Create a temporary file.
	struct TempFile
	{
		string filename;

		TempFile(const string& mask) : filename("")
		{
			vector<char> vfilename(mask.c_str(), mask.c_str() + mask.size() + 1);
			int fd = mkstemp(&vfilename[0]);
			if (fd == -1)
				return;
			close(fd);
			filename = (char*)&vfilename[0];
			unlink(filename.c_str());
		}
	}
	tmp("/tmp/fileXXXXXX");

	// Make sure the temp filename is generated.		
	if (tmp.filename == "")
	{
		cerr << "Cannot create a temp file" << endl;
		exit(1);
	}

	// Store LLVM IR bitcode into temp file.
	string input = "";
	input.reserve(size);
	input.assign(bitcode, bitcode + size);
	ofstream tmpstream(tmp.filename.c_str(), ios::out | ios::binary);
	tmpstream << input;
	tmpstream.close();

	// Invoke LLVM IR modification pass program and receive
	// the output IR bitcode.
	{
		stringstream cmd;
		char* cwd = get_current_dir_name();
		cmd << cwd << "/pass " << tmp.filename;
		//cout << cmd.str() << endl;
		system(cmd.str().c_str());
	}

	ifstream inputstr(tmp.filename.c_str(), ios::in | ios::binary);
	output.assign(istreambuf_iterator<char>(inputstr), istreambuf_iterator<char>());
	
	if (output == "")
	{
		cerr << "Module modification pass not found" << endl;
		exit(1);
	}

	memory_hooks = true;
}

bool called_compile = false;

nvvmResult nvvmAddModuleToProgram(nvvmProgram prog, const char *bitcode, size_t size, const char *name)
{
	bind_lib(LIBNVVM);
	bind_sym(libnvvm, nvvmAddModuleToProgram, nvvmResult, nvvmProgram, const char*, size_t, const char*);

	// Load module from bitcode.
	if (getenv("CICC_MODIFY_UNOPT_MODULE"))
	{
		memory_hooks = false;

		string output;
		modifyModule(bitcode, size, output);

		if (print_module)
		{
			printf("\n===========================\n");
			printf("MODULE BEFORE OPTIMIZATIONS\n");
			printf("===========================\n\n");

			string err;
			LLVMContext &context = getGlobalContext();
			MemoryBuffer* mboutput = MemoryBuffer::getMemBuffer(output);
			Module* module = ParseBitcodeFile(mboutput, context, &err);

			if (!module)
			{
				cerr << "Error parsing output module: " << err << endl;
				exit(1);
			}

			outs() << *module;
		}

		memory_hooks = true;

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

		memory_hooks = false;

		// Save module into bitcode.
		SmallVector<char, 128> bitcode;
		raw_svector_ostream outputStream(bitcode);
		WriteBitcodeToFile(optimized_module, outputStream);
		outputStream.flush();

		string output;
		modifyModule(bitcode.data(), bitcode.size(), output);

		if (print_module)
		{
			printf("\n==========================\n");
			printf("MODULE AFTER OPTIMIZATIONS\n");
			printf("==========================\n\n");

			string err;
			LLVMContext &context = getGlobalContext();
			MemoryBuffer* mboutput = MemoryBuffer::getMemBuffer(output);
			Module* module = ParseBitcodeFile(mboutput, context, &err);

			if (!module)
			{
				cerr << "Error parsing output module: " << err << endl;
				exit(1);
			}

			outs() << *module;
		}

		memory_hooks = true;
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
	
	if (!inside_malloc || !memory_hooks)
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
	
	if (memory_hooks)
	{
		for (int i = 0; i < nsbrks; i++)
			if (ptr == sbrks[i].address)
			{
				void* result = malloc(size);
#define MIN(a,b) (a) < (b) ? (a) : (b)
				memcpy(result, ptr, MIN(size, sbrks[i].size));
				return result;
			}
	}
	
	return realloc_real(ptr, size);
}

extern "C" void free(void* ptr)
{
	bind_lib(LIBC);
	bind_sym(libc, free, void, void*);

	if (memory_hooks)
	{
		pthread_mutex_lock(&mutex);
		for (int i = 0; i < nsbrks; i++)
			if (ptr == sbrks[i].address) return;
		pthread_mutex_unlock(&mutex);
	}
	
	free_real(ptr);
}

