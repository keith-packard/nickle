# Nickle's pretty printer

`print` and `edit` show code, and `nickle --format` (`-F`) prints a
whole file, as Nickle's pretty printer lays it out: a style close to
what clang-format makes of C, with the comments the source held kept
beside the code they describe. This document gives the rules it
follows.

## Where comments belong

Where a comment belongs depends only on where it is:

- A comment starting on the same line as the code before it trails
  that code: the statement, declaration, struct, union, or enum member,
  or parameter ending there, including the comma after a member or
  parameter, or the case label or a case's block, printing after the
  label or the block's `}` when it fits.
- Any other comment leads the code after it, even across blank lines.
  One before a case label stays before the label.
- A comment between the tokens of an expression, or beside a name a
  declaration or a function definition declares, stays between them;
  `print` shows one beside a function's name too, apart from one
  written across lines, which goes above the function. One with code before it on
  its line follows that code, unless code other than a closing `)`,
  `]`, `}`, comma, or semicolon follows it on its line too: then it
  leads the code after it, as one on a line of its own does. One ending
  its line ends a line when printed, unless a closing token follows it,
  and one on a line of its own keeps its line.
- A comment between a control statement's own tokens follows the token
  before it: `if`, `else`, `while`, `for`, `do`, `switch`, `return`,
  `break`, `continue`, `raise`, a header's or a raise's `(` or `)`, or
  the `}` before `else` or `while`. One on a line of its own after a header's `)`, or before
  `else` or `while`, keeps its line, splitting the line there: a block's
  `{`, the `else`, or the `while` then starts a line of its own.
- A comment after the `)` ending a function's, lambda's, catch's, or
  exception's parameters, before its body or `;`, documents it: it
  prints on lines of its own under the parameters, four columns in, and
  a function with one prints with a block.
- A comment among a type's tokens, in a declaration, parameter,
  member, typedef, function definition, or initializer, or among a
  declaration's keywords, stays among them, as does one between a type
  and the name it declares. One between a `(` or `{` and a first
  parameter or member, with code after it on its line, goes before the
  parameter or member; one ending the line of a struct's `{` leads its
  first member. One before a struct's `}` leads the statement.
- Comments elsewhere in a statement lead it, as do those a global
  variable's initializer, declared names, or type holds, and those a
  typedef's type holds, when printing shows the variable's value or the
  typedef.
- A comment holding just `[[fallthrough]]`, as `/* [[fallthrough]] */`
  or `# [[fallthrough]]`, marks the case above it falling through to
  the next, as C's attribute does. Before a case label, it ends the
  statements of the case above it instead, and prints at their level
  with no blank line above it.
- The comments at the top of a file, except one directly above its
  first line of code, and those after its last code belong to the file,
  not to any code.

## Laying out comments

Printed comments are re-indented with their lines as written. The
comment marks go: `/*` and `*/` with any extra `*`s next to them, as in
`/**`, the `*` starting each later line when every line has one, and
the `#` starting each line of a shell-style comment. Each line is then
indented from the least indented one as far as it was, with its tabs
expanded. A comment written with `#` marks prints with them, `# `
starting each line, wherever it keeps lines of its own; since `#` can't
follow code on its line, one that comes to share a line with code, as
between a type's tokens, prints as a block comment. Any other comment
of one line prints as `/* text */`, and longer ones as a block with `*`
starting each line. A trailing comment that doesn't fit after its code
moves above it.

## Laying out code

Printed code fits when its line ends by column 100 and spans at most 80
columns from the start of its statement.

### Spelling

Numbers print as written. Strings and characters print from their
values, as UTF-8, escaping only control characters, quotes, and
backslashes, so an escape such as `\x41` prints as `A`. An operator in
an expression prints as the source spelled it, in ASCII or in UTF-8
(`×`, `≤`, `→`, `◊`, `²`, and the rest); one in a type, such as a
parameter's `...`, prints in ASCII. Names can't hold the characters
operators use, so those print against names as their ASCII spellings
do: `¬a`, `a⧺`, `p→x`, and `a²`.

### Parentheses

An operand keeps parentheses where the grammar needs them, and also
where people misread how operators group, as gcc's `-Wparentheses`
warns: around arithmetic, shifts, comparisons, or a different bitwise
operator inside `&`, `|`, or `^`, as in `(a + b) & 0xff` and `(a & b) |
c`; around arithmetic inside a shift, as in `a >> (i % 4 * 8)`; around a
comparison inside a comparison, as in `(a < b) == c`; and around `&&`
inside `||`. Elsewhere, as in `a + b * c`, they go.

### Types

A struct or union prints on one line only when its members come from
one declaration, as in `struct { int a, b; }`, none has comments, and
the line fits. Otherwise it prints one declaration per line, with the
names lined up past the widest type and trailing comments lined up
after them. The same holds for a struct or union in a declaration, in
an array, hash, or function type, and joined to others with `+`.

An enum prints on one line when none of its values has comments and the
line fits. Otherwise its values print in aligned columns, row by row: a
rectangle when some number of columns that fits divides the number of
values, the one closest to square, and otherwise as many columns as fit
with a short last row. An enum with comments instead fills lines with
its values, a value with comments starting or ending a line.

### Parameters

A function with comments before or after its parameters prints one
parameter per line, lined up the same way; otherwise a function
definition's, lambda's, catch's, or exception's parameters break as call
arguments do, and subscripts break the same way. A parameter that starts
a line in the source, after its comma, starts one when printed, and a
function written with one then keeps its block instead of printing as
`name(args) = value;`. When a catch's or lambda's parameters break, its
`{` goes on a line of its own, as a wrapped header's does. The
parameters of a function type print on one line, with their comments.

### Breaking long statements

A statement too long to fit, its lines ending by column 100 and at most
80 columns past the statement's start, breaks as clang-format breaks C:
call arguments and initializers after their commas, lined up after the
opening parenthesis or brace, or on the lines after it, four columns
in, when that takes no more lines; operators before the operator,
lowest precedence first, lined up right of the enclosing parenthesis or
four columns in, with a chain inside another four columns further, and
one starting the line after a comment ending a line under its first
operand; and `?:` before the `:`, under the `?`. The breaks spread the
text evenly over the fewest lines, except in a list holding a break
from the source or from a comment ending its line: its lines then fill
in order, so they print back the same way.

A statement breaks at its nicest places first, using worse ones only
while a line still passes its end: argument and initializer lists
before operators, operators binding loosest first, places outside
brackets before places inside them, and, only to keep lines within
column 100, places inside subscripts, before the `.` or `->` of member
accesses, and after casts. `is_type` and `has_member` break between
their arguments as calls do.

A for or twixt statement's header that doesn't fit puts each clause on
a line of its own, lined up after the opening parenthesis, as a comma
expression that doesn't fit puts each operand on a line of its own.
When the header of an if, while, for, twixt, or switch breaks, its
block's opening brace goes on a line of its own, and a body without
braces has the header's continued lines four columns further in
instead, so they don't line up with it.

A declaration breaks after a comma between its names where the source
did, and when too long for its line, the next name lined up under the
first. A comment ending the line after a declared name puts its `=` on
the next line, four columns past the name. In a function, an
initializer goes on the line after its `=`, four columns in, when fewer
of its lines pass column 100 there, or as many do and it takes fewer
lines in all. An assignment statement's value goes on the line after
its operator, four columns in, when that fits better than breaking
inside the value.

At the top level, where a newline outside parentheses, brackets, or
braces ends a command, an expression that must break outside them gets
parentheses around it to break inside, as does a declaration of several
names, and a declaration whose initializer fits better on the next
line, as in a function.

### Initializers

Struct and hash initializers break like array initializers. An
initializer holding one too wide for a line of its own opens as a block
instead: its elements one per line, four columns in from the line
holding its `{`, and its `}` on a line of its own under that line's
start.

An initializer written across lines with a trailing comma prints as
rows, as written: its `{` ends the line, each row of items starts a line
four columns in from the statement, breaking only when too long, every
item is followed by a comma, and the `}` starts a line of its own. When
the rows hold the same number of items, the last perhaps fewer, or
every row holds one initializer and those do, the items line up in
columns, as wide as their widest item: numbers on the right and
anything else on the left. Rows too long to line up split evenly over
more lines. At least two lines must be full. A comment on a line of its
own stays on a line of its own, and one ending a row stays at its end,
counting toward its length; in columns, those line up after the widest
row, without widening the columns. An initializer holding one opens the
same way, one item per line.

### Lambdas

A lambda with a block keeps its `{` after its parameters, its body four
columns past where the lambda starts, and its `}` under that start.
An initializer's struct or union type printing on more than one line,
as in `(struct {` ... `}) { x = 1 }`, does the same: its members four
columns past where its `(` lands, and its `}` under that `(`. In
call arguments or an initializer, a lambda with a block starts a line
of its own, lined up after the opening parenthesis or brace. A lambda
goes instead on a line of its own four columns in from the line it
starts on, or its call's arguments start on the line after the
parenthesis, when fewer of its lines pass column 100 there, or as many
do and it takes fewer lines.

### Lining up and blank lines

In function bodies and namespaces, a run of declarations and imports,
which comments don't interrupt, lines up its names past the widest
type, taking `import` as an import's type. Trailing comments on
consecutive lines of any kind line up after the longest of those lines;
a line without one, a comment on its own line, or a blank line starts a
new group. A line whose comments don't all fit there keeps them right
after its code.

Printed code keeps the source's blank lines, one for a run of them,
between statements, declarations, case labels, struct, union, and enum
members, and parameters, and between comments; a blank line inside an
expression, or above a `[[fallthrough]]` comment, is dropped. These
rules add more: above leading comments, in function bodies, among
declarations, and before case labels; above a function definition or
namespace and its leading comments and below it; and below the
declarations starting a block when more than one statement follows
them. No blank line goes just inside a block's, a switch's, or a
namespace's braces, except above a leading comment, function
definition, or namespace starting a block or namespace whose `{` ends
a line of code, as in `if (a) {` or `namespace N {`, nor at the start
of the output, and never two in a row.

## Formatting a file

`nickle --format FILE` prints each top-level command and declaration
as `print` lays it out, with its comments, after the comments heading
the file. Commands that load code, such as `autoimport` and `library`,
still run, since the code after them needs their names. Declarations
are parsed without running, so formatting a file, here or with `print
"file"`, then gives back the names and commands it changed: a variable
keeps its value, and the file's names and what it loaded are gone.

`nickle --check-format FILE` reports the first line of `FILE` that
formatting it would change, and exits with status 1 when formatting
would change any file given. The test `nickle-format-check` checks
Nickle's own sources so, and `ninja nickle-format` formats them: the
`.5c` files the meson build lists, except two tests that say why they
must not be formatted.

Formatting the output again changes nothing. A comment ends a line only
where it ended one in the source, since reading the output again would
give it that line end and break the line there: no line breaks just
after a comment written inline, so a long statement holding one may
break elsewhere, and a value doesn't move past its `=` leaving a comment
alone on a line. A comment just before a closing token, as in
`(a /* c */)`, or before another comment, ends no line. Of the first
statement's leading comments, only the last, just above its code, leads
it; the others print as the file's heading, with a blank line below
them. A global declaration whose value holds a comment on a line of its
own breaks inside parentheses around the value.

## Summaries and full definitions

`print` shows names as a summary, and in full a name written with a
trailing `::`, as in `print N::`, every name for `print ::`, and files.
It takes names (`print N::f, x`) or file names in quotes (`print
"lib.5c"`), whose files print after the names, in the order given.
`Command::format_names(out, full, names...)` and
`Command::format_file(out, in, name, full)` print names, or the file
`in`, named `name` in error messages, to the file `out`, as a summary
or in full.

- `print`, and `format_names` with `full` false, show a variable with
  its current value and a function with its definition. A namespace
  shows a summary: the names in it that aren't private, in the order
  they were declared, functions as declarations without bodies.
- `print` given a name ending in `::`, or `::` alone, and
  `format_names` with `full` true, show a namespace with every name in
  it, functions with their bodies, laid out as formatting a file would
  lay them out: a blank line around each function definition or
  namespace, and no `global` the source wouldn't have written.
  Variables still show their current values.
- `print`, and `format_file` with `full` true, format the file as
  `--format` does. With `full` false, `format_file` shows a summary of
  what the file declares: variables, typedefs, and exceptions as
  written, functions as declarations without bodies, and namespaces
  with their names that aren't private, each with its comments. Other
  commands are left out.
