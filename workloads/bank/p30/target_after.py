# Extracted from mwparserfromhell/parser/tokenizer.py (_parse, AFTER)
# Third-party local modules `contexts` and `tokens` are replaced with minimal
# in-file stubs so the function is self-contained and stdlib-only.

import string
from html.entities import entitydefs  # Py3 replacement for Py2 htmlentitydefs


# --- minimal `contexts` stub: bit flags used by _parse ---
class _Contexts(object):
    TEMPLATE_NAME = 1 << 0
    TEMPLATE_PARAM_KEY = 1 << 1
    TEMPLATE_PARAM_VALUE = 1 << 2
    TEMPLATE = TEMPLATE_NAME | TEMPLATE_PARAM_KEY | TEMPLATE_PARAM_VALUE


contexts = _Contexts()


# --- minimal `tokens` stub: lightweight token classes ---
class _Token(object):
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

    def __eq__(self, other):
        return type(self) == type(other) and self.__dict__ == other.__dict__

    def __repr__(self):
        return "{0}({1})".format(type(self).__name__, self.__dict__)


class _Tokens(object):
    class Text(_Token):
        pass

    class TemplateOpen(_Token):
        pass

    class TemplateClose(_Token):
        pass

    class TemplateParamSeparator(_Token):
        pass

    class TemplateParamEquals(_Token):
        pass

    class HTMLEntityStart(_Token):
        pass

    class HTMLEntityNumeric(_Token):
        pass

    class HTMLEntityHex(_Token):
        pass

    class HTMLEntityEnd(_Token):
        pass


tokens = _Tokens()


class _HtmlEntityDefs(object):
    entitydefs = entitydefs


htmlentitydefs = _HtmlEntityDefs()


class BadRoute(Exception):
    pass


class Tokenizer(object):
    START = object()
    END = object()
    SENTINELS = ["{", "}", "[", "]", "|", "=", "&", END]

    def __init__(self):
        self._text = None
        self._head = 0
        self._stacks = []

    @property
    def _stack(self):
        return self._stacks[-1][0]

    @property
    def _context(self):
        return self._stacks[-1][1]

    @_context.setter
    def _context(self, value):
        self._stacks[-1][1] = value

    @property
    def _textbuffer(self):
        return self._stacks[-1][2]

    @_textbuffer.setter
    def _textbuffer(self, value):
        self._stacks[-1][2] = value

    def _push(self, context=0):
        self._stacks.append([[], context, []])

    def _push_textbuffer(self):
        if self._textbuffer:
            self._stack.append(tokens.Text(text="".join(self._textbuffer)))
            self._textbuffer = []

    def _pop(self):
        self._push_textbuffer()
        return self._stacks.pop()[0]

    def _write(self, data, text=False):
        if text:
            self._textbuffer.append(data)
            return
        self._push_textbuffer()
        self._stack.append(data)

    def _write_all(self, tokenlist):
        self._push_textbuffer()
        self._stack.extend(tokenlist)

    def _read(self, delta=0, wrap=False):
        index = self._head + delta
        if index < 0 and (not wrap or abs(index) > len(self._text)):
            return self.START
        try:
            return self._text[index]
        except IndexError:
            return self.END

    def _parse_template(self):
        reset = self._head
        self._head += 2
        try:
            template = self._parse(contexts.TEMPLATE_NAME)
        except BadRoute:
            self._head = reset
            self._write(self._read(), text=True)
        else:
            self._write(tokens.TemplateOpen())
            self._write_all(template)
            self._write(tokens.TemplateClose())

    def _verify_template_name(self):
        self._push_textbuffer()
        if self._stack:
            text = [tok for tok in self._stack if isinstance(tok, tokens.Text)]
            text = "".join([token.text for token in text])
            if text.strip() and "\n" in text.strip():
                raise BadRoute(self._pop())

    def _handle_template_param(self):
        if self._context & contexts.TEMPLATE_NAME:
            self._verify_template_name()
            self._context ^= contexts.TEMPLATE_NAME
        if self._context & contexts.TEMPLATE_PARAM_VALUE:
            self._context ^= contexts.TEMPLATE_PARAM_VALUE
        self._context |= contexts.TEMPLATE_PARAM_KEY
        self._write(tokens.TemplateParamSeparator())

    def _handle_template_param_value(self):
        self._context ^= contexts.TEMPLATE_PARAM_KEY
        self._context |= contexts.TEMPLATE_PARAM_VALUE
        self._write(tokens.TemplateParamEquals())

    def _handle_template_end(self):
        if self._context & contexts.TEMPLATE_NAME:
            self._verify_template_name()
        self._head += 1
        return self._pop()

    def _parse_entity(self):
        reset = self._head
        self._head += 1
        try:
            self._push()
            self._write(tokens.HTMLEntityStart())
            numeric = hexadecimal = False
            if self._read() == "#":
                numeric = True
                self._write(tokens.HTMLEntityNumeric())
                if self._read(1).lower() == "x":
                    hexadecimal = True
                    self._write(tokens.HTMLEntityHex(char=self._read(1)))
                    self._head += 2
                else:
                    self._head += 1
            text = []
            valid = string.hexdigits if hexadecimal else string.digits
            if not numeric and not hexadecimal:
                valid += string.ascii_letters
            while True:
                this = self._read()
                if this == ";":
                    text = "".join(text)
                    if numeric:
                        test = int(text, 16) if hexadecimal else int(text)
                        if test < 1 or test > 0x10FFFF:
                            raise BadRoute(self._pop())
                    else:
                        if text not in htmlentitydefs.entitydefs:
                            raise BadRoute(self._pop())
                    self._write(tokens.Text(text=text))
                    self._write(tokens.HTMLEntityEnd())
                    break
                if this is self.END or this not in valid:
                    raise BadRoute(self._pop())
                text.append(this)
                self._head += 1
        except BadRoute:
            self._head = reset
            self._write(self._read(), text=True)
        else:
            self._write_all(self._pop())

    def _parse(self, context=0):
        self._push(context)
        while True:
            this = self._read()
            if this not in self.SENTINELS:
                self._write(self._read(), text=True)
                self._head += 1
                continue
            if this is self.END:
                if self._context & contexts.TEMPLATE:
                    raise BadRoute(self._pop())
                return self._pop()
            next = self._read(1)
            if this == next == "{":
                self._parse_template()
            elif this == "|" and self._context & contexts.TEMPLATE:
                self._handle_template_param()
            elif this == "=" and self._context & contexts.TEMPLATE_PARAM_KEY:
                self._handle_template_param_value()
            elif this == next == "}" and self._context & contexts.TEMPLATE:
                return self._handle_template_end()
            elif this == "&":
                self._parse_entity()
            else:
                self._write(this, text=True)
            self._head += 1

    def tokenize(self, text):
        self._text = list(text)
        return self._parse()
