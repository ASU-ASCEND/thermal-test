import os
import re
from pathlib import Path

Import("env")

values = {}
dotenv = Path(env.subst("$PROJECT_DIR")) / ".env"
if dotenv.exists():
    for line in dotenv.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^\s*(?:export\s+)?(WIFI_SSID|WIFI_PASSWORD)\s*=\s*(.*?)\s*$", line)
        if match:
            value = match.group(2)
            if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
                value = value[1:-1]
            values[match.group(1)] = value

for key in ("WIFI_SSID", "WIFI_PASSWORD"):
    values[key] = os.environ.get(key, values.get(key, ""))
    if not values[key]:
        raise RuntimeError(f"Set {key} in .env or the environment before building")

def c_string(value):
    return '"' + ''.join(f'\\{byte:03o}' for byte in value.encode('utf-8')) + '"'

header = Path(env.subst("$BUILD_DIR")) / "wifi_credentials.h"
header.parent.mkdir(parents=True, exist_ok=True)
content = "#pragma once\n" + "".join(
    f"#define {key} {c_string(values[key])}\n" for key in ("WIFI_SSID", "WIFI_PASSWORD")
)
if not header.exists() or header.read_text(encoding="utf-8") != content:
    header.write_text(content, encoding="utf-8")
env.Append(CPPPATH=[str(header.parent)])
