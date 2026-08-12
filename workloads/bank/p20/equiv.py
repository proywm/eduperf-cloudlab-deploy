import random

import target_before
import target_after


def build(mod):
  rng = random.Random(1234)
  piece_size = 32000
  pieces = ["p%d" % i for i in range(piece_size)]
  tok = mod._Tokenizer(pieces, unk=2)
  vocab = mod.SentencePieceVocabulary(tok)
  seqs = []
  for _ in range(500):
    n = rng.randint(50, 150)
    seq = []
    for _ in range(n):
      if rng.random() < 0.1:
        seq.append(rng.randint(piece_size, piece_size + 100))
      else:
        seq.append(rng.randint(0, piece_size - 1))
    seqs.append(seq)
  return vocab, seqs


def main():
  vb, sb = build(target_before)
  va, sa = build(target_after)
  ok = True
  for x, y in zip(sb, sa):
    rb = vb._decode(list(x))
    ra = va._decode(list(y))
    if rb != ra:
      ok = False
      break
  print("EQUIV=" + ("yes" if ok else "no"))


if __name__ == "__main__":
  main()
