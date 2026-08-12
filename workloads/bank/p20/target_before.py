"""Extracted SentencePieceVocabulary._decode (BEFORE).

The real code calls a sentencepiece tokenizer. We model the tokenizer as a
plain Python object whose unk_id() / GetPieceSize() / DecodeIds() perform
real (minimal) work. No artificial cost is injected: unk_id() and
GetPieceSize() simply return stored integers exactly as the real accessors do,
and DecodeIds() performs a real lookup+join over the id list. The optimization
under test is hoisting the two per-iteration method calls out of the loop.
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
    ids = [
        self.tokenizer.unk_id() if i >= self.tokenizer.GetPieceSize()
        else int(i) for i in ids]
    return self.tokenizer.DecodeIds(ids)
