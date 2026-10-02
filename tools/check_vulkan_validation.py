#!/usr/bin/env python3
"""Fail qualification if captured Vulkan validation reports an error."""
from pathlib import Path
import re
import sys

text = Path(sys.argv[1]).read_text()
errors = re.findall(r"^.*(?:Validation Error|VUID-|UNASSIGNED-.*[Ee]rror).*$", text, re.MULTILINE)
if errors:
    raise SystemExit("\n".join(errors))
print("No Vulkan validation errors in captured execution log")
