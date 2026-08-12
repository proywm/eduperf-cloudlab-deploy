def _colourify(colour_class, text):
    nbsp_text = text.replace(' ', '&nbsp')
    return '<span class="' + colour_class + '">' + nbsp_text + '</span>'
