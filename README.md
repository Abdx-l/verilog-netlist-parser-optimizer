# Verilog Netlist Parser and Optimizer

C++17 structural Verilog expression parser with constant propagation, dead-expression removal from outputs, commutative common-subexpression sharing, DOT/Verilog export, and exhaustive equivalence checking on small circuits.

## Run

`cmake -S . -B build && cmake --build build && ctest --test-dir build`. `build/netlist examples/circuit.v` writes optimized.v and graph.dot. Supports scalar input/output/wire declarations and assign expressions with &, |, ^, ~ and parentheses, plus scalar and/or/xor/not primitives.
