# Extracted from electrum/lnwatcher.py :: inspect_tx_candidate
# AFTER: `not self.is_mine(o.address)` -> is_mine uses a set membership test,
# which is O(1) instead of the O(n) linear list scan.

class Output:
    def __init__(self, address):
        self.address = address


class Watcher:
    def __init__(self, addresses):
        self._addresses = list(addresses)
        self._addr_set = set(addresses)
        self.added = []

    def get_addresses(self):
        return self._addresses

    def is_mine(self, addr):
        # O(1) set membership (mirrors the wallet's is_mine fast path).
        return addr in self._addr_set

    def add_address(self, addr):
        self.added.append(addr)
        self._addresses.append(addr)
        self._addr_set.add(addr)

    def inspect_outputs(self, outputs):
        result = []
        for i, o in enumerate(outputs):
            if not self.is_mine(o.address):
                self.add_address(o.address)
            else:
                result.append((i, o.address))
        return result
