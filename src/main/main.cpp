#include "../include/CPU.hpp"
int main() {
  int clk = 0;
  int max_clk;
  //  std::cin >> max_clk;
  //  // Highly recommended practice: used to control the total
  //  number of clock cycles to prevent falling into an
  //  infinite loop during debugging without realizing it.
  Memory mem;
  mem.load_ins();
  CPU cpu(mem); // Initialize various components and read instructions (only
                // memory is shown as an example here).
  cpu.run();
}