import sys, time, math, random, copy
try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

func = getattr(target, 'is_inside_postgis', None)
if not callable(func):
    print("SKIP=is_inside_postgis not callable")
    sys.exit(0)

# deterministic data
random.seed(0)
num_points = 2000
radius = 10.0
polygon = []
for i in range(num_points):
    angle = 2 * math.pi * i / num_points
    polygon.append((radius * math.cos(angle), radius * math.sin(angle)))
polygon.append(polygon[0])  # close the polygon
point = (0.0, 0.0)

# detect if the function mutates the polygon
poly_copy = copy.deepcopy(polygon)
_ = func(poly_copy, point)
destructive = (poly_copy != polygon)

iterations = 2000

# prepare polygons if needed
if destructive:
    polys = [polygon[:] for _ in range(iterations)]
else:
    polys = [polygon] * iterations

# warm‑up call
_ = func(polygon, point)

start = time.perf_counter()
for i in range(iterations):
    poly = polys[i] if destructive else polygon
    func(poly, point)
end = time.perf_counter()

print(f"TIME_SECONDS={end - start}")