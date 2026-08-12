import re


class Key(str):
    """Key or key combination as string"""

    # Convert some urwid key names and some other stuff
    _INIT = (
        (re.compile(r'^<(.+)>$'),                r'\1'),
        (re.compile(r'^esc$', flags=re.I),       r'escape'),
        (re.compile(r'^ $'),                     r'space'),
        (re.compile(r'^meta', flags=re.I),       r'alt'),
        (re.compile(r'^pos1$', flags=re.I),      r'home'),
        (re.compile(r'^del$', flags=re.I),       r'delete'),
        (re.compile(r'^ins$', flags=re.I),       r'insert'),
        (re.compile(r'^return$', flags=re.I),    r'enter'),
        (re.compile(r'^page up$', flags=re.I),   r'pgup'),
        (re.compile(r'^page down$', flags=re.I), r'pgdn'),
        (re.compile(r'^page dn$', flags=re.I),   r'pgdn'),
        (re.compile(r' '),                       r'-'),
        # The first part in key combos must always be the same, but the part
        # after must be preserved. <alt-l> is not the same as <alt-L>.
        (re.compile(r'^(\w+)-(\S+)$'),
         lambda m: m.group(1).lower()+'-'+m.group(2)),
    )

    _MODS = ('shift', 'alt', 'ctrl')
    _cache = {}

    def __new__(cls, key):
        if isinstance(key, Key):
            return key
        elif key in cls._cache:
            return cls._cache[key]
        else:
            orig_key = key

        # Remove brackets (<>) around key, some renaming, etc.
        for regex,repl in cls._INIT:
            key = regex.sub(repl, key)

        # Convert 'X' to 'shift-x'
        if len(key) == 1 and key.isupper():
            key = 'shift-%s' % key.lower()

        # Validate modifier
        if '-' in key:
            mod, char = key.split('-', 1)
            # If the modifier is '', '-' is the actual key
            if len(mod) > 0:
                if len(char) == 0:
                    raise ValueError('Missing character after modifier: <%s>' % key)
                if mod not in cls._MODS:
                    raise ValueError('Invalid modifier: <%s>' % key)
                if mod == 'shift':
                    # 'shift-E' is the same as 'shift-e'
                    key = key.lower()

        obj = super().__new__(cls, key)
        cls._cache[orig_key] = obj
        return obj

    def __repr__(self):
        return '<%s>' % self
