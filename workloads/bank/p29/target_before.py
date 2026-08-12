import math


def shannon(data):
    '''
    Performs a Shannon entropy analysis on a given block of data.
    '''
    entropy = 0

    if data:
        for x in range(0, 256):
            p_x = float(data.count(chr(x))) / len(data)
            if p_x > 0:
                entropy += - p_x*math.log(p_x, 2)

    return (entropy / 8)
