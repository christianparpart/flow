# afk - AWK-style file processing using Flow Language

*afk* is a proof-of-concept demo program utilizing Flow language to process text input
in a customizable way.

### Usage:

```
afk [options] INPUT_FILE
  -f FILENAME           Path to program to process the input
  -w, --watch           Enables watching input for new content instead of terminating at the end
```

### Examples

* accesslog rewriter
* error log parser for alerting on severe messages

### API

```
string FILENAME();            # returns input filename or "<stdin>" for STDIN
string LINE();                # returns the current line being processed
void print(string message);   # prints given message to output along with a line-feed
void terminate(int code);     # terminates processing early
void udp.send(ipaddr, port, text) # sends a UDP message to given target.
```

### Handlers

* `handler init {}` - executed once at startup
* `handler line {}` - executed once every input text
* `handler finally {}` - executed upon termination

### TODO

- The Flow language needs global variables to pass them from init to main to fini.

