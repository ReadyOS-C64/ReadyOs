# Calc Plus

Keyboard-first calculator app for ReadyOS with scrolling history and three math modes.

## Input behavior

- Enter evaluates the editor line.
- Up/Down moves history selection.
- Leading operator (`+ - * / ^`) carries from the previous result.
- Prefix `;` forces a fresh expression for that one line (no carry).
  - Example: `;-1+1`

## Operators and constants

- Operators: `+ - * / ^ ( )`
- Exponent input accepts both `^` and C64 up-arrow key.
- In Calc Plus display, exponent renders as PETSCII up-arrow.
- Constant: `PI`

## Modes

- `F7`: cycle `INTEGER -> FIXED -> FLOAT`
- `F6`: toggle trig angle mode in FLOAT (`RAD`/`DEG`)

## Functions

- INTEGER/FIXED: `ABS SGN INT`
- FLOAT: `ABS SGN INT SQR EXP LOG SIN COS TAN ATN RND`

## Variables

- Up to 10 variables, names up to 10 chars.
- Store last result: `STORE $name`
- Assign directly: `$name = expression`
- Use in expressions as `$name`.
- Variable names are case-insensitive and cannot reuse reserved function names.

## Clipboard

- `F1`: copy selected history line or editor text
- `F3`: paste into editor (equation-only content is accepted)

## App/global keys

- `F2/F4`: switch apps (global)
- `CTRL+B`: return to launcher
- `RUN/STOP`: exit app
