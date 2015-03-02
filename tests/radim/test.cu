#include <stdio.h>

#define N 1

__global__ void mykernel(int *a, int *b, int *c)
{
   *c = *a + *b;
}

int main()
{
   int *host_a = (int*)malloc(N * sizeof(int));
   int *host_b = (int*)malloc(N * sizeof(int));
   int *host_c = (int*)malloc(N * sizeof(int));
   int *gpu_a = NULL;
   int *gpu_b = NULL;
   int *gpu_c = NULL;
   cudaMalloc(&gpu_a, N * sizeof(int));
   cudaMalloc(&gpu_b, N * sizeof(int));
   cudaMalloc(&gpu_c, N * sizeof(int));

   cudaMemcpy(gpu_a, host_a, N * sizeof(int), cudaMemcpyHostToDevice);
   cudaMemcpy(gpu_b, host_b, N * sizeof(int), cudaMemcpyHostToDevice);

	mykernel<<<1,1>>>(gpu_a, gpu_b, gpu_c);

   cudaMemcpy(host_c, gpu_c, N * sizeof(int), cudaMemcpyDeviceToHost);

   cudaFree(gpu_a);
   cudaFree(gpu_b);
   cudaFree(gpu_c);
   free(host_a);
   free(host_b);
   free(host_c);

	return 0;
}
