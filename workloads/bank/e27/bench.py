import time
import os

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    exit(0)

func = getattr(target, 'pad_text_to_characters_length', None)
if not callable(func):
    print("SKIP=pad_text_to_characters_length not found")
    exit(0)

# deterministic input data
lines_count = 1000
lines = [f"Line {i}" for i in range(lines_count)]
text = os.linesep.join(lines)
characters = "=" * 10

iterations = 1000

start = time.perf_counter()
for _ in range(iterations):
    func(text, characters)
end = time.perf_counter()

print(f"TIME_SECONDS={end - start}")