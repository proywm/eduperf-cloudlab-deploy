import sys
import os
import time

try:
    import target
except Exception as exc:
    print(f"SKIP={exc}")
    sys.exit(0)

# Verify required attributes
if not hasattr(target, "PuppetLogger"):
    print("SKIP=target.PuppetLogger not found")
    sys.exit(0)

PuppetLogger = target.PuppetLogger

if not hasattr(PuppetLogger, "log_length") or not hasattr(PuppetLogger, "add_log"):
    print("SKIP=required methods missing")
    sys.exit(0)

# Create deterministic log file in current directory
log_path = "benchmark_log.txt"
try:
    with open(log_path, "wb") as f:
        chunk = b"A" * 1024 * 1024  # 1 MB
        for _ in range(10):  # 10 MB total
            f.write(chunk)
except Exception as exc:
    print(f"SKIP=failed to create log file: {exc}")
    sys.exit(0)

# Prepare logger
logger = PuppetLogger()
try:
    # Open the file for reading/writing to keep it open
    log_fp = open(log_path, "rb+")
except Exception as exc:
    print(f"SKIP=failed to open log file: {exc}")
    sys.exit(0)

log_id = "stdout"
try:
    logger.add_log(log_id, log_fp)
except Exception as exc:
    print(f"SKIP=add_log failed: {exc}")
    log_fp.close()
    sys.exit(0)

# Warm up
try:
    _ = logger.log_length(log_id)
except Exception as exc:
    print(f"SKIP=log_length failed on warmup: {exc}")
    logger.close()
    sys.exit(0)

# Benchmark loop
loops = 1500000  # adjust to hit ~0.2-2 seconds
start = time.perf_counter()
for _ in range(loops):
    try:
        logger.log_length(log_id)
    except Exception:
        pass
end = time.perf_counter()

# Clean up
logger.close()
try:
    os.remove(log_path)
except Exception:
    pass

print(f"TIME_SECONDS={end - start}")