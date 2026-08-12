import sys, time

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

func = getattr(target, 'reverseText', None)
if not callable(func):
    print("SKIP=reverseText not callable")
    sys.exit(0)

tx = 'a' * 1_000_000
iterations = 400

start = time.perf_counter()
try:
    for _ in range(iterations):
        func(tx)
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")