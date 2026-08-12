"""
Standalone extraction of netaddr EUI.eui64 (BEFORE optimization).

Only the in-file machinery required to run EUI.eui64 is kept:
the EUI class (__init__/value/version/__getitem__/__str__) and the
real eui48 / eui64 strategy logic (string<->int conversion, word splitting).
Stdlib only (re, struct).

Source: netaddr/eui/__init__.py  EUI.eui64  (lines 618-633)
"""
import re as _re
import struct as _struct


class AddrFormatError(Exception):
    pass


# --------------------------------------------------------------------------
#   Base strategy helpers (netaddr/strategy/__init__.py)
# --------------------------------------------------------------------------
def _int_to_words(int_val, word_size, num_words):
    max_int = 2 ** (num_words * word_size) - 1
    if not 0 <= int_val <= max_int:
        raise IndexError('integer out of bounds: %r!' % hex(int_val))
    max_word = 2 ** word_size - 1
    words = []
    for _ in range(num_words):
        word = int_val & max_word
        words.append(int(word))
        int_val >>= word_size
    return tuple(reversed(words))


# --------------------------------------------------------------------------
#   eui48 strategy (netaddr/strategy/eui48.py)
# --------------------------------------------------------------------------
class mac_eui48(object):
    word_size = 8
    num_words = 48 // word_size
    max_word = 2 ** word_size - 1
    word_sep = '-'
    word_fmt = '%.2X'
    word_base = 16


_RE_MAC_FORMATS = (
    '^' + ':'.join(['([0-9A-F]{1,2})'] * 6) + '$',
    '^' + '-'.join(['([0-9A-F]{1,2})'] * 6) + '$',
    '^' + ':'.join(['([0-9A-F]{1,4})'] * 3) + '$',
    '^' + '-'.join(['([0-9A-F]{1,4})'] * 3) + '$',
    '^' + r'\.'.join(['([0-9A-F]{1,4})'] * 3) + '$',
    '^' + '-'.join(['([0-9A-F]{5,6})'] * 2) + '$',
    '^' + ':'.join(['([0-9A-F]{5,6})'] * 2) + '$',
    '^(' + ''.join(['[0-9A-F]'] * 12) + ')$',
    '^(' + ''.join(['[0-9A-F]'] * 11) + ')$',
)
_RE_MAC_FORMATS = [_re.compile(_, _re.IGNORECASE) for _ in _RE_MAC_FORMATS]


class _eui48:
    version = 48
    width = 48
    max_int = 2 ** 48 - 1

    @staticmethod
    def str_to_int(addr):
        words = []
        if isinstance(addr, str):
            found_match = False
            for regexp in _RE_MAC_FORMATS:
                match_result = regexp.findall(addr)
                if len(match_result) != 0:
                    found_match = True
                    if isinstance(match_result[0], tuple):
                        words = match_result[0]
                    else:
                        words = (match_result[0],)
                    break
            if not found_match:
                raise AddrFormatError('%r is not a supported MAC format!' % (addr,))
        else:
            raise TypeError('%r is not str()!' % (addr,))

        if len(words) == 6:
            int_val = int(''.join(['%.2x' % int(w, 16) for w in words]), 16)
        elif len(words) == 3:
            int_val = int(''.join(['%.4x' % int(w, 16) for w in words]), 16)
        elif len(words) == 2:
            int_val = int(''.join(['%.6x' % int(w, 16) for w in words]), 16)
        elif len(words) == 1:
            int_val = int('%012x' % int(words[0], 16), 16)
        else:
            raise AddrFormatError('unexpected word count in MAC address %r!' % (addr,))
        return int_val

    @staticmethod
    def int_to_words(int_val, dialect=None):
        if dialect is None:
            dialect = mac_eui48
        return _int_to_words(int_val, dialect.word_size, dialect.num_words)

    @staticmethod
    def int_to_str(int_val, dialect=None):
        if dialect is None:
            dialect = mac_eui48
        words = _eui48.int_to_words(int_val, dialect)
        tokens = [dialect.word_fmt % i for i in words]
        return dialect.word_sep.join(tokens)


# --------------------------------------------------------------------------
#   eui64 strategy (netaddr/strategy/eui64.py)
# --------------------------------------------------------------------------
class eui64_base(object):
    word_size = 8
    num_words = 64 // word_size
    max_word = 2 ** word_size - 1
    word_sep = '-'
    word_fmt = '%.2X'
    word_base = 16


_RE_EUI64_FORMATS = (
    '^' + ':'.join(['([0-9A-F]{1,2})'] * 8) + '$',
    '^' + '-'.join(['([0-9A-F]{1,2})'] * 8) + '$',
    '^' + ':'.join(['([0-9A-F]{1,4})'] * 4) + '$',
    '^' + '-'.join(['([0-9A-F]{1,4})'] * 4) + '$',
    '^' + r'\.'.join(['([0-9A-F]{1,4})'] * 4) + '$',
    '^(' + ''.join(['[0-9A-F]'] * 16) + ')$',
)
_RE_EUI64_FORMATS = [_re.compile(_, _re.IGNORECASE) for _ in _RE_EUI64_FORMATS]


def _get_match_result(address, formats):
    for regexp in formats:
        match = regexp.findall(address)
        if match:
            return match[0]


class _eui64:
    version = 64
    width = 64
    max_int = 2 ** 64 - 1

    @staticmethod
    def str_to_int(addr):
        try:
            words = _get_match_result(addr, _RE_EUI64_FORMATS)
            if not words:
                raise TypeError
        except TypeError:
            raise AddrFormatError('invalid IEEE EUI-64 identifier: %r!' % (addr,))

        if not isinstance(words, tuple):
            words = (words,)

        if len(words) == 8:
            int_val = int(''.join(['%.2x' % int(w, 16) for w in words]), 16)
        elif len(words) == 4:
            int_val = int(''.join(['%.4x' % int(w, 16) for w in words]), 16)
        elif len(words) == 1:
            int_val = int('%016x' % int(words[0], 16), 16)
        else:
            raise AddrFormatError('bad word count for EUI-64 identifier: %r!' % addr)
        return int_val

    @staticmethod
    def int_to_words(int_val, dialect=None):
        if dialect is None:
            dialect = eui64_base
        return _int_to_words(int_val, dialect.word_size, dialect.num_words)

    @staticmethod
    def int_to_str(int_val, dialect=None):
        if dialect is None:
            dialect = eui64_base
        words = _eui64.int_to_words(int_val, dialect)
        tokens = [dialect.word_fmt % i for i in words]
        return dialect.word_sep.join(tokens)


def _is_int(x):
    return isinstance(x, int) and not isinstance(x, bool)


# --------------------------------------------------------------------------
#   EUI class (netaddr/eui/__init__.py)
# --------------------------------------------------------------------------
class EUI(object):
    def __init__(self, addr, version=None, dialect=None):
        self._value = None
        self._module = None

        if isinstance(addr, EUI):
            if version is not None and version != addr._module.version:
                raise ValueError('cannot switch EUI versions using copy constructor!')
            self._module = addr._module
            self._value = addr._value
            self._dialect = addr._dialect
            return

        if version is not None:
            if version == 48:
                self._module = _eui48
            elif version == 64:
                self._module = _eui64
            else:
                raise ValueError('unsupported EUI version %r' % version)
        else:
            if _is_int(addr):
                if 0 <= addr <= 0xffffffffffff:
                    self._module = _eui48
                elif 0xffffffffffff < addr <= 0xffffffffffffffff:
                    self._module = _eui64

        self._set_value(addr)
        # Default dialect depends on the detected module (netaddr semantics).
        if dialect is None:
            self._dialect = eui64_base if self._module is _eui64 else mac_eui48
        else:
            self._dialect = dialect

    def _set_value(self, value):
        if self._module is None:
            for module in (_eui48, _eui64):
                try:
                    self._value = module.str_to_int(value)
                    self._module = module
                    break
                except AddrFormatError:
                    try:
                        if 0 <= int(value) <= module.max_int:
                            self._value = int(value)
                            self._module = module
                            break
                    except ValueError:
                        pass
            if self._module is None:
                raise AddrFormatError('failed to detect EUI version: %r' % value)
        else:
            if isinstance(value, str):
                try:
                    self._value = self._module.str_to_int(value)
                except AddrFormatError:
                    raise AddrFormatError('address %r is not an EUIv%d'
                                          % (value, self._module.version))
            else:
                if 0 <= int(value) <= self._module.max_int:
                    self._value = int(value)
                else:
                    raise AddrFormatError('bad address format: %r' % value)

    @property
    def version(self):
        return self._module.version

    def __getitem__(self, idx):
        if _is_int(idx):
            num_words = self._dialect.num_words
            if not (-num_words) <= idx <= (num_words - 1):
                raise IndexError('index out range for address type!')
            return self._module.int_to_words(self._value, self._dialect)[idx]
        elif isinstance(idx, slice):
            words = self._module.int_to_words(self._value, self._dialect)
            return [words[i] for i in range(*idx.indices(len(words)))]
        else:
            raise TypeError('unsupported type %r!' % idx)

    def __str__(self):
        return self._module.int_to_str(self._value, self._dialect)

    def __eq__(self, other):
        try:
            return (self.version, self._value) == (other.version, other._value)
        except AttributeError:
            return NotImplemented

    # ----------------------------------------------------------------------
    #   TARGET FUNCTION (BEFORE)
    # ----------------------------------------------------------------------
    def eui64(self):
        """
        - If this object represents an EUI-48 it is converted to EUI-64
            as per the standard.
        - If this object is already and EUI-64, it just returns a new,
            numerically equivalent object is returned instead.

        :return: The value of this EUI object as a new 64-bit EUI object.
        """
        if self.version == 48:
            eui64_words = ["%02x" % i for i in self[0:3]] + ['ff', 'fe'] + \
                     ["%02x" % i for i in self[3:6]]

            return self.__class__('-'.join(eui64_words))
        else:
            return EUI(str(self))
