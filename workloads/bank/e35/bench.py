import sys, time, random, io

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

if not hasattr(target, 'ContainerIO'):
    print("SKIP=ContainerIO not found")
    sys.exit(0)

# deterministic data
random.seed(0)
lines_count = 500000
line_len = 50
lines = []
for _ in range(lines_count):
    line = ''.join(random.choice('abcdefghijklmnopqrstuvwxyz0123456789') for _ in range(line_len))
    lines.append(line + '\n')
data = ''.join(lines).encode('utf-8')
file_obj = io.BytesIO(data)
file_obj.mode = 'rb'

offset = 0
length = len(data)
container = target.ContainerIO(file_obj, offset, length)

start = time.perf_counter()
while True:
    line = container.readline()
    if not line:
        break
end = time.perf_counter()
print(f"TIME_SECONDS={end - start}")