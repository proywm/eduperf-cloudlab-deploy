import sys
import time
import random

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

if not hasattr(target, "get_color_mix"):
    print("SKIP=get_color_mix not found")
    sys.exit(0)

random.seed(0)
left_color = [random.randint(0, 255) for _ in range(3)]
right_color = [random.randint(0, 255) for _ in range(3)]
proportion = 0.5

# Number of iterations chosen to keep runtime between 0.2 and 2 seconds
N = 200000

start = time.perf_counter()
for _ in range(N):
    target.get_color_mix(left_color, right_color, proportion)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")