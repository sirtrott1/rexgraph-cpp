"""Write an offline presentation of completed native observations."""
import json
from pathlib import Path
import shutil


def render(records, catalog, verification, output):
    here = Path(__file__).resolve().parent
    for name in ("index.html", "demo.css", "demo.js"):
        shutil.copyfile(here/name, output/name)
    shutil.copyfile(here/"README.md", output/"README.md")
    value = {"records": records, "catalog": catalog, "verification": verification}
    (output/"demo-data.js").write_text("'use strict';\nconst DEMO = "+json.dumps(value, ensure_ascii=True)+";\n")
