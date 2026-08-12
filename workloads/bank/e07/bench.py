import sys
import time
import operator

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Ensure the required function exists
if not hasattr(target, "UVal") or not hasattr(target.UVal, "_calc"):
    print("SKIP=target.UVal._calc not found")
    sys.exit(0)

# Prepare deterministic input data
# Minimal plausible state for UVal
UVal = target.UVal
lhs = UVal(name="lhs", value=1.2345, stddev=0.0123, samples=1, mux=100.0, comment="", computed=False)
rhs = UVal(name="rhs", value=6.7890, stddev=0.0456, samples=1, mux=100.0, comment="", computed=False)

# Choose an operator that is supported
op = operator.add

# Number of iterations to target ~0.5-1.5 seconds (adjust if needed)
iterations = 2000000

start = time.perf_counter()
for _ in range(iterations):
    # Call the function under test
    _ = UVal._calc(op, lhs, rhs)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")