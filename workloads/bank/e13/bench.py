import sys
import time
import inspect

try:
    import target
except Exception as e:
    print(f"SKIP={e}")
    sys.exit(0)

# Helper to create a LoggerAdapter instance if needed
def create_adapter():
    # Try to get a logger from target
    logger = None
    try:
        if hasattr(target, "getLogger"):
            logger = target.getLogger("bench")
    except Exception:
        pass
    if logger is None:
        try:
            import logging
            logger = logging.getLogger("bench")
        except Exception:
            return None
    mapping = {}
    # Find LoggerAdapter class
    for name, cls in inspect.getmembers(target, inspect.isclass):
        if name == "LoggerAdapter":
            try:
                return cls(logger, mapping)
            except Exception:
                return None
    return None

# Determine the callable to benchmark
callable_obj = None
level_arg = 10  # DEBUG level

# Check if target has a standalone function
if hasattr(target, "isEnabledFor"):
    attr = getattr(target, "isEnabledFor")
    if inspect.isfunction(attr) or inspect.isbuiltin(attr):
        # Test if it accepts a single argument
        try:
            attr(level_arg)
            callable_obj = attr
        except Exception:
            pass

# If not found, look for a method in a class
if callable_obj is None:
    for name, cls in inspect.getmembers(target, inspect.isclass):
        if hasattr(cls, "isEnabledFor"):
            method = getattr(cls, "isEnabledFor")
            if inspect.isfunction(method) or inspect.ismethod(method):
                # Try to instantiate the class
                instance = None
                try:
                    instance = cls()
                except Exception:
                    # Try with logger and mapping if class is LoggerAdapter
                    instance = create_adapter()
                if instance is not None:
                    try:
                        instance.isEnabledFor(level_arg)
                        callable_obj = instance.isEnabledFor
                        break
                    except Exception:
                        pass

if callable_obj is None:
    print("SKIP=No suitable isEnabledFor callable found")
    sys.exit(0)

# Benchmark parameters
ITERATIONS = 20000000  # 20 million iterations

start = time.perf_counter()
for _ in range(ITERATIONS):
    callable_obj(level_arg)
end = time.perf_counter()

elapsed = end - start
print(f"TIME_SECONDS={elapsed}")