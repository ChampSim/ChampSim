#include "stdio.h"

extern "C" {
__attribute__((noinline)) void __champsim_start_trace(void) { asm volatile(""); }
__attribute__((noinline)) void __champsim_stop_trace(void) { asm volatile(""); }
}
int main()
{
  for (size_t i = 0; i < 10; ++i) { // not tracing the loop maintainance code
    __champsim_start_trace();
    printf("Hello World"); // but tracing this code
    __champsim_stop_trace();
  }
}
