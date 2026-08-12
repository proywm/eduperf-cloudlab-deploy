def consume_response(chunks):
    """
    Fully consume the response iterator (BEFORE: string concatenation in loop)
    """
    data = ""
    for chunk in chunks:
        data += chunk
    return data
