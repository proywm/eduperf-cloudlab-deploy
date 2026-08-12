import time
import sys

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

if not hasattr(target, "create_accessor_from_properties"):
    print("SKIP=missing function")
    sys.exit(0)

# Build deterministic input data
count = 1_000_000
num_components = 3
component_size = 4  # float
stride = component_size * num_components
total_bytes = stride * count
buffer = bytearray(total_bytes)  # zeros

buffer_view = memoryview(buffer)

class DummyOp:
    def __init__(self, buffer_view, stride):
        self.gltf = {
            "buffers": [{"uri": ""}],
            "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": len(buffer)}],
            "accessors": []
        }
        self._buffer_view = buffer_view
        self._stride = stride

    def get(self, key, default):
        if key == "buffer_view":
            # Return a tuple as expected by create_accessor_from_properties
            return (self._buffer_view, self._stride)
        if key == "buffer":
            return self._buffer_view
        return default

op = DummyOp(buffer_view, stride)

accessor = {
    "count": count,
    "componentType": 5126,  # FLOAT
    "type": "VEC3",
    "bufferView": 0
}

# Warm‑up call (outside timed region)
_ = target.create_accessor_from_properties(op, accessor)

iterations = 2
start = time.perf_counter()
for _ in range(iterations):
    _ = target.create_accessor_from_properties(op, accessor)
elapsed = time.perf_counter() - start

print(f"TIME_SECONDS={elapsed}")