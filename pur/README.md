# Pur

Pur is the panuti shell

## Syntax

    command    := expression ( op expression )*
    op         := "&&" | "||" | "&"
    expression := operand ( ">" operand )*
    operand    := word+ | "(" expression ( ";" expression )* ")"

`>` binds tighter than the command operators, which share one precedence and are
left associative. The whole line is checked before any of it runs.

- `a > b` wires a's output into b's input, left associative, so `a > b > c` is
  `(a > b) > c`. `|` is not used - pur pipes with `>`.
- `(a ; b)` is a group, its members run together. A one member group is
  transparent, and an empty group is an error.
- `&&`, `||` and `&` join whole expressions: `&&` runs the next only if the
  status so far is 0, `||` only if it is not 0, `&` whatever the status. They are
  command level only, and a trailing `&` is an error.
- `"..."` keeps the spaces and punctuation inside it.

### Example

    in /cd/log > (cat > wc ; echo done) && "cat done"

`in` reads `/cd/log`, its output fans out to both `cat > wc` and `echo done`, and
the quoted `cat done` runs only if that group succeeds.
