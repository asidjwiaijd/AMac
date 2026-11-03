"""aMac link evidence for the source-built ESP-Hosted C5 transport."""
from pathlib import Path
import subprocess
import sys
Import("env")
subprocess.run([sys.executable, str(Path(env.subst("$PROJECT_DIR")) /
                                  "scripts/generate_hd20034_profile.py"), "--check"], check=True)
map_path = env.subst("$BUILD_DIR").replace("\\", "/") + "/firmware.map"
env.Append(LINKFLAGS=["-Wl,-Map," + map_path])
sdk = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs"))
variant = env.BoardConfig().get("build.chip_variant", "esp32p4")
hosted_headers = sdk / variant / "include/espressif__esp_hosted"
# The SDK exports public paths by default. The recompiled transport also
# consumes its matching private headers (serial, mempool, transport, HCI).
private_headers = Path(env.subst("$PROJECT_DIR")) / "third_party/esp_hosted_2_12_3/include"
transport_includes = sorted({str(p.parent) for folder in (hosted_headers, private_headers)
                             for p in folder.rglob("*.h")})


def configure_c5_transport(build_env, node):
    # Keep private SDK paths scoped to this C file. Adding them to every
    # Arduino/WiFi compile can exceed Windows' process command-line limit.
    return build_env.Object(node, CPPPATH=build_env.get("CPPPATH", []) + transport_includes)


env.AddBuildMiddleware(configure_c5_transport, "*/esp_hosted_transport_amac.c")
