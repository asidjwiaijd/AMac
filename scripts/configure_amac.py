"""aMac link evidence for the source-built ESP-Hosted C5 transport."""
from pathlib import Path
Import("env")
map_path = env.subst("$BUILD_DIR").replace("\\", "/") + "/firmware.map"
env.Append(LINKFLAGS=["-Wl,-Map," + map_path])
sdk = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32-libs"))
variant = env.BoardConfig().get("build.chip_variant", "esp32p4")
hosted_headers = sdk / variant / "include/espressif__esp_hosted"
# The SDK exports public paths by default. The recompiled transport also
# consumes its matching private headers (serial, mempool, transport, HCI).
env.Append(CPPPATH=sorted({str(p.parent) for p in hosted_headers.rglob("*.h")}))
private_headers = Path(env.subst("$PROJECT_DIR")) / "third_party/esp_hosted_2_12_3/include"
env.Append(CPPPATH=sorted({str(p.parent) for p in private_headers.rglob("*.h")}))
