# Extracted from electrum/lnwatcher.py :: inspect_tx_candidate
# The performance-relevant operation is the per-output address membership check.
# BEFORE: `o.address not in self.get_addresses()` -> get_addresses() returns a
# list, so membership is a linear O(n) scan performed for every output.

class Output:
    def __init__(self, address):
        self.address = address


class Watcher:
    def __init__(self, addresses):
        # backing store of watched addresses (insertion order preserved)
        self._addresses = list(addresses)
        self._addr_set = set(addresses)
        self.added = []

    def get_addresses(self):
        # Returns the list of watched addresses (mirrors original API).
        return self._addresses

    def add_address(self, addr):
        self.added.append(addr)
        self._addresses.append(addr)
        self._addr_set.add(addr)

    def inspect_outputs(self, outputs):
        # Mirrors the loop body of inspect_tx_candidate (recursion/db removed).
        result = []
        for i, o in enumerate(outputs):
            if o.address not in self.get_addresses():
                self.add_address(o.address)
            else:
                result.append((i, o.address))
        return result
