def oui_str(int_val):
    """:return: string representation of this OUI"""
    words = []
    for _ in range(3):
        word = int_val & 0xff
        words.append('%02x' % word)
        int_val >>= 8
    return '-'.join(reversed(words)).upper()
