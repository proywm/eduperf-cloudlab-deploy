import random
import time

import target

random.seed(1234)

# Deterministic input: a realistic-size list of user-defined action names,
# some of which collide with default action names.
ALPHABET = "abcdefghijklmnopqrstuvwxyz"
user_actions = []
for i in range(400):
    if i % 50 == 0:
        # occasionally inject a default action name to exercise the membership test
        user_actions.append("action_listen")
    else:
        user_actions.append("user_action_" + "".join(random.choice(ALPHABET) for _ in range(8)))

ITERS = 2000

start = time.perf_counter()
total = 0
for _ in range(ITERS):
    result = target.combine_user_with_default_actions(user_actions)
    total += len(result)
elapsed = time.perf_counter() - start

# guard against dead-code elimination
assert total > 0
print("TIME_SECONDS=%f" % elapsed)
