#include "../include/CPU.hpp"
int main() {
  int clk = 0;
  Memory mem;
  mem.load_ins();
  CPU cpu(mem);
  cpu.run();
}