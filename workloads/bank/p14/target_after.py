# Extracted from blivet/devices/lvm.py LVMVolumeGroupDevice.free_space
# Stdlib-only standalone version. The dead-code `raid_disks` calculation
# was removed (no longer needed; space taken by LVs is computed precisely
# elsewhere now).


class Size(int):
    """Minimal stand-in for blivet.size.Size (an int-like quantity)."""
    def __add__(self, other):
        return Size(int(self) + int(other))
    def __radd__(self, other):
        return Size(int(self) + int(other))
    def __sub__(self, other):
        return Size(int(self) - int(other))


class MDRaidArrayDevice(object):
    """Stand-in for an MD RAID array PV (has a list of backing disks)."""
    def __init__(self, disks):
        self.disks = disks


class PlainPV(object):
    """A non-RAID physical volume."""
    pass


class LV(object):
    """Logical volume with precomputed vg_space_used."""
    def __init__(self, vg_space_used):
        self.vg_space_used = vg_space_used


class VG(object):
    def __init__(self, pvs, lvs, size, reserved_space):
        self._pvs = pvs
        self._lvs = lvs
        self._size = size
        self._reserved_space = reserved_space

    @property
    def pvs(self):
        return self._pvs[:]

    @property
    def lvs(self):
        return self._lvs[:]

    @property
    def size(self):
        return self._size

    @property
    def reserved_space(self):
        return self._reserved_space

    @property
    def free_space(self):
        """ The amount of free space in this VG. """
        # TODO: just ask lvm if is_modified returns False

        # total the sizes of any LVs
        used = sum((lv.vg_space_used for lv in self.lvs), Size(0))
        used += self.reserved_space
        free = self.size - used
        return free
