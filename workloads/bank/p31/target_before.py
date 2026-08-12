import hashlib
import struct

try:
    import cPickle as pickle
except ImportError:
    import pickle

md5_unpacker = struct.Struct('2Q')


def _calc_hash(op, args, keywords):
    """
    Calculates the hash of an AST, given the operation, args, and kwargs.
    """
    args_tup = tuple(a if type(a) in (int, float) else getattr(a, '_hash', hash(a)) for a in args)
    to_hash = (
        op, args_tup,
        str(keywords.get('length', None)),
        hash(keywords['variables']),
        keywords['symbolic'],
        hash(keywords.get('annotations', None)),
    )

    hd = hashlib.md5(pickle.dumps(to_hash, -1)).digest()
    return md5_unpacker.unpack(hd)[0]  # 64 bits
