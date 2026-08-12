import sys
import time
import os
import tempfile
import zipfile
import io

# Try to import the target module
try:
    import target
except Exception as e:
    print(f"SKIP=import error: {e}")
    sys.exit(0)

# Determine how to create a ZipFile instance
ZipFileCls = None
try:
    ZipFileCls = getattr(target, "ZipFile")
except AttributeError:
    pass

# If target has a function named namelist directly, we will use it later
has_namelist_func = hasattr(target, "namelist") and callable(target.namelist)

# Create a deterministic zip file with many entries
NUM_ENTRIES = 20000
ENTRY_PREFIX = "file_"
ENTRY_SUFFIX = ".txt"

# Use a temporary file to store the zip
tmp_zip = tempfile.NamedTemporaryFile(delete=False)
zip_path = tmp_zip.name
tmp_zip.close()

try:
    with zipfile.ZipFile(zip_path, mode="w", compression=zipfile.ZIP_STORED) as z:
        for i in range(NUM_ENTRIES):
            name = f"{ENTRY_PREFIX}{i:08d}{ENTRY_SUFFIX}"
            z.writestr(name, "")
except Exception as e:
    print(f"SKIP=failed to create zip file: {e}")
    os.unlink(zip_path)
    sys.exit(0)

# Try to instantiate ZipFile from target
zf = None
try:
    if ZipFileCls is not None:
        zf = ZipFileCls(zip_path, "r")
    else:
        # Maybe target accepts a file-like object
        with open(zip_path, "rb") as f:
            zf = target.ZipFile(f, "r")
except Exception as e:
    print(f"SKIP=cannot instantiate ZipFile: {e}")
    os.unlink(zip_path)
    sys.exit(0)

# Verify that namelist is available
if not hasattr(zf, "namelist"):
    if has_namelist_func:
        # Use the function directly
        def call_namelist():
            return target.namelist(zip_path)
    else:
        print("SKIP=namelist method not found")
        zf.close()
        os.unlink(zip_path)
        sys.exit(0)
else:
    def call_namelist():
        return zf.namelist()

# Run the benchmark
ITERATIONS = 10
start = time.perf_counter()
for _ in range(ITERATIONS):
    names = call_namelist()
end = time.perf_counter()

# Clean up
zf.close()
os.unlink(zip_path)

print(f"TIME_SECONDS={end - start}")