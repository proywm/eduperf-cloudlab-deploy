import random
import time

import target


def build():
  rng = random.Random(1234)
  piece_size = 32000
  pieces = ["p%d" % i for i in range(piece_size)]
  tok = target._Tokenizer(pieces, unk=2)
  vocab = target.SentencePieceVocabulary(tok)
  # Build a batch of id sequences. Some ids exceed piece_size (sentinels),
  # which exercises the unk branch.
  seqs = []
  for _ in range(4000):
    n = rng.randint(150, 400)
    seq = []
    for _ in range(n):
      if rng.random() < 0.1:
        seq.append(rng.randint(piece_size, piece_size + 100))  # sentinel
      else:
        seq.append(rng.randint(0, piece_size - 1))
    seqs.append(seq)
  return vocab, seqs


def main():
  vocab, seqs = build()
  # warmup
  for s in seqs[:50]:
    vocab._decode(s)

  start = time.perf_counter()
  total = 0
  for _ in range(3):
    for s in seqs:
      out = vocab._decode(s)
      total += len(out)
  elapsed = time.perf_counter() - start
  print("TIME_SECONDS=%f" % elapsed)
  print("checksum=%d" % total)


if __name__ == "__main__":
  main()
