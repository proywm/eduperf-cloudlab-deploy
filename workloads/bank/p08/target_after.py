def consume_response(chunks):
    """
    Fully consume the response iterator (AFTER: join)
    """
    return "".join(chunks)
