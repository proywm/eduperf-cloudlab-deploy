def oui_str(int_val):
    """:return: string representation of this OUI"""
    return "%02X-%02X-%02X" % (
            (int_val >> 16) & 0xff,
            (int_val >> 8) & 0xff,
            int_val & 0xff)
