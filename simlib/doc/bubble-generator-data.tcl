# File taken from SQLite 3 Project
# This file contains the data used by the syntax diagram rendering program:
#
#   bubble-generator.tcl
#

# Graphs:
#
set all_graphs {
  config-file {
    line {toploop {or directive-formal {line {opt ws} {or comment /newline}}}}
  }
  directive {
    line /name {or = :} {or {} value {line [ {toploop {line {opt comment} value {opt comment}} ,} ]}} {opt comment}
  }
  directive-formal {
    stack
      {line {opt ws} /name {opt ws} {or = :} {opt ws}}
      {line {or {optx value}
        {line [ {or {line {toploop {line {opt comment-or-wsn} value {opt comment-or-wsn}} ,}}
          {opt comment-or-wsn}} ]}}}
      {line {opt ws} {or /newline comment}}
  }
  comment {
    line # {loop {} /anything-except-newline} /newline
  }
  comment-or-wsn {
    or wsn comment
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
      opt {
        stack
          /anything-except-wsn-and-[-and-,-and-]-and-#-and-single-and-double-quote {
            toploop {
              or /anything-except-newline-and-#-and-]-and-, {line /anything-except-wsn-and-] #}
            }
          }
          {or /anything-except-wsn-and-#-and-]-and-, {line /anything-except-wsn-and-] #}}
      }
    }
  }
  single-quoted-string-literal {
    line ' {opt {toploop {or /anything-except-newline-and-single-quote {line ' '}}}} '
  }
  double-quoted-string-literal {
    line '' {opt {toploop {or /anything-except-\\ -and-newline-and-double-quote
      \\' \\'' \\? \\\\ \\a \\b \\f \\n \\r \\t \\v \\xnn}}} ''
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
