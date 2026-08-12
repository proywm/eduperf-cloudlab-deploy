"""Extracted SentencePieceVocabulary._decode (AFTER).

See target_before.py for modeling notes. Only the loop-hoisting changed.
"""


class _Tokenizer:
  """Minimal stand-in for a sentencepiece processor.

  Stores a vocabulary of pieces; unk_id() / GetPieceSize() return stored ints;
  DecodeIds() does a real piece lookup and join.
  """

  def __init__(self, pieces, unk):
    self._pieces = pieces
    self._unk = unk

  def unk_id(self):
    return self._unk

  def GetPieceSize(self):
    return len(self._pieces)

  def DecodeIds(self, ids):
    pieces = self._pieces
    return "".join(pieces[i] for i in ids)


class SentencePieceVocabulary:

  def __init__(self, tokenizer):
    self.tokenizer = tokenizer

  def _decode(self, ids):
    """Decode a list of integers to a python string.

    Args:
      ids: a list of integers (not terminated by EOS)
    Returns:
      a string
    """
    # convert all the extra ids (sentinels) to UNK=2
    unk_id = self.tokenizer.unk_id()
    piece_size = self.tokenizer.GetPieceSize()
    ids = [unk_id if i >= piece_size else int(i) for i in ids]
    return self.tokenizer.DecodeIds(ids)
