#!/usr/bin/env python3
from pathlib import Path

root = Path("/Users/prabathwijethilaka/Sem7/Embedded/EcoWatt_TeamPowerPort/PIO/ECOWATT/src")
output = Path("all_cpp_code.txt")

with output.open("w", encoding="utf-8") as out_f:
    for cpp_path in sorted(root.rglob("*.cpp")):
        header = f"// ===== {cpp_path} =====\n"
        out_f.write(header)
        out_f.write(cpp_path.read_text(encoding="utf-8"))
        if not out_f.tell() == 0 and not header.endswith("\n\n"):
            out_f.write("\n")
        out_f.write("\n")

print(f"Combined {len(list(root.rglob('*.cpp')))} .cpp files into {output.resolve()}")
