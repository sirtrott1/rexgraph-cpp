#!/usr/bin/env python3
"""Validate generated shader structure and exclude floating arithmetic."""
import re
import subprocess
import sys

subprocess.run([sys.argv[2], "--target-env", "vulkan1.1", sys.argv[1]], check=True)
assembly = subprocess.check_output([sys.argv[3], sys.argv[1]], text=True)
assert not re.search(r"\bOp(?:TypeFloat|FAdd|FSub|FMul|FDiv|FRem|FMod|FNegate|FOrd\w+|FUnord\w+|ConvertF\w*|Convert\w*ToF)\b", assembly), "floating shader instruction"
assert "OpBitwiseAnd" in assembly and "OpBitwiseOr" in assembly and "OpBitwiseXor" in assembly
assert "OpNot" in assembly and "OpSwitch" in assembly
print("SPIR-V validated; exact bitwise interpreter contains no floating instructions")
