"""Extracted parse_content (before)."""


def parse_content(content, remove_first_line=False):
    """Removes new line characters and ¶."""
    content = content.replace('¶', '').strip()

    # removing the starting text of each
    content = content.split('\n')
    if remove_first_line and len(content) > 1:
        content = content[1:]

    content = map(lambda x: x.strip(), content)
    content = ' '.join(text for text in content if text)
    return content
