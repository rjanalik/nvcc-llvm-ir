.PHONY: test test1 test1_unopt test1_opt test2 test2_unopt test2_opt

all: libcicc.so libnvcc.so pass

libcicc.so: cicc.cpp
	g++ -g -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -I. -I/opt/llvm-3.0/include -I/opt/cuda/nvvm/include/ -fPIC $< -shared -o $@ -ldl

libnvcc.so: nvcc.cpp
	g++ -g -O3 -I/opt/cuda/nvvm/include/ -fPIC $< -shared -o $@ -ldl

pass: pass.cpp
	g++ -g -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -I/opt/llvm-3.0/include $< -o $@ -L/opt/llvm-3.0/lib -lLLVM-3.0 -Wl,-rpath=/opt/llvm-3.0/lib

clean:
	rm -rf libcicc.so libnvcc.so pass

test: test1 test2

test1: test1_unopt test1_opt

test1_unopt: libcicc.so libnvcc.so
	CICC_MODIFY_UNOPT_MODULE=1 LD_PRELOAD=./libnvcc.so nvcc -arch=sm_30 test1.cu -rdc=true -c -keep

test1_opt: libcicc.so libnvcc.so
	CICC_MODIFY_OPT_MODULE=1 LD_PRELOAD=./libnvcc.so nvcc -arch=sm_30 test1.cu -rdc=true -c -keep

test2: test2_unopt test2_opt

test2_unopt: libcicc.so libnvcc.so
	CICC_MODIFY_UNOPT_MODULE=1 LD_PRELOAD=./libnvcc.so nvcc -arch=sm_30 test2.cu -rdc=true -c -keep

test2_opt: libcicc.so libnvcc.so
	CICC_MODIFY_OPT_MODULE=1 LD_PRELOAD=./libnvcc.so nvcc -arch=sm_30 test2.cu -rdc=true -c -keep

