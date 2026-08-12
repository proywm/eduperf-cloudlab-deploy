"""
Extracted `conll` method from pyconll/unit/conll.py (BEFORE).

The original method iterates over self._sentences and calls sentence.conll()
on each. Here it is extracted as a standalone function that takes the list of
already-rendered sentence strings (sentence.conll() results) as input data.
"""


def conll(sentences):
    """
    Output the Conll object to a CoNLL-U formatted string.

    Args:
        sentences: A list of rendered sentence strings (sentence.conll()).

    Returns:
        The CoNLL-U object as a string. This string will end in a newline.
    """
    # Add newlines along with sentence strings so that there is no need to
    # slice potentially long lists or modify strings.
    components = []
    for sentence in sentences:
        components.append(sentence)
        components.append('\n\n')

    return ''.join(components)
