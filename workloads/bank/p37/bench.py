import random
import time

import target


class DataLearner:
    """Pure-data learner stub: returns precomputed values, no injected cost.

    `choose_points` returns a fixed candidate point; `loss_improvement`
    returns a precomputed float from a list (plain data lookup); `add_point`
    just records. No sleeps, no latency, no cost modeling.
    """

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


class Balancer:
    def __init__(self, learners):
        self.learners = learners

    choose_points = target.choose_points
    add_point = target.add_point


def make_balancer(seed, num_learners):
    rng = random.Random(seed)
    learners = [
        DataLearner(point=rng.random(), loss=rng.random())
        for _ in range(num_learners)
    ]
    return Balancer(learners)


def run():
    # n points chosen, each requiring a max over num_learners candidates.
    n = 200
    num_learners = 300
    total = 0
    for trial in range(20):
        b = make_balancer(seed=trial, num_learners=num_learners)
        pts = b.choose_points(n)
        total += len(pts)
    return total


if __name__ == '__main__':
    # warm up
    run()
    t0 = time.perf_counter()
    result = run()
    t1 = time.perf_counter()
    print('RESULT=%d' % result)
    print('TIME_SECONDS=%.6f' % (t1 - t0))
