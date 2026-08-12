import random

import target_before
import target_after

random.seed(1234)

ALPHABET = "abcdefghijklmnopqrstuvwxyz"
user_actions = []
for i in range(400):
    if i % 50 == 0:
        user_actions.append("action_listen")
    else:
        user_actions.append("user_action_" + "".join(random.choice(ALPHABET) for _ in range(8)))

r_before = target_before.combine_user_with_default_actions(user_actions)
r_after = target_after.combine_user_with_default_actions(user_actions)

print("EQUIV=" + ("yes" if r_before == r_after else "no"))
