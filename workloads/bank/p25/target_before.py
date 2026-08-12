def solve(s, iterations):
    pop = [0] * 9
    for t in map(int, s.split(',')):
        pop[t] += 1
    for _ in range(iterations):
        reproducing = pop[0]
        pop = pop[1:] + [reproducing]
        pop[6] += reproducing
    return sum(pop)
