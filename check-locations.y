/*
 * Copyright © 2026 Keith Packard and Bart Massey.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 *
 * meson.build runs each parser generator on this to find one tracking
 * locations (%locations, @$, and @N), which keeping comments needs:
 * bison does, as does byacc built with btyacc's backtracking support
 */

%{
int  yylex(void);
void yyerror(const char *msg);
%}

%locations
%token A

%%

start : A
            { @$ = @1; }
      ;
