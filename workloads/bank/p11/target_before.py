def _colourify(colour_class, text):
    list_copy = list(text)
    for i in range(len(list_copy)):
        if list_copy[i] == ' ':
            list_copy[i] = '&nbsp'
    nbsp_text = "".join(list_copy)
    return '<span class="' + colour_class + '">' + nbsp_text + '</span>'
