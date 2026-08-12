import sys
import time

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Ensure normalize_options exists and is callable
normalize = getattr(target, "normalize_options", None)
if not callable(normalize):
    print(f"SKIP=normalize_options not found or not callable")
    sys.exit(0)

# Build deterministic input data
options_list = [
    {"output_format": "PNG", "resize": "512x512", "jpeg_quality": 90, "opacity_threshold": 200},
    {"output_format": "jpg", "resize": 256, "jpeg_quality": "80", "opacity_threshold": "150"},
    {"output_format": "auto", "resize": "orig", "jpeg_quality": 1.2, "opacity_threshold": 0.5},
    {"output_format": "jpeg", "resize": "1024 768", "jpeg_quality": 0.95, "opacity_threshold": 255},
    {"output_format": "orig", "resize": "512,512", "jpeg_quality": "100", "opacity_threshold": 300},
    {"output_format": "PNG", "resize": "512:512", "jpeg_quality": 0, "opacity_threshold": 0},
    {"output_format": "jpg", "resize": "512;512", "jpeg_quality": 50, "opacity_threshold": 128},
    {"output_format": "auto", "resize": "512x512", "jpeg_quality": "110", "opacity_threshold": "0.1"},
    {"output_format": "jpeg", "resize": "512", "jpeg_quality": 0.84, "opacity_threshold": 254},
    {"output_format": "orig", "resize": "orig", "jpeg_quality": 0.5, "opacity_threshold": 255},
    {"output_format": "PNG", "resize": "512x512", "jpeg_quality": 90, "opacity_threshold": 200},
    {"output_format": "png", "resize": "512x512", "jpeg_quality": 0.84, "opacity_threshold": 254},
]

# Number of iterations to aim for ~0.2-2 seconds
ITERATIONS = 500_000

# Warm-up to avoid first-call overhead
for opt in options_list:
    normalize(opt)

start = time.perf_counter()
for i in range(ITERATIONS):
    opt = options_list[i % len(options_list)]
    normalize(opt)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")