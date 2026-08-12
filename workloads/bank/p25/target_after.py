def solve(s, iterations):
    pop = [0] * 9
    for t in map(int, s.split(',')):
        pop[t] += 1
    off = 0
    for _ in range(iterations):
        pop[off + 7 - 9] += pop[off]
        off = (off + 1) % 9
    return sum(pop)
