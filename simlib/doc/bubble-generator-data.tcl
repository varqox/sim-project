# File taken from SQLite 3 Project
# This file contains the data used by the syntax diagram rendering program:
#
#   bubble-generator.tcl
#

# Graphs:
#
set all_graphs {
  config-file {
    line {toploop
      {or
        directive-formal
        {line wsn /newline}
        {line {opt wsn} comment}
      }
    }
  }
  directive {
    line name {or = :} {or {} value {line [ {or {toploop {line {opt comment} value {opt comment}} delimiter} {}} ]}} {opt comment}
  }
  directive-formal {
    stack
      {line {opt ws} name {opt ws} {or = :} {opt ws}}
      {line {or {optx value}
        {line [ {opt delimiter} {or {line {toploop {line {opt comment-or-wsn} value {opt comment-or-wsn}} delimiter}}
          {or comment-or-wsn {}}} ]}}}
      {line {opt ws} {or /newline comment}}
  }
  delimiter {
    toploop
      {line
        {or , /newline}
        {opt comment-or-wsn}
      }
  }
  comment {
    line # {loop {} /anything-except-newline} /newline
  }
  comment-or-wsn {
    or wsn comment {line wsn comment}
  }
  value {
    or
      {line single-quoted-string-literal}
      {line double-quoted-string-literal}
      {line string-literal}
      {line number}
      {boolean}
  }
  string-literal {
    line {
      line {
        stack
          /anything-except-wsn-and-[-and-,-and-]-and-#-and-single-and-double-quote {
            opt {
              stack {opt {loop /anything-except-newline-and-#-and-]-and-,}}
              /anything-except-wsn-and-#-and-]-and-,
            }
          }
        }
    }
  }
  single-quoted-string-literal {
    line ' {opt {toploop {or /anything-except-newline-and-single-quote {line ' '}}}} '
  }
  double-quoted-string-literal {
    line '' {opt {toploop {or /anything-except-\\-and-newline-and-double-quote
      {line \\ '}
      {line \\ ''}
      {line \\ ?}
      {line \\ \\}
      {line \\ /a}
      {line \\ /b}
      {line \\ /f}
      {line \\ /n}
      {line \\ /r}
      {line \\ /t}
      {line \\ /v}
      {line \\ /x hex-digit hex-digit}
    }}}
    ''
  }
  hex-digit {
    toploop {or /digit /a /b /c /d /e /f A B C D E F}
  }
  number {
    line {or {} + -} {loop /digit} {opt /decimal-point {loop /digit}}
  }
  boolean {
    or {or 0 /false /off} {or 1 /true /on}
  }
  name {
    toploop {or /letter /digit - _ . }
  }
  ws {
    loop /any-whitespace-except-newline
  }
  wsn {
    loop /any-whitespace
  }
}
