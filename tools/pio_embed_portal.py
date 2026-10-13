# Script do PlatformIO: regenera firmware/src/portal_gz.h antes de compilar.
Import("env")  # noqa: F821
import subprocess, sys, os
subprocess.check_call([sys.executable, os.path.join(env["PROJECT_DIR"], "tools", "embed_portal.py")])  # noqa: F821
