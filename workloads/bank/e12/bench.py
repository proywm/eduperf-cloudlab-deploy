import time
import sys
import os

try:
    import target
    mk_func = target.SourceInfo.mk
    if not callable(mk_func):
        raise AttributeError("SourceInfo.mk is not callable")
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Number of iterations chosen to make the benchmark run between 0.2 and 2 seconds.
# Adjust if necessary for the target environment.
ITERATIONS = 5000 if os.environ.get("EDUPERF_PROFILE") == "1" else 200000

def main():
    start = time.perf_counter()
    for _ in range(ITERATIONS):
        mk_func(levels=1)
    end = time.perf_counter()
    print(f"TIME_SECONDS={end - start}")

if __name__ == "__main__":
    main()
