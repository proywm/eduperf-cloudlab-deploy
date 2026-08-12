import random

import target_before
import target_after


class DataLearner:
    def __init__(self, point, loss):
        self._point = point
        self._loss = loss
        self.added = []

    def choose_points(self, n, add_data=True):
        return [self._point]

    def loss_improvement(self, points):
        return self._loss

    def add_point(self, x, y):
        self.added.append((x, y))


def make_balancer(mod, seed, num_learners):
    rng = random.Random(seed)
    learners = [
        DataLearner(point=rng.random(), loss=rng.random())
        for _ in range(num_learners)
    ]

    class Balancer:
        choose_points = mod.choose_points
        add_point = mod.add_point

    b = Balancer()
    b.learners = learners
    return b


def scenario(n, num_learners, seed):
    bb = make_balancer(target_before, seed, num_learners)
    ba = make_balancer(target_after, seed, num_learners)
    rb = bb.choose_points(n)
    ra = ba.choose_points(n)
    # compare return values
    if rb != ra:
        return False
    # compare mutation: which (index, point) pairs got added to each learner
    mb = [l.added for l in bb.learners]
    ma = [l.added for l in ba.learners]
    if mb != ma:
        return False
    return True


def run():
    cases = [
        (1, 1, 0),       # single point, single learner
        (1, 5, 1),       # single point, several learners
        (3, 1, 2),       # several points, single learner
        (10, 20, 3),     # typical
        (50, 200, 4),    # larger
    ]
    # empty-ish: n=0 produces empty result
    bb = make_balancer(target_before, 9, 3)
    ba = make_balancer(target_after, 9, 3)
    if bb.choose_points(0) != ba.choose_points(0):
        return False

    for n, nl, seed in cases:
        if not scenario(n, nl, seed):
            return False
    return True


if __name__ == '__main__':
    ok = run()
    print('EQUIV=%s' % ('yes' if ok else 'no'))
