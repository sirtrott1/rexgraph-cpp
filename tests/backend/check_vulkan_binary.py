#!/usr/bin/env python3
"""Independent per-bit Boolean interpreter, with no packed evaluator reuse."""
import json
import subprocess
import sys

command = [sys.argv[1], "--emit", *sys.argv[2:]]
checks = 0
cases = 0
for line in subprocess.check_output(command, text=True).splitlines():
    case = json.loads(line)
    size = case["slots"]
    expected = [[0] * ((size + 63) // 64) for _ in case["outputs"]]
    for bit in range(size):
        registers = [False] * case["registers"]
        for opcode, dst, a, b, c, aspect in case["instructions"]:
            left, right, third = registers[a], registers[b], registers[c]
            if opcode == 0:
                value = bool((case["input"][aspect][bit // 64] >> (bit % 64)) & 1)
            elif opcode == 1:
                value = False
            elif opcode == 2:
                value = True
            elif opcode == 3:
                value = not left
            elif opcode == 4:
                value = left and right
            elif opcode == 5:
                value = left or right
            elif opcode == 6:
                value = left != right
            elif opcode == 7:
                value = left and not right
            elif opcode == 8:
                value = right if left else third
            else:
                raise AssertionError("unknown instruction")
            registers[dst] = value
        for output, reg in enumerate(case["outputs"]):
            if registers[reg]:
                expected[output][bit // 64] |= 1 << (bit % 64)
            checks += 1
    assert case["result"] == expected, (cases, size)
    cases += 1
assert cases == 196
print(f"{checks} independent Boolean output checks passed in {cases} Vulkan cases")
