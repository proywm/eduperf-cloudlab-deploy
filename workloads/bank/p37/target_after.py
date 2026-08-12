# Extracted from adaptive/learner.py : BalancingLearner.choose_points (lines 462-480)
# AFTER version: uses `operator.itemgetter(1)` as the max key.
from operator import itemgetter


def choose_points(self, n, add_data=True):
    """Choses points for learners."""
    if not add_data:
        raise NotImplementedError('')

    points = []
    for _ in range(n):
        loss_improvements = []
        pairs = []
        for index, learner in enumerate(self.learners):
            point = learner.choose_points(n=1, add_data=False)[0]
            loss_improvements.append(learner.loss_improvement([point]))
            pairs.append((index, point))

        x, _ = max(zip(pairs, loss_improvements), key=itemgetter(1))
        points.append(x)
        self.add_point(x, None)

    return points


def add_point(self, x, y):
    index, x = x
    self.learners[index].add_point(x, y)
