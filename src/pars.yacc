%{
/*
 * Grace - GRaphing, Advanced Computation and Exploration of data
 * 
 * Home page: http://plasma-gate.weizmann.ac.il/Grace/
 * 
 * Copyright (c) 1991-95 Paul J Turner, Portland, OR
 * Copyright (c) 1996-99 Grace Development Team
 * 
 * Maintained by Evgeny Stambulchik <fnevgeny@plasma-gate.weizmann.ac.il>
 * 
 * 
 *                           All Rights Reserved
 * 
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 * 
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 * 
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*  
 * 
 * evaluate expressions, commands, parameter files
 * 
 */

#include <config.h>
#include <cmath.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#if defined(HAVE_SYS_PARAM_H)
#  include <sys/param.h>
#endif

/* bison not always handles it well itself */
#if defined(HAVE_ALLOCA_H)
#  include <alloca.h>
#endif

#include "defines.h"
#include "globals.h"
#include "cephes/cephes.h"
#include "device.h"
#include "utils.h"
#include "files.h"
#include "graphs.h"
#include "graphutils.h"
#include "plotone.h"
#include "dlmodule.h"
#include "t1fonts.h"
#include "ssdata.h"
#include "protos.h"

extern graph *g;

#define GRVAR_TMP       0
#define GRVAR_SCRARRAY  1
#define GRVAR_DATASET   2

/* variable */
typedef struct _variable {
    int type;
    int length;
    double *data;
} grvar;

static void free_tmpvrbl(grvar *vrbl);
static void copy_vrbl(grvar *dest, grvar *src);
static int find_set_bydata(double *data, target *tgt);

static double  s_result;    /* return value if a scalar expression is scanned*/
static grvar *v_result;    /* return value if a vector expression is scanned*/

static int expr_parsed, vexpr_parsed;

static int interr;

static grvar freelist[100]; 	/* temporary vectors */
static int fcnt = 0;		/* number of the temporary vectors allocated */

static target trgt_pool[100]; 	/* pool of temporary targets */
static int tgtn = 0;		/* number of the temporary targets used */

int naxis = 0;	/* current axis */
static int curline, curbox, curellipse, curstring;
/* these guys attempt to avoid reentrancy problems */
static int gotparams = FALSE, gotread = FALSE, gotnlfit = FALSE; 
int readxformat;
static int nlfit_gno, nlfit_setno, nlfit_nsteps;

char batchfile[GR_MAXPATHLEN] = "",
     paramfile[GR_MAXPATHLEN] = "",
     readfile[GR_MAXPATHLEN] = "";

static char f_string[MAX_STRING_LENGTH]; /* buffer for string to parse */
static int pos;

/* scratch arrays used in scanner */
static int maxarr = 0;
static double *ax = NULL, *bx = NULL, *cx = NULL, *dx = NULL;

/* the graph, set, and its length of the parser's current state */
static int whichgraph;
static int whichset;

/* the graph and set of the left part of a vector assignment */
static int vasgn_gno;
static int vasgn_setno;

static int alias_force = FALSE; /* controls whether aliases can override
                                                       existing keywords */

extern char print_file[];
extern char *close_input;

static int filltype_obs;

static int index_shift = 0;     /* 0 for C, 1 for F77 index notation */

static double rnorm(double mean, double sdev);
static double fx(double x);
static int getcharstr(void);
static void ungetchstr(void);
static int follow(int expect, int ifyes, int ifno);

static double ai_wrap(double x);
static double bi_wrap(double x);
static double ci_wrap(double x);
static double si_wrap(double x);
static double chi_wrap(double x);
static double shi_wrap(double x);
static double fresnlc_wrap(double x);
static double fresnls_wrap(double x);
static double iv_wrap(double v, double x);
static double jv_wrap(double v, double x);
static double kn_wrap(int n, double x);
static double yv_wrap(double v, double x);
static double sqr_wrap(double x);
static double max_wrap(double x, double y);
static double min_wrap(double x, double y);
static double irand_wrap(int x);

/* constants */
static double pi_const(void);
static double deg_uconst(void);
static double rad_uconst(void);

int yylex(void);
int yyparse(void);
void yyerror(char *s);

int findf(symtab_entry *keytable, char *s);

/* Total (intrinsic + user-defined) list of functions and keywords */
symtab_entry *key;

%}

%union {
    long    ival;
    double  dval;
    char   *sval;
    double *dptr;
    target *trgt;
    grvar  *vrbl;
}

%token <ival> INDEX
%token <ival> JDAY
%token <ival> JDAY0

%token <ival> CONSTANT	 /* a (double) constant                                     */
%token <ival> UCONSTANT	 /* a (double) unit constant                                */
%token <ival> FUNC_I	 /* a function of 1 int variable                            */
%token <ival> FUNC_D	 /* a function of 1 double variable                         */
%token <ival> FUNC_NN    /* a function of 2 int parameters                          */
%token <ival> FUNC_ND    /* a function of 1 int parameter and 1 double variable     */
%token <ival> FUNC_DD    /* a function of 2 double variables                        */
%token <ival> FUNC_NND   /* a function of 2 int parameters and 1 double variable    */
%token <ival> FUNC_PPD   /* a function of 2 double parameters and 1 double variable */
%token <ival> FUNC_PPPD  /* a function of 3 double parameters and 1 double variable */
%token <ival> PROC_CONST
%token <ival> PROC_UNIT
%token <ival> PROC_FUNC_I
%token <ival> PROC_FUNC_D
%token <ival> PROC_FUNC_NN
%token <ival> PROC_FUNC_ND
%token <ival> PROC_FUNC_DD
%token <ival> PROC_FUNC_NND
%token <ival> PROC_FUNC_PPD
%token <ival> PROC_FUNC_PPPD

%token <ival> ABOVE
%token <ival> ABSOLUTE
%token <ival> ALIAS
%token <ival> ALT
%token <ival> ALTXAXIS
%token <ival> ALTYAXIS
%token <ival> ANGLE
%token <ival> ANTIALIASING
%token <ival> APPEND
%token <ival> ARRANGE
%token <ival> ARROW
%token <ival> ASCENDING
%token <ival> ASPLINE
%token <ival> AUTO
%token <ival> AUTOSCALE
%token <ival> AUTOTICKS
%token <ival> AVALUE
%token <ival> AVG
%token <ival> AXES
%token <ival> BACKGROUND
%token <ival> BAR
%token <ival> BARDY
%token <ival> BARDYDY
%token <ival> BASELINE
%token <ival> BATCH
%token <ival> BEGIN
%token <ival> BELOW
%token <ival> BETWEEN
%token <ival> BLACKMAN
%token <ival> BLOCK
%token <ival> BOTH
%token <ival> BOTTOM
%token <ival> BOX
%token <ival> CD
%token <ival> CENTER
%token <ival> CHAR
%token <ival> CHART
%token <sval> CHRSTR
%token <ival> CLEAR
%token <ival> CLICK
%token <ival> CLIP
%token <ival> CLOSE
%token <ival> COEFFICIENTS
%token <ival> COLOR
%token <ival> COMMENT
%token <ival> COMPLEX
%token <ival> CONSTRAINTS
%token <ival> COPY
%token <ival> CYCLE
%token <ival> DAYMONTH
%token <ival> DAYOFWEEKL
%token <ival> DAYOFWEEKS
%token <ival> DAYOFYEAR
%token <ival> DDMMYY
%token <ival> DECIMAL
%token <ival> DEF
%token <ival> DEFAULT
%token <ival> DEFINE
%token <ival> DEGREESLAT
%token <ival> DEGREESLON
%token <ival> DEGREESMMLAT
%token <ival> DEGREESMMLON
%token <ival> DEGREESMMSSLAT
%token <ival> DEGREESMMSSLON
%token <ival> DESCENDING
%token <ival> DESCRIPTION
%token <ival> DEVICE
%token <ival> DFT
%token <ival> DIFFERENCE
%token <ival> DISK
%token <ival> DOWN
%token <ival> DPI
%token <ival> DROP
%token <ival> DROPLINE
%token <ival> ECHO
%token <ival> ELLIPSE
%token <ival> ENGINEERING
%token <ival> ERRORBAR
%token <ival> EXIT
%token <ival> EXPONENTIAL
%token <ival> FFT
%token <ival> FILEP
%token <ival> FILL
%token <ival> FIT
%token <ival> FIXED
%token <ival> FIXEDPOINT
%token <ival> FLUSH
%token <ival> FOCUS
%token <ival> FOLLOWS
%token <ival> FONTP
%token <ival> FORCE
%token <ival> FORMAT
%token <ival> FORMULA
%token <ival> FRAMEP
%token <ival> FREE
%token <ival> FREQUENCY
%token <ival> FROM
%token <ival> GENERAL
%token <ival> GETP
%token <ival> GRAPHNO
%token <ival> GRID
%token <ival> HAMMING
%token <ival> HANNING
%token <ival> HARDCOPY
%token <ival> HBAR
%token <ival> HGAP
%token <ival> HIDDEN
%token <ival> HISTO
%token <ival> HMS
%token <ival> HORIZI
%token <ival> HORIZONTAL
%token <ival> HORIZO
%token <ival> ID
%token <ival> IFILTER
%token <ival> IN
%token <ival> INCREMENT
%token <ival> INOUT
%token <ival> INTEGRATE
%token <ival> INTERP
%token <ival> INVDFT
%token <ival> INVERT
%token <ival> INVFFT
%token <ival> JOIN
%token <ival> JUST
%token <ival> KILL
%token <ival> LABEL
%token <ival> LANDSCAPE
%token <ival> LAYOUT
%token <ival> LEFT
%token <ival> LEGEND
%token <ival> LENGTH
%token <ival> LINE
%token <ival> LINESTYLE
%token <ival> LINEWIDTH
%token <ival> LINK
%token <ival> LOAD
%token <ival> LOCTYPE
%token <ival> LOG
%token <ival> LOGARITHMIC
%token <ival> LOGX
%token <ival> LOGXY
%token <ival> LOGY
%token <ival> MAGIC
%token <ival> MAGNITUDE
%token <ival> MAJOR
%token <ival> MAP
%token <ival> MAXP
%token <ival> MINP
%token <ival> MINOR
%token <ival> MMDD
%token <ival> MMDDHMS
%token <ival> MMDDYY
%token <ival> MMDDYYHMS
%token <ival> MMSSLAT
%token <ival> MMSSLON
%token <ival> MMYY
%token <ival> MONTHDAY
%token <ival> MONTHL
%token <ival> MONTHS
%token <ival> MONTHSY
%token <ival> MOVE
%token <ival> NEGATE
%token <ival> NEW
%token <ival> NONE
%token <ival> NONLFIT
%token <ival> NORMAL
%token <ival> NXY
%token <ival> OFF
%token <ival> OFFSET
%token <ival> OFFSETX
%token <ival> OFFSETY
%token <ival> OFILTER
%token <ival> ON
%token <ival> OP
%token <ival> OPPOSITE
%token <ival> OUT
%token <ival> PAGE
%token <ival> PARA
%token <ival> PARAMETERS
%token <ival> PARZEN
%token <ival> PATTERN
%token <ival> PERIOD
%token <ival> PERP
%token <ival> PHASE
%token <ival> PIPE
%token <ival> PLACE
%token <ival> POINT
%token <ival> POLAR
%token <ival> POLYI
%token <ival> POLYO
%token <ival> POP
%token <ival> PORTRAIT
%token <ival> POWER
%token <ival> PREC
%token <ival> PREPEND
%token <ival> PRINT
%token <ival> PS
%token <ival> PUSH
%token <ival> PUTP
%token <ival> READ
%token <ival> REAL
%token <ival> RECIPROCAL
%token <ival> REDRAW
%token <ival> REGNUM
%token <ival> REGRESS
%token <ival> REVERSE
%token <ival> RIGHT
%token <ival> RISER
%token <ival> ROT
%token <ival> ROUNDED
%token <ival> RULE
%token <ival> RUNAVG
%token <ival> RUNMAX
%token <ival> RUNMED
%token <ival> RUNMIN
%token <ival> RUNSTD
%token <ival> SAVEALL
%token <ival> SCALE
%token <ival> SCIENTIFIC
%token <ival> SCROLL
%token <ival> SD
%token <ival> SET
%token <ival> SETNUM
%token <ival> SFORMAT
%token <ival> SIGN
%token <ival> SIZE
%token <ival> SKIP
%token <ival> SLEEP
%token <ival> SMITH 
%token <ival> SORT
%token <ival> SOURCE
%token <ival> SPEC
%token <ival> SPLINE
%token <ival> SPLIT
%token <ival> STACK
%token <ival> STACKED
%token <ival> STACKEDBAR
%token <ival> STACKEDHBAR
%token <ival> STAGGER
%token <ival> START
%token <ival> STOP
%token <ival> STRING
%token <ival> SUBTITLE
%token <ival> SYMBOL
%token <ival> TARGET
%token <ival> TICKLABEL
%token <ival> TICKP
%token <ival> TICKSP
%token <ival> TIMER
%token <ival> TIMESTAMP
%token <ival> TITLE
%token <ival> TO
%token <ival> TOP
%token <ival> TRIANGULAR
%token <ival> TYPE
%token <ival> UP
%token <ival> USE
%token <ival> UNLINK
%token <ival> VERSION
%token <ival> VERTI
%token <ival> VERTICAL
%token <ival> VERTO
%token <ival> VGAP
%token <ival> VIEW
%token <ival> VX1
%token <ival> VX2
%token <ival> VXMAX
%token <ival> VY1
%token <ival> VY2
%token <ival> VYMAX
%token <ival> WELCH
%token <ival> WITH
%token <ival> WORLD
%token <ival> WRITE
%token <ival> WX1
%token <ival> WX2
%token <ival> WY1
%token <ival> WY2
%token <ival> X_TOK
%token <ival> X0
%token <ival> X1
%token <ival> XAXES
%token <ival> XAXIS
%token <ival> XCOR
%token <ival> XMAX
%token <ival> XMIN
%token <ival> XY
%token <ival> XYDX
%token <ival> XYDXDX
%token <ival> XYDXDXDYDY
%token <ival> XYDXDY
%token <ival> XYDY
%token <ival> XYDYDY
%token <ival> XYHILO
%token <ival> XYR
%token <ival> XYSTRING
%token <ival> XYZ
%token <ival> Y_TOK
%token <ival> Y0
%token <ival> Y1
%token <ival> Y2
%token <ival> Y3
%token <ival> Y4
%token <ival> YAXES
%token <ival> YAXIS
%token <ival> YMAX
%token <ival> YMIN
%token <ival> YYMMDD
%token <ival> YYMMDDHMS
%token <ival> ZERO

%token <ival> SCRARRAY

%token <ival> FITPARM
%token <ival> FITPMAX
%token <ival> FITPMIN
%token <dval> NUMBER

%type <ival> onoff

%type <trgt> selectset

%type <ival> pagelayout
%type <ival> pageorient

%type <ival> regiontype

%type <ival> color_select
%type <ival> pattern_select
%type <ival> font_select

%type <ival> lines_select
%type <dval> linew_select

%type <ival> graphtype
%type <ival> xytype

%type <ival> scaletype
%type <ival> signchoice

%type <ival> colpat_obs
%type <ival> direction

%type <ival> formatchoice
%type <ival> inoutchoice
%type <ival> justchoice

%type <ival> opchoice
%type <ival> opchoice_sel
%type <ival> opchoice_obs
%type <ival> opchoice_sel_obs

%type <ival> worldview

%type <ival> filtermethod
%type <ival> filtertype

%type <ival> sourcetype

%type <ival> extremetype
%type <ival> datacolumn

%type <ival> runtype

%type <ival> ffttype
%type <ival> fourierdata
%type <ival> fourierloadx
%type <ival> fourierloady
%type <ival> windowtype

%type <ival> nonlfitopts

%type <ival> sortdir
%type <ival> sorton

%type <ival> proctype

%type <ival> indx

%type <dval> expr
%type <dval> asgn

%type <vrbl> array
%type <vrbl> lside_array

%type <vrbl> vexpr
%type <dptr> vasgn

/* Precedence */
%nonassoc '?' ':'
%left OR
%left AND
%nonassoc GT LT LE GE EQ NE
%right UCONSTANT
%left '+' '-'
%left '*' '/' '%'
%left UMINUS NOT	/* negation--unary minus */
%right '^'		/* exponentiation        */


%%

list:
	parmset {}
	| parmset_obs {}
	| regionset {}
	| setaxis {}
	| set_setprop {}
	| actions {}
	| options {}
        | expr {
            expr_parsed = TRUE;
            s_result = $1;
        }
        | vexpr {
            vexpr_parsed = TRUE;
            v_result = $1;
        }
	| asgn {}
	| vasgn {}
	| error {
	    return 1;
	}
	;



expr:	NUMBER {
	    $$ = $1;
	}
	|  FITPARM {
	    $$ = nonl_parms[$1].value;
	}
	|  FITPMAX {
	    $$ = nonl_parms[$1].max;
	}
	|  FITPMIN {
	    $$ = nonl_parms[$1].min;
	}
	|  array indx {
            if ($2 >= $1->length) {
                errmsg("Access beyond array bounds");
                return 1;
            }
            $$ = $1->data[$2];
	}
	| extremetype '(' vexpr ')' {
	    double dummy;
            int length = $3->length;
	    if ($3->data == NULL) {
		yyerror("NULL variable, check set type");
		return 1;
	    }
	    switch ($1) {
	    case MINP:
		$$ = vmin($3->data, length);
		break;
	    case MAXP:
		$$ = vmax($3->data, length);
		break;
            case AVG:;
		stasum($3->data, length, &$$, &dummy);
                break;
            case SD:
		stasum($3->data, length, &dummy, &$$);
                break;
	    }
	}
	| selectset '.' LENGTH {
	    $$ = getsetlength($1->gno, $1->setno);
	}
	| selectset '.' ID {
	    $$ = $1->setno;
	}
	| GRAPHNO '.' ID {
	    $$ = $1;
	}
	| CONSTANT
	{
	    $$ = key[$1].fnc();
	}
	| expr UCONSTANT
	{
	    $$ = $1 * key[$2].fnc();
	}
	| FUNC_I '(' expr ')'
	{
	    $$ = key[$1].fnc((int) $3);
	}
	| FUNC_D '(' expr ')'
	{
	    $$ = key[$1].fnc($3);
	}
	| FUNC_ND '(' expr ',' expr ')'
	{
	    $$ = key[$1].fnc((int) $3, $5);
	}
	| FUNC_NN '(' expr ',' expr ')'
	{
	    $$ = key[$1].fnc((int) $3, (int) $5);
	}
	| FUNC_DD '(' expr ',' expr ')'
	{
	    $$ = key[$1].fnc($3, $5);
	}
	| FUNC_NND '(' expr ',' expr ',' expr ')'
	{
	    $$ = key[$1].fnc((int) $3, (int) $5, $7);
	}
	| FUNC_PPD '(' expr ',' expr ',' expr ')'
	{
	    $$ = key[$1].fnc($3, $5, $7);
	}
	| FUNC_PPPD '(' expr ',' expr ',' expr ',' expr ')'
	{
	    $$ = key[$1].fnc($3, $5, $7, $9);
	}
	| GRAPHNO '.' VX1 {
	    $$ = g[$1].v.xv1;
	}
	| GRAPHNO '.' VX2 {
	    $$ = g[$1].v.xv2;
	}
	| GRAPHNO '.' VY1 {
	    $$ = g[$1].v.yv1;
	}
	| GRAPHNO '.' VY2 {
	    $$ = g[$1].v.yv2;
	}
	| GRAPHNO '.' WX1 {
	    $$ = g[$1].w.xg1;
	}
	| GRAPHNO '.' WX2 {
	    $$ = g[$1].w.xg2;
	}
	| GRAPHNO '.' WY1 {
	    $$ = g[$1].w.yg1;
	}
	| GRAPHNO '.' WY2 {
	    $$ = g[$1].w.yg2;
	}
	| JDAY '(' expr ',' expr ',' expr ')' { /* yr, mo, day */
	    $$ = julday((int) $5, (int) $7, (int) $3, 12, 0, 0.0);
	}
	| JDAY0 '(' expr ',' expr ',' expr ',' expr ',' expr ',' expr ')' 
	{ /* yr, mo, day, hr, min, sec */
	    $$ = julday((int) $5, (int) $7, (int) $3, (int) $9, (int) $11, (double) $13);
	}
	| VX1 {
	    $$ = g[whichgraph].v.xv1;
	}
	| VX2 {
	    $$ = g[whichgraph].v.xv2;
	}
	| VY1 {
	    $$ = g[whichgraph].v.yv1;
	}
	| VY2 {
	    $$ = g[whichgraph].v.yv2;
	}
	| WX1 {
	    $$ = g[whichgraph].w.xg1;
	}
	| WX2 {
	    $$ = g[whichgraph].w.xg2;
	}
	| WY1 {
	    $$ = g[whichgraph].w.yg1;
	}
	| WY2 {
	    $$ = g[whichgraph].w.yg2;
	}
	| VXMAX {
	    double vx, vy;
            get_page_viewport(&vx, &vy);
            $$ = vx;
	}
	| VYMAX {
	    double vx, vy;
            get_page_viewport(&vx, &vy);
            $$ = vy;
	}
	| '(' expr ')' {
	    $$ = $2;
	}
	| expr '+' expr {
	    $$ = $1 + $3;
	}
	| expr '-' expr {
	    $$ = $1 - $3;
	}
	| '-' expr %prec UMINUS {
	    $$ = -$2;
	}
	| expr '*' expr {
	    $$ = $1 * $3;
	}
	| expr '/' expr
	{
	    if ($3 != 0.0) {
		$$ = $1 / $3;
	    } else {
		yyerror("Divide by zero");
		return 1;
	    }
	}
	| expr '%' expr {
	    if ($3 != 0.0) {
		$$ = fmod($1, $3);
	    } else {
		yyerror("Divide by zero");
		return 1;
	    }
	}
	| expr '^' expr {
	    if ($1 < 0 && rint($3) != $3) {
		yyerror("Negative value raised to non-integer power");
		return 1;
            } else if ($1 == 0.0 && $3 <= 0.0) {
		yyerror("Zero raised to non-positive power");
		return 1;
            } else {
                $$ = pow($1, $3);
            }
	}
	| expr '?' expr ':' expr {
	    $$ = $1 ? $3 : $5;
	}
	| expr GT expr {
	   $$ = ($1 > $3);
	}
	| expr LT expr  {
	   $$ = ($1 < $3);
	}
	| expr LE expr {
	   $$ = ($1 <= $3);
	}
	| expr GE expr {
	   $$ = ($1 >= $3);
	}
	| expr EQ expr {
	   $$ = ($1 == $3);
	}
	| expr NE expr {
	    $$ = ($1 != $3);
	}
	| expr AND expr {
	    $$ = $1 && $3;
	}
	| expr OR expr {
	    $$ = $1 || $3;
	}
	| NOT expr {
	    $$ = !($2);
	}
	;

indx:	'[' expr ']' {
	    int itmp = rint($2);
            if (fabs(itmp - $2) > 1.e-6) {
		yyerror("Non-integer index");
		return 1;
            }
            itmp -= index_shift;
            if (itmp < 0) {
		yyerror("Negative index");
		return 1;
            }
            $$ = itmp;
	}
        ;

array:
	SCRARRAY
	{
	    double *ptr = get_scratch($1);
            $$ = &freelist[fcnt++];
            $$->type = GRVAR_SCRARRAY;
            $$->data = ptr;
            if (ptr == NULL) {
                $$->length = 0;
            } else {
                $$->length = maxarr;
            }
	}
        | datacolumn
	{
	    double *ptr = getcol(whichgraph, whichset, $1);
            $$ = &freelist[fcnt++];
            $$->type = GRVAR_DATASET;
            $$->data = ptr;
            if (ptr == NULL) {
                $$->length = 0;
            } else {
                $$->length = getsetlength(whichgraph, whichset);
            }
	}
	| selectset '.' datacolumn
	{
	    double *ptr = getcol($1->gno, $1->setno, $3);
            $$ = &freelist[fcnt++];
            $$->type = GRVAR_DATASET;
            $$->data = ptr;
            if (ptr == NULL) {
                $$->length = 0;
            } else {
                $$->length = getsetlength($1->gno, $1->setno);
            }
	}
        ;
        
vexpr:
	array
	{
            $$ = &freelist[fcnt++];
            copy_vrbl($$, $1);
	}
	| INDEX
	{
	    int length;
	    length = getsetlength(vasgn_gno, vasgn_setno);
            $$ = &freelist[fcnt++];
            $$->type = GRVAR_TMP;
            $$->data = allocate_index_data(length);
            $$->length = length;
	}
	| FUNC_I '(' vexpr ')'
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;
	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = key[$1].fnc((int) ($3->data[i]));
	    }
	}
	| FUNC_D '(' vexpr ')'
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;
	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = key[$1].fnc(($3->data[i]));
	    }
	}
	| FUNC_DD '(' vexpr ',' vexpr ')'
	{
	    int i;
	    if ($3->length != $5->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;
            
	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = key[$1].fnc($3->data[i], $5->data[i]);
	    }
	}
	| FUNC_DD '(' expr ',' vexpr ')'
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $5);
            $$->type = GRVAR_TMP;
            
	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = key[$1].fnc($3, $5->data[i]);
	    }
	}
	| FUNC_DD '(' vexpr ',' expr ')'
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;
            
	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = key[$1].fnc($3->data[i], $5);
	    }
	}
	| FUNC_ND '(' expr ',' vexpr ')'
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $5);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = key[$1].fnc((int) $3, $5->data[i]);
	    }
	}
	| FUNC_NND '(' expr ',' expr ',' vexpr ')'
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $7);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = key[$1].fnc((int) $3, (int) $5, $7->data[i]);
	    }
	}
	| FUNC_PPD '(' expr ',' expr ',' vexpr ')'
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $7);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = key[$1].fnc($3, $5, $7->data[i]);
	    }
	}
	| FUNC_PPPD '(' expr ',' expr ',' expr ',' vexpr ')'
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $9);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = key[$1].fnc($3, $5, $7, $9->data[i]);
	    }
	}
	| vexpr '+' vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] + $3->data[i];
	    }
	}
	| vexpr '+' expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] + $3;
	    }
	}
	| expr '+' vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1 + $3->data[i];
	    }
	}
	| vexpr '-' vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] - $3->data[i];
	    }
	}
	| vexpr '-' expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] - $3;
	    }
	}
	| expr '-' vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1 - $3->data[i];
	    }
	}
	| vexpr '*' vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] * $3->data[i];
	    }
	}
	| vexpr '*' expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] * $3;
	    }
	}
	| expr '*' vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1 * $3->data[i];
	    }
	}
	| vexpr '/' vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		if ($3->data[i] == 0.0) {
                    errmsg("Divide by zero");
                    return 1;
                }
                $$->data[i] = $1->data[i] / $3->data[i];
	    }
	}
	| vexpr '/' expr
	{
	    int i;
	    if ($3 == 0.0) {
                errmsg("Divide by zero");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] / $3;
	    }
	}
	| expr '/' vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		if ($3->data[i] == 0.0) {
                    errmsg("Divide by zero");
                    return 1;
                }
		$$->data[i] = $1 / $3->data[i];
	    }
	}
	| vexpr '%' vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		if ($3->data[i] == 0.0) {
                    errmsg("Divide by zero");
                    return 1;
                } else {
                    $$->data[i] = fmod($1->data[i], $3->data[i]);
                }
	    }
	}
	| vexpr '%' expr
	{
	    int i;
	    if ($3 == 0.0) {
                errmsg("Divide by zero");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = fmod($1->data[i], $3);
	    }
	}
	| expr '%' vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		if ($3->data[i] == 0.0) {
                    errmsg("Divide by zero");
                    return 1;
                } else {
		    $$->data[i] = fmod($1, $3->data[i]);
                }
	    }
	}
	| vexpr '^' vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
	        if ($1->data[i] < 0 && rint($3->data[i]) != $3->data[i]) {
	            yyerror("Negative value raised to non-integer power");
	            return 1;
                } else if ($1->data[i] == 0.0 && $3->data[i] <= 0.0) {
	            yyerror("Zero raised to non-positive power");
	            return 1;
                } else {
                    $$->data[i] = pow($1->data[i], $3->data[i]);
                }
	    }
	}
	| vexpr '^' expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
	        if ($1->data[i] < 0 && rint($3) != $3) {
	            yyerror("Negative value raised to non-integer power");
	            return 1;
                } else if ($1->data[i] == 0.0 && $3 <= 0.0) {
	            yyerror("Zero raised to non-positive power");
	            return 1;
                } else {
                    $$->data[i] = pow($1->data[i], $3);
                }
	    }
	}
	| expr '^' vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
	        if ($1 < 0 && rint($3->data[i]) != $3->data[i]) {
	            yyerror("Negative value raised to non-integer power");
	            return 1;
                } else if ($1 == 0.0 && $3->data[i] <= 0.0) {
	            yyerror("Zero raised to non-positive power");
	            return 1;
                } else {
                    $$->data[i] = pow($1, $3->data[i]);
                }
	    }
	}
	| vexpr UCONSTANT
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;
	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] * key[$2].fnc();
	    }
	}
	| vexpr '?' expr ':' expr {
            int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;
            for (i = 0; i < $$->length; i++) { 
                $$->data[i] = $1->data[i] ? $3 : $5;
            }
	}
	| vexpr '?' expr ':' vexpr {
            int i;
	    if ($1->length != $5->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;
            for (i = 0; i < $$->length; i++) { 
                $$->data[i] = $1->data[i] ? $3 : $5->data[i];
            }
	}
	| vexpr '?' vexpr ':' expr {
            int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;
            for (i = 0; i < $$->length; i++) { 
                $$->data[i] = $1->data[i] ? $3->data[i] : $5;
            }
	}
	| vexpr '?' vexpr ':' vexpr {
            int i;
	    if ($1->length != $5->length || $1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;
            for (i = 0; i < $$->length; i++) { 
                $$->data[i] = $1->data[i] ? $3->data[i] : $5->data[i];
            }
	}
	| vexpr OR vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] || $3->data[i];
	    }
	}
	| vexpr OR expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] || $3;
	    }
	}
	| expr OR vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1 || $3->data[i];
	    }
	}
	| vexpr AND vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] && $3->data[i];
	    }
	}
	| vexpr AND expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1->data[i] && $3;
	    }
	}
	| expr AND vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = $1 && $3->data[i];
	    }
	}
	| vexpr GT vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] > $3->data[i]);
	    }
	}
	| vexpr GT expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] > $3);
	    }
	}
	| expr GT vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1 > $3->data[i]);
	    }
	}
	| vexpr LT vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] < $3->data[i]);
	    }
	}
	| vexpr LT expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] < $3);
	    }
	}
	| expr LT vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1 < $3->data[i]);
	    }
	}
	| vexpr GE vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] >= $3->data[i]);
	    }
	}
	| vexpr GE expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] >= $3);
	    }
	}
	| expr GE vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1 >= $3->data[i]);
	    }
	}
	| vexpr LE vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] <= $3->data[i]);
	    }
	}
	| vexpr LE expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] <= $3);
	    }
	}
	| expr LE vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1 <= $3->data[i]);
	    }
	}
	| vexpr EQ vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] == $3->data[i]);
	    }
	}
	| vexpr EQ expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] == $3);
	    }
	}
	| expr EQ vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1 == $3->data[i]);
	    }
	}
	| vexpr NE vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] != $3->data[i]);
	    }
	}
	| vexpr NE expr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $1);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1->data[i] != $3);
	    }
	}
	| expr NE vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $3);
            $$->type = GRVAR_TMP;

	    for (i = 0; i < $$->length; i++) {
		$$->data[i] = ($1 != $3->data[i]);
	    }
	}
	| NOT vexpr
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $2);
            $$->type = GRVAR_TMP;
            for (i = 0; i < $$->length; i++) { 
                $$->data[i] = !$2->data[i];
            }
	}
	| '(' vexpr ')'
	{
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $2);
            $$->type = GRVAR_TMP;
            for (i = 0; i < $$->length; i++) { 
                $$->data[i] = $2->data[i];
            }
	}
	| '-' vexpr %prec UMINUS {
	    int i;
            $$ = &freelist[fcnt++];
	    copy_vrbl($$, $2);
            $$->type = GRVAR_TMP;
            for (i = 0; i < $$->length; i++) { 
                $$->data[i] = - $2->data[i];
            }
	}
	;


asgn:
	FITPARM '=' expr
	{
	    nonl_parms[$1].value = $3;
	}
	| FITPMAX '=' expr
	{
	    nonl_parms[$1].max = $3;
	}
	| FITPMIN '=' expr
	{
	    nonl_parms[$1].min = $3;
	}
	| array indx '=' expr
	{
	    if ($2 >= $1->length) {
		yyerror("Access beyond array bounds");
		return 1;
            }
            $1->data[$2] = $4;
	}
	;

lside_array:
        array
        {
            target tgt;
            if (find_set_bydata($1->data, &tgt) == GRACE_EXIT_SUCCESS) {
                vasgn_gno   = tgt.gno;
                vasgn_setno = tgt.setno;
            }
            $$ = $1;
        }
        ;

vasgn:
	lside_array '=' vexpr
	{
	    int i;
	    if ($1->length != $3->length) {
                errmsg("Can't operate on vectors of different lengths");
                return 1;
            }
	    for (i = 0; i < $1->length; i++) {
	        $1->data[i] = $3->data[i];
	    }
	}
	| lside_array '=' expr
	{
	    int i;
	    for (i = 0; i < $1->length; i++) {
	        $1->data[i] = $3;
	    }
	}
        ;

regionset:
	REGNUM onoff {
	    rg[$1].active = $2;
	}
	| REGNUM TYPE regiontype {
	    rg[$1].type = $3;
	}
	| REGNUM color_select {
	    rg[$1].color = $2;
	}
	| REGNUM lines_select {
	    rg[$1].lines = $2;
	}
	| REGNUM linew_select {
	    rg[$1].linew = $2;
	}
	| REGNUM LINE expr ',' expr ',' expr ',' expr
	{
	    rg[$1].x1 = $3;
	    rg[$1].y1 = $5;
	    rg[$1].x2 = $7;
	    rg[$1].y2 = $9;
	}
	| REGNUM XY expr ',' expr
	{
	    if (rg[$1].x == NULL || rg[$1].n == 0) {
		rg[$1].n = 0;
		rg[$1].x = calloc(1, SIZEOF_DOUBLE);
		rg[$1].y = calloc(1, SIZEOF_DOUBLE);
	    } else {
		rg[$1].x = xrealloc(rg[$1].x, (rg[$1].n + 1) * SIZEOF_DOUBLE);
		rg[$1].y = xrealloc(rg[$1].y, (rg[$1].n + 1) * SIZEOF_DOUBLE);
	    }
	    rg[$1].x[rg[$1].n] = $3;
	    rg[$1].y[rg[$1].n] = $5;
	    rg[$1].n++;
	}
	| LINK REGNUM TO GRAPHNO {
	    rg[$2].linkto[$4] = TRUE;
	}
	| UNLINK REGNUM FROM GRAPHNO {
	    rg[$2].linkto[$4]=FALSE;
	}
	;


parmset:
        VERSION NUMBER {
            if (set_project_version((int) $2) != GRACE_EXIT_SUCCESS) {
                errmsg("Project version is newer than software!");
            }
            if (get_project_version() < 50001) {
                map_fonts(FONT_MAP_ACEGR);
            } else {
                map_fonts(FONT_MAP_DEFAULT);
            }
        }
        | PAGE SIZE NUMBER NUMBER {
            Page_geometry pg;
            pg = get_page_geometry();
            pg.width =  (long) $3;
            pg.height = (long) $4;
            set_page_geometry(pg);
        }
        | DEVICE CHRSTR PAGE SIZE NUMBER NUMBER {
            int device_id;
            Device_entry dev;
            
            device_id = get_device_by_name($2);
            if (device_id < 0) {
                yyerror("Unknown device");
            } else {
                dev = get_device_props(device_id);
                dev.pg.width =  (long) $5;
                dev.pg.height = (long) $6;
                set_device_props(device_id, dev);
            }
            free($2);
        }
        | DEVICE CHRSTR DPI NUMBER NUMBER {
            int device_id;
            Device_entry dev;
            
            device_id = get_device_by_name($2);
            if (device_id < 0) {
                yyerror("Unknown device");
            } else {
                dev = get_device_props(device_id);
                dev.pg.dpi_x = (long) $4;
                dev.pg.dpi_y = (long) $5;
                set_device_props(device_id, dev);
            }
            free($2);
        }
        | DEVICE CHRSTR DPI NUMBER {
            int device_id;
            Device_entry dev;
            
            device_id = get_device_by_name($2);
            if (device_id < 0) {
                yyerror("Unknown device");
            } else {
                dev = get_device_props(device_id);
                dev.pg.dpi_x = dev.pg.dpi_y = (long) $4;
                set_device_props(device_id, dev);
            }
            free($2);
        }
        | DEVICE CHRSTR FONTP ANTIALIASING onoff {
            int device_id;
            Device_entry dev;
            
            device_id = get_device_by_name($2);
            if (device_id < 0) {
                yyerror("Unknown device");
            } else {
                dev = get_device_props(device_id);
                dev.fontaa = $5;
                set_device_props(device_id, dev);
            }
            free($2);
        }
        | DEVICE CHRSTR FONTP onoff {
            int device_id;
            Device_entry dev;
            
            device_id = get_device_by_name($2);
            if (device_id < 0) {
                yyerror("Unknown device");
            } else {
                dev = get_device_props(device_id);
                dev.devfonts = $4;
                set_device_props(device_id, dev);
            }
            free($2);
        }
        | DEVICE CHRSTR OP CHRSTR {
            int device_id;
            
            device_id = get_device_by_name($2);
            if (device_id < 0) {
                yyerror("Unknown device");
            } else {
                if (parse_device_options(device_id, $4) != 
                                                        GRACE_EXIT_SUCCESS) {
                    yyerror("Incorrect device option string");
                }
            }
            free($2);
            free($4);
        }
        | HARDCOPY DEVICE CHRSTR {
            set_printer_by_name($3);
            free($3);
        }
	| BACKGROUND color_select {
	    setbgcolor($2);
	}
	| PAGE BACKGROUND FILL onoff {
	    setbgfill($4);
	}
	| PAGE SCROLL NUMBER '%' {
	    scroll_proc((int) $3);
	}
	| PAGE INOUT NUMBER '%' {
	    scrollinout_proc((int) $3);
	}
	| LINK PAGE onoff {
	    scrolling_islinked = $3;
	}

	| STACK WORLD expr ',' expr ',' expr ',' expr
	{
	    add_world(whichgraph, $3, $5, $7, $9);
	}

	| TIMER NUMBER {
            timer_delay = (int) $2;
	}

	| TARGET selectset {
	    target_set = *($2);
	    set_parser_setno(target_set.gno, target_set.setno);
	}
	| WITH GRAPHNO {
	    set_parser_gno($2);
	}
	| WITH selectset {
	    set_parser_setno($2->gno, $2->setno);
	}

/* Hot links */
	| selectset LINK sourcetype CHRSTR {
	    set_hotlink($1->gno, $1->setno, 1, $4, $3);
	    free($4);
	}
	| selectset LINK onoff {
	    set_hotlink($1->gno, $1->setno, $3, NULL, 0);
	}

/* boxes */
	| WITH BOX {
	    curbox = next_box();
	}
	| WITH BOX NUMBER {
	    curbox = (int) $3;
	}
	| BOX onoff {
	    if (!is_valid_box(curbox)) {
                yyerror("Box not active");
	    } else {
	        boxes[curbox].active = $2;
            }
	}
	| BOX GRAPHNO {
	    if (!is_valid_box(curbox)) {
                yyerror("Box not active");
	    } else {
	        boxes[curbox].gno = $2;
            }
	}
	| BOX expr ',' expr ',' expr ',' expr {
	    if (!is_valid_box(curbox)) {
                yyerror("Box not active");
	    } else {
		boxes[curbox].x1 = $2;
		boxes[curbox].y1 = $4;
		boxes[curbox].x2 = $6;
		boxes[curbox].y2 = $8;
	    }
	}
	| BOX LOCTYPE worldview {
	    box_loctype = $3;
	}
	| BOX lines_select {
	    box_lines = $2;
	}
	| BOX linew_select {
	    box_linew = $2;
	}
	| BOX color_select {
	    box_color = $2;
	}
	| BOX FILL color_select {
	    box_fillcolor = $3;
	}
	| BOX FILL pattern_select {
	    box_fillpat = $3;
	}
	| BOX DEF {
	    if (!is_valid_box(curbox)) {
                yyerror("Box not active");
	    } else {
		boxes[curbox].lines = box_lines;
		boxes[curbox].linew = box_linew;
		boxes[curbox].color = box_color;
		if (get_project_version() <= 40102) {
                    switch (filltype_obs) {
                    case COLOR:
                        boxes[curbox].fillcolor = box_fillcolor;
		        boxes[curbox].fillpattern = 1;
                        break;
                    case PATTERN:
                        boxes[curbox].fillcolor = 1;
		        boxes[curbox].fillpattern = box_fillpat;
                        break;
                    default: /* NONE */
                        boxes[curbox].fillcolor = box_fillcolor;
		        boxes[curbox].fillpattern = 0;
                        break;
                    }
		} else {
                    boxes[curbox].fillcolor = box_fillcolor;
		    boxes[curbox].fillpattern = box_fillpat;
                }
                boxes[curbox].loctype = box_loctype;
	    }
	}

/* ellipses */
	| WITH ELLIPSE {
		curellipse = next_ellipse();
	}
	| WITH ELLIPSE NUMBER {
	    curellipse = (int) $3;
	}
	| ELLIPSE onoff {
	    if (!is_valid_ellipse(curellipse)) {
                yyerror("Ellipse not active");
	    } else {
	        ellip[curellipse].active = $2;
            }
	}
	| ELLIPSE GRAPHNO {
	    if (!is_valid_ellipse(curellipse)) {
                yyerror("Ellipse not active");
	    } else {
	        ellip[curellipse].gno = $2;
            }
	}
	| ELLIPSE expr ',' expr ',' expr ',' expr {
	    if (!is_valid_ellipse(curellipse)) {
                yyerror("Ellipse not active");
	    } else {
		ellip[curellipse].x1 = $2;
		ellip[curellipse].y1 = $4;
		ellip[curellipse].x2 = $6;
		ellip[curellipse].y2 = $8;
	    }
	}
	| ELLIPSE LOCTYPE worldview {
	    ellipse_loctype = $3;
	}
	| ELLIPSE lines_select {
	    ellipse_lines = $2;
	}
	| ELLIPSE linew_select {
	    ellipse_linew = $2;
	}
	| ELLIPSE color_select {
	    ellipse_color = $2;
	}
	| ELLIPSE FILL color_select {
	    ellipse_fillcolor = $3;
	}
	| ELLIPSE FILL pattern_select {
	    ellipse_fillpat = $3;
	}
	| ELLIPSE DEF {
	    if (!is_valid_ellipse(curellipse)) {
                yyerror("Ellipse not active");
	    } else {
		ellip[curellipse].lines = ellipse_lines;
		ellip[curellipse].linew = ellipse_linew;
		ellip[curellipse].color = ellipse_color;
		if (get_project_version() <= 40102) {
                    switch (filltype_obs) {
                    case COLOR:
                        ellip[curellipse].fillcolor = ellipse_fillcolor;
		        ellip[curellipse].fillpattern = 1;
                        break;
                    case PATTERN:
                        ellip[curellipse].fillcolor = 1;
		        ellip[curellipse].fillpattern = ellipse_fillpat;
                        break;
                    default: /* NONE */
                        ellip[curellipse].fillcolor = ellipse_fillcolor;
		        ellip[curellipse].fillpattern = 0;
                        break;
                    }
		} else {
                    ellip[curellipse].fillcolor = ellipse_fillcolor;
		    ellip[curellipse].fillpattern = ellipse_fillpat;
                }
		ellip[curellipse].loctype = ellipse_loctype;
	    }
	}

/* lines */
	| WITH LINE {
	    curline = next_line();
	}
	| WITH LINE NUMBER {
	    curline = (int) $3;
	}
	| LINE onoff {
	    if (!is_valid_line(curline)) {
                yyerror("Line not active");
	    } else {
	        lines[curline].active = $2;
            }
	}
	| LINE GRAPHNO {
	    if (!is_valid_line(curline)) {
                yyerror("Line not active");
	    } else {
	        lines[curline].gno = $2;
            }
	}
	| LINE expr ',' expr ',' expr ',' expr {
	    if (!is_valid_line(curline)) {
                yyerror("Line not active");
	    } else {
	        lines[curline].x1 = $2;
	        lines[curline].y1 = $4;
	        lines[curline].x2 = $6;
	        lines[curline].y2 = $8;
            }
	}
	| LINE LOCTYPE worldview {
	    line_loctype = $3;
	}
	| LINE linew_select {
	    line_linew = $2;
	}
	| LINE lines_select {
	    line_lines = $2;
	}
	| LINE color_select {
	    line_color = $2;
	}
	| LINE ARROW NUMBER {
	    line_arrow_end = (int) $3;
	}
	| LINE ARROW LENGTH expr {
	    line_asize = $4;
	}
	| LINE ARROW TYPE NUMBER {
	    line_atype = (int) $4;
	}
	| LINE ARROW LAYOUT expr ',' expr {
	    line_a_dL_ff = $4;
	    line_a_lL_ff = $6;
	}
	| LINE DEF {
	    if (!is_valid_line(curline)) {
                yyerror("Line not active");
	    } else {
	        lines[curline].lines = line_lines;
	        lines[curline].linew = line_linew;
	        lines[curline].color = line_color;
	        lines[curline].arrow_end = line_arrow_end;
	        lines[curline].arrow.length = line_asize;
	        lines[curline].arrow.type = line_atype;
	        lines[curline].arrow.dL_ff = line_a_dL_ff;
	        lines[curline].arrow.lL_ff = line_a_lL_ff;
	        lines[curline].loctype = line_loctype;
            }
	}

/* strings */
	| WITH STRING {
            curstring = next_string();
        }
	| WITH STRING NUMBER {
            curstring = (int) $3;
        }
	| STRING onoff {
	    if (!is_valid_string(curstring)) {
                yyerror("String not active");
	    } else {
                pstr[curstring].active = $2;
            }
        }
	| STRING GRAPHNO {
	    if (!is_valid_string(curstring)) {
                yyerror("String not active");
	    } else {
                pstr[curstring].gno = $2;
            }
        }
	| STRING expr ',' expr {
	    if (!is_valid_string(curstring)) {
                yyerror("String not active");
	    } else {
	        pstr[curstring].x = $2;
	        pstr[curstring].y = $4;
            }
	}
	| STRING LOCTYPE worldview {
            string_loctype = $3;
        }
	| STRING color_select {
            string_color = $2;
        }
	| STRING ROT NUMBER {
            string_rot = (int) $3;
        }
	| STRING font_select {
            string_font = $2;
        }
	| STRING JUST NUMBER {
            string_just = (int) $3;
        }
	| STRING CHAR SIZE NUMBER {
            string_size = $4;
        }
	| STRING DEF CHRSTR {
	    if (!is_valid_string(curstring)) {
                yyerror("String not active");
	    } else {
	        set_plotstr_string(&pstr[curstring], $3);
	        pstr[curstring].color = string_color;
	        pstr[curstring].font = string_font;
	        pstr[curstring].just = string_just;
	        pstr[curstring].loctype = string_loctype;
	        pstr[curstring].rot = string_rot;
	        pstr[curstring].charsize = string_size;
            }
	    free($3);
	}

/* timestamp */
	| TIMESTAMP onoff {
            timestamp.active = $2;
        }
	| TIMESTAMP font_select {
            timestamp.font = $2;
        }
	| TIMESTAMP CHAR SIZE NUMBER {
            timestamp.charsize = $4;
        }
	| TIMESTAMP ROT NUMBER {
            timestamp.rot = (int) $3;
        }
	| TIMESTAMP color_select {
            timestamp.color = $2;
        }
	| TIMESTAMP NUMBER ',' NUMBER {
	    timestamp.x = $2;
	    timestamp.y = $4;
	}
	| TIMESTAMP DEF CHRSTR {
	  set_plotstr_string(&timestamp, $3);
	  free($3);
	}

/* defaults */
	| DEFAULT lines_select {
	    grdefaults.lines = $2;
	}
	| DEFAULT linew_select {
	    grdefaults.linew = $2;
	}
	| DEFAULT color_select {
	    grdefaults.color = $2;
	}
	| DEFAULT pattern_select {
	    grdefaults.pattern = $2;
	}
	| DEFAULT CHAR SIZE NUMBER {
	    grdefaults.charsize = $4;
	}
	| DEFAULT font_select {
	    grdefaults.font = $2;
	}
	| DEFAULT SYMBOL SIZE NUMBER {
	    grdefaults.symsize = $4;
	}
	| DEFAULT SFORMAT CHRSTR {
	    strcpy(sformat, $3);
	    free($3);
	}
	| MAP FONTP NUMBER TO CHRSTR ',' CHRSTR {
	    if ((map_font_by_name($5, (int) $3) == GRACE_EXIT_SUCCESS) || 
                (map_font_by_name($7, (int) $3) == GRACE_EXIT_SUCCESS)) {
                ;
            } else {
                errmsg("Failed mapping a font");
                map_font(0, (int) $3);
            }
            free($5);
	    free($7);
	}
	| MAP COLOR NUMBER TO '(' NUMBER ',' NUMBER ',' NUMBER ')' ',' CHRSTR {
	    CMap_entry cmap;
            cmap.rgb.red   = $6;
            cmap.rgb.green = $8;
            cmap.rgb.blue  = $10;
            cmap.ctype = COLOR_MAIN;
            cmap.cname = $13;
            if (store_color((int) $3, cmap) == GRACE_EXIT_FAILURE) {
                errmsg("Failed mapping a color");
            }
	    free($13);
        }

	| WORLD expr ',' expr ',' expr ',' expr {
	    g[whichgraph].w.xg1 = $2;
	    g[whichgraph].w.yg1 = $4;
	    g[whichgraph].w.xg2 = $6;
	    g[whichgraph].w.yg2 = $8;
	}
	| WORLD XMIN expr {
	    g[whichgraph].w.xg1 = $3;
	}
	| WORLD XMAX expr {
	    g[whichgraph].w.xg2 = $3;
	}
	| WORLD YMIN expr {
	    g[whichgraph].w.yg1 = $3;
	}
	| WORLD YMAX expr {
	    g[whichgraph].w.yg2 = $3;
	}
	| VIEW expr ',' expr ',' expr ',' expr {
	    g[whichgraph].v.xv1 = $2;
	    g[whichgraph].v.yv1 = $4;
	    g[whichgraph].v.xv2 = $6;
	    g[whichgraph].v.yv2 = $8;
	}
	| VIEW XMIN expr {
	    g[whichgraph].v.xv1 = $3;
	}
	| VIEW XMAX expr {
	    g[whichgraph].v.xv2 = $3;
	}
	| VIEW YMIN expr {
	    g[whichgraph].v.yv1 = $3;
	}
	| VIEW YMAX expr {
	    g[whichgraph].v.yv2 = $3;
	}
	| TITLE CHRSTR {
	    set_plotstr_string(&g[whichgraph].labs.title, $2);
	    free($2);
	}
	| TITLE font_select {
	    g[whichgraph].labs.title.font = $2;
	}
	| TITLE SIZE NUMBER {
	    g[whichgraph].labs.title.charsize = $3;
	}
	| TITLE color_select {
	    g[whichgraph].labs.title.color = $2;
	}
	| SUBTITLE CHRSTR {
	    set_plotstr_string(&g[whichgraph].labs.stitle, $2);
	    free($2);
	}
	| SUBTITLE font_select {
	    g[whichgraph].labs.stitle.font = $2;
	}
	| SUBTITLE SIZE NUMBER {
	    g[whichgraph].labs.stitle.charsize = $3;
	}
	| SUBTITLE color_select {
	    g[whichgraph].labs.stitle.color = $2;
	}

	| XAXES SCALE scaletype {
	    g[whichgraph].xscale = $3;
	}
	| YAXES SCALE scaletype {
	    g[whichgraph].yscale = $3;
	}
	| XAXES INVERT onoff {
	    g[whichgraph].xinvert = $3;
	}
	| YAXES INVERT onoff {
	    g[whichgraph].yinvert = $3;
	}

	| DESCRIPTION CHRSTR {
            char *s;
            s = copy_string(NULL, get_project_description());
            s = concat_strings(s, $2);
	    free($2);
            s = concat_strings(s, "\n");
            set_project_description(s);
            free(s);
	}
        | CLEAR DESCRIPTION {
            set_project_description(NULL);
        }

	| LEGEND onoff {
	    g[whichgraph].l.active = $2;
	}
	| LEGEND LOCTYPE worldview {
	    g[whichgraph].l.loctype = $3;
	}
	| LEGEND VGAP NUMBER {
            g[whichgraph].l.vgap = (int) $3;
	}
	| LEGEND HGAP NUMBER {
	    g[whichgraph].l.hgap = (int) $3;
	}
	| LEGEND LENGTH NUMBER {
	    g[whichgraph].l.len = (int) $3;
	}
	| LEGEND INVERT onoff {
	    g[whichgraph].l.invert = $3;
        }
	| LEGEND BOX FILL color_select {
	    g[whichgraph].l.boxfillpen.color = $4;
        }
	| LEGEND BOX FILL pattern_select {
	    g[whichgraph].l.boxfillpen.pattern = $4;
        }
	| LEGEND BOX color_select {
	    g[whichgraph].l.boxpen.color = $3;
	}
	| LEGEND BOX pattern_select {
	    g[whichgraph].l.boxpen.pattern = $3;
	}
	| LEGEND BOX lines_select {
	    g[whichgraph].l.boxlines = $3;
	}
	| LEGEND BOX linew_select {
	    g[whichgraph].l.boxlinew = $3;
	}
	| LEGEND expr ',' expr {
	    g[whichgraph].l.legx = $2;
	    g[whichgraph].l.legy = $4;
	}
	| LEGEND X1 expr {
	    g[whichgraph].l.legx = $3;
	}
	| LEGEND Y1 expr {
	    g[whichgraph].l.legy = $3;
	}
	| LEGEND CHAR SIZE NUMBER {
	    g[whichgraph].l.charsize = $4;
	}
	| LEGEND font_select {
	    g[whichgraph].l.font = $2;
	}
	| LEGEND color_select {
	    g[whichgraph].l.color = $2;
	}
	| LEGEND STRING NUMBER CHRSTR {
	    strcpy(g[whichgraph].p[(int) $3].lstr, $4);
	    free($4);
	}

	| FRAMEP onoff {
            g[whichgraph].f.pen.pattern = $2;
	}
	| FRAMEP TYPE NUMBER {
	    g[whichgraph].f.type = (int) $3;
	}
	| FRAMEP lines_select {
	    g[whichgraph].f.lines = $2;
	}
	| FRAMEP linew_select {
	    g[whichgraph].f.linew = $2;
	}
	| FRAMEP color_select {
	    g[whichgraph].f.pen.color = $2;
	}
	| FRAMEP pattern_select {
	    g[whichgraph].f.pen.pattern = $2;
	}
	| FRAMEP BACKGROUND color_select
        { 
            g[whichgraph].f.fillpen.color = $3;
        }
	| FRAMEP BACKGROUND pattern_select
        {
            g[whichgraph].f.fillpen.pattern = $3;
        }

	| GRAPHNO onoff {
            set_graph_active($1, $2);
        }
	| GRAPHNO HIDDEN onoff {
            set_graph_hidden($1, $3);
        }
	| GRAPHNO TYPE graphtype {
            set_graph_type($1, $3);
        }
	| GRAPHNO STACKED onoff {
            set_graph_stacked($1, $3);
        }

	| GRAPHNO BAR HGAP expr {
	    set_graph_bargap($1, $4);
	}
        
	| GRAPHNO FIXEDPOINT onoff {
            g[$1].locator.pointset = $3;
        }
	| GRAPHNO FIXEDPOINT FORMAT formatchoice formatchoice {
	    g[$1].locator.fx = $4;
	    g[$1].locator.fy = $5;
	}
	| GRAPHNO FIXEDPOINT PREC NUMBER ',' NUMBER {
	    g[$1].locator.px = $4;
	    g[$1].locator.py = $6;
	}
	| GRAPHNO FIXEDPOINT XY expr ',' expr {
	    g[$1].locator.dsx = $4;
	    g[$1].locator.dsy = $6;
	}
	| GRAPHNO FIXEDPOINT TYPE NUMBER {
            g[$1].locator.pt_type = (int) $4;
        }
        
	| TYPE xytype {
	    curtype = $2;
	}


	| ALIAS CHRSTR CHRSTR {
	    int position;
	    lowtoupper($2);
	    lowtoupper($3);
	    if ((position = findf(key, $3)) >= 0) {
	        symtab_entry tmpkey;
		tmpkey.s = malloc(strlen($2) + 1);
		strcpy(tmpkey.s, $2);
		tmpkey.type = key[position].type;
		tmpkey.fnc = key[position].fnc;
		if (addto_symtab(tmpkey) != 0) {
		    yyerror("Keyword already exists");
		}
		free (tmpkey.s);
	    } else {
	        yyerror("Aliased keyword not found");
	    }
	    free($2);
	    free($3);
	}
	| ALIAS FORCE onoff {
	    alias_force = $3;
	}
	| USE CHRSTR TYPE proctype FROM CHRSTR {
	    if (load_module($6, $2, $2, $4) != 0) {
	        yyerror("DL module load failed");
	    }
	    free($2);
	    free($6);
	}
	| USE CHRSTR TYPE proctype FROM CHRSTR ALIAS CHRSTR {
	    if (load_module($6, $2, $8, $4) != 0) {
	        yyerror("DL module load failed");
	    }
	    free($2);
	    free($6);
	    free($8);
	}

/* I/O filters */
	| DEFINE filtertype CHRSTR filtermethod CHRSTR {
	    if (add_io_filter($2, $4, $5, $3) != 0) {
	        yyerror("Failed adding i/o filter");
	    }
	    free($3);
	    free($5);
	}
	| CLEAR filtertype {
	    clear_io_filters($2);
	}

	| SOURCE sourcetype {
	    cursource = $2;
	}
	| FORMAT formatchoice {
	    readxformat = $2;
	}
        | FIT nonlfitopts { }
	| FITPARM CONSTRAINTS onoff {
	    nonl_parms[$1].constr = $3;
	}
	;

actions:
	REDRAW {
	    drawgraph();
	}
	| CD CHRSTR {
	    set_workingdir($2);
	    free($2);
	}
	| ECHO CHRSTR {
	    echomsg($2);
	    free($2);
	}
	| ECHO expr {
	    char buf[32];
            sprintf(buf, "%g", (double) $2);
            echomsg(buf);
	}
	| CLOSE {
	    close_input = copy_string(close_input, "");
	}
	| CLOSE CHRSTR {
	    close_input = copy_string(close_input, $2);
	}
	| EXIT {
	    exit(0);
	}
	| PRINT {
	    do_hardcopy();
	}
	| PRINT TO DEVICE {
            set_ptofile(FALSE);
	}
	| PRINT TO CHRSTR {
            set_ptofile(TRUE);
	    strcpy(print_file, $3);
            free($3);
	}
	| PAGE direction {
	    switch ($2) {
	    case UP:
		graph_scroll(GSCROLL_UP);
		break;
	    case DOWN:
		graph_scroll(GSCROLL_DOWN);
		break;
	    case RIGHT:
		graph_scroll(GSCROLL_RIGHT);
		break;
	    case LEFT:
		graph_scroll(GSCROLL_LEFT);
		break;
	    case IN:
		graph_zoom(GZOOM_SHRINK);
		break;
	    case OUT:
		graph_zoom(GZOOM_EXPAND);
		break;
	    }
	}
	| SLEEP NUMBER {
	    if ($2 > 0) {
	        msleep_wrap((unsigned int) (1000 * $2));
	    }
	}
	| GETP CHRSTR {
	    gotparams = TRUE;
	    strcpy(paramfile, $2);
	    free($2);
	}
	| PUTP CHRSTR {
	    FILE *pp = grace_openw($2);
	    if (pp != NULL) {
	        putparms(whichgraph, pp, 0);
	        grace_close(pp);
	    }
	    free($2);
	}
	| selectset HIDDEN onoff {
	    set_set_hidden($1->gno, $1->setno, $3);
	}
	| selectset LENGTH NUMBER {
	    setlength($1->gno, $1->setno, (int) $3);
	}
	| selectset POINT expr ',' expr {
	    add_point($1->gno, $1->setno, $3, $5);
	}

	| selectset DROP NUMBER ',' NUMBER {
	    int start = (int) $3 - 1;
	    int stop = (int) $5 - 1;
	    int dist = stop - start + 1;
	    if (dist > 0 && start >= 0) {
	        droppoints($1->gno, $1->setno, start, stop, dist);
	    }
	}
	| SORT selectset sorton sortdir {
	    if (is_set_active($2->gno, $2->setno)) {
	        sortset($2->gno, $2->setno, $3, $4 == ASCENDING ? 0 : 1);
	    }
	}
	| COPY selectset TO selectset {
	    do_copyset($2->gno, $2->setno, $4->gno, $4->setno);
	}
	| APPEND selectset TO selectset {
	    if ($2->gno != $4->gno) {
                errmsg("Can't append sets from different graphs");
            } else {
                int sets[2];
	        sets[0] = $4->setno;
	        sets[1] = $2->setno;
	        join_sets($2->gno, sets, 2);
            }
	}
	| REVERSE selectset {
            reverse_set($2->gno, $2->setno);
	}
	| SPLIT selectset NUMBER {
            do_splitsets($2->gno, $2->setno, (int) $3);
	}
	| MOVE selectset TO selectset {
	    do_moveset($2->gno, $2->setno, $4->gno, $4->setno);
	}
	| KILL selectset {
	    killset($2->gno, $2->setno);
	}
	| KILL selectset SAVEALL {
            killsetdata($2->gno, $2->setno);
        }
	| KILL GRAPHNO {
            kill_graph($2);
        }
	| FLUSH {
            wipeout();
        }
	| ARRANGE NUMBER ',' NUMBER {
            arrange_graphs((int) $2, (int) $4);
        }
	| LOAD SCRARRAY NUMBER ',' expr ',' expr {
	    int i, ilen = (int) $3;
            double *ptr;
	    if (ilen < 0) {
		yyerror("Length of array < 0");
		return 1;
	    } else if (ilen > maxarr) {
		init_scratch_arrays(ilen);
	    }
            ptr = get_scratch((int) $2);
	    for (i = 0; i < ilen; i++) {
		ptr[i] = $5 + $7 * i;
	    }
	}
	| NONLFIT '(' selectset ',' NUMBER ')' {
	    gotnlfit = TRUE;
	    nlfit_gno = $3->gno;
	    nlfit_setno = $3->setno;
	    nlfit_nsteps = (int) $5;
	}
	| REGRESS '(' selectset ',' NUMBER ')' {
	    do_regress($3->gno, $3->setno, (int) $5, 0, -1, 0, -1);
	}
	| runtype '(' selectset ',' NUMBER ')' {
	    do_runavg($3->gno, $3->setno, (int) $5, $1, -1, 0);
	}
	| ffttype '(' selectset ',' NUMBER ')' {
	    do_fourier_command($3->gno, $3->setno, $1, (int) $5);
	}
        | ffttype '(' selectset ',' fourierdata ',' windowtype ',' 
                      fourierloadx ','  fourierloady ')' {
	    switch ($1) {
	    case FFT_DFT:
                do_fourier($3->gno, $3->setno, 0, $11, $9, 0, $5, $7);
	        break;
	    case FFT_INVDFT    :
                do_fourier($3->gno, $3->setno, 0, $11, $9, 1, $5, $7);
	        break;
	    case FFT_FFT:
                do_fourier($3->gno, $3->setno, 1, $11, $9, 0, $5, $7);
	        break;
	    case FFT_INVFFT    :
                do_fourier($3->gno, $3->setno, 1, $11, $9, 1, $5, $7);
	        break;
	    default:
                errmsg("Internal error");
	        break;
	    }
        }
	| SPLINE '(' selectset ',' expr ',' expr ',' NUMBER ')' {
	    do_spline($3->gno, $3->setno, $5, $7, (int) $9, SPLINE_CUBIC);
	}
	| ASPLINE '(' selectset ',' expr ',' expr ',' NUMBER ')' {
	    do_spline($3->gno, $3->setno, $5, $7, (int) $9, SPLINE_AKIMA);
	}
	| INTERP '(' selectset ',' selectset ',' NUMBER ')' {
	    do_interp($3->gno, $3->setno, $5->gno, $5->setno, (int) $7);
	}
	| HISTO '(' selectset ',' expr ',' expr ',' NUMBER ')' {
            do_histo($3->gno, $3->setno, SET_SELECT_NEXT, -1, $7, $5, $5 + ((int) $9)*$7, 
                                        HISTOGRAM_TYPE_ORDINARY);
	}
	| DIFFERENCE '(' selectset ',' NUMBER ')' {
	    do_differ($3->gno, $3->setno, (int) $5);
	}
	| INTEGRATE '(' selectset ')' {
	    do_int($3->gno, $3->setno, 0);
	}
 	| XCOR '(' selectset ',' selectset ',' NUMBER ')' {
	    do_xcor($3->gno, $3->setno, $5->gno, $5->setno, (int) $7);
	}
	| AUTOSCALE {
	    if (activeset(whichgraph)) {
		autoscale_graph(whichgraph, AUTOSCALE_XY);
	    } else {
		errmsg("No active sets!");
	    }
	}
	| AUTOSCALE XAXES {
	    if (activeset(whichgraph)) {
		autoscale_graph(whichgraph, AUTOSCALE_X);
	    } else {
		errmsg("No active sets!");
	    }
	}
	| AUTOSCALE YAXES {
	    if (activeset(whichgraph)) {
                autoscale_graph(whichgraph, AUTOSCALE_Y);
	    } else {
		errmsg("No active sets!");
	    }
	}
	| AUTOSCALE selectset {
	    autoscale_byset($2->gno, $2->setno, AUTOSCALE_XY);
	}
        | AUTOTICKS {
            autotick_axis(whichgraph, ALL_AXES);
        }
	| FOCUS GRAPHNO {
	    int gno = $2;
            if (is_graph_hidden(gno) == FALSE) {
                select_graph(gno);
            } else {
		errmsg("Graph is not active");
            }
	}
	| READ CHRSTR {
	    gotread = TRUE;
	    strcpy(readfile, $2);
	    free($2);
	}
	| READ BATCH CHRSTR {
	    strcpy(batchfile, $3);
	    free($3);
	}
	| READ BLOCK CHRSTR {
	    getdata(whichgraph, $3, SOURCE_DISK, LOAD_BLOCK);
	    free($3);
	}
	| READ BLOCK sourcetype CHRSTR {
	    getdata(whichgraph, $4, $3, LOAD_BLOCK);
	    free($4);
	}
	| BLOCK xytype CHRSTR {
	    create_set_fromblock(whichgraph, $2, $3);
	    free($3);
	}
	| READ xytype CHRSTR {
	    gotread = TRUE;
	    curtype = $2;
	    strcpy(readfile, $3);
	    free($3);
	}
	| READ xytype sourcetype CHRSTR {
	    gotread = TRUE;
	    strcpy(readfile, $4);
	    curtype = $2;
	    cursource = $3;
	    free($4);
	}
	| WRITE selectset {
	    outputset($2->gno, $2->setno, NULL, NULL);
	}
	| WRITE selectset FORMAT CHRSTR {
	    outputset($2->gno, $2->setno, NULL, $4);
	    free($4);
	}
	| WRITE selectset FILEP CHRSTR {
	    outputset($2->gno, $2->setno, $4, NULL);
	    free($4);
	}
	| WRITE selectset FILEP CHRSTR FORMAT CHRSTR {
	    outputset($2->gno, $2->setno, $4, $6);
	    free($4);
	    free($6);
	}
        | SAVEALL CHRSTR {
            save_project($2);
            free($2);
        }
        | LOAD CHRSTR {
            load_project($2);
            free($2);
        }
        | NEW {
            new_project(NULL);
        }
        | NEW FROM CHRSTR {
            new_project($3);
            free($3);
        }
	| PUSH {
	    push_world();
	}
	| POP {
	    pop_world();
	}
	| CYCLE {
	    cycle_world_stack();
	}
	| STACK NUMBER {
	    if ((int) $2 > 0)
		show_world_stack((int) $2 - 1);
	}
	| CLEAR STACK {
	    clear_world_stack();
	}
	| CLEAR BOX {
	    do_clear_boxes();
	}
	| CLEAR ELLIPSE {
	    do_clear_ellipses();
	}
	| CLEAR LINE {
	    do_clear_lines();
	}
	| CLEAR STRING {
	    do_clear_text();
	}
        ;


options:
        PAGE LAYOUT pagelayout {
#ifndef NONE_GUI
            set_pagelayout($3);
#endif
        }
	| AUTO REDRAW onoff {
	    auto_redraw = $3;
	}
	| FOCUS onoff {
	    draw_focus_flag = $2;
	}
	| FOCUS SET {
	    focus_policy = FOCUS_SET;
	}
	| FOCUS FOLLOWS {
	    focus_policy = FOCUS_FOLLOWS;
	}
	| FOCUS CLICK {
	    focus_policy = FOCUS_CLICK;
	}
        ;


set_setprop:
	setprop {}
	| setprop_obs {}
	;

setprop:
	selectset onoff {
	    set_set_hidden($1->gno, $1->setno, !$2);
	}
	| selectset TYPE xytype {
	    set_dataset_type($1->gno, $1->setno, $3);
	}

	| selectset SYMBOL NUMBER {
	    g[$1->gno].p[$1->setno].sym = (int) $3;
	}
	| selectset SYMBOL color_select {
	    g[$1->gno].p[$1->setno].sympen.color = $3;
	}
	| selectset SYMBOL pattern_select {
	    g[$1->gno].p[$1->setno].sympen.pattern = $3;
	}
	| selectset SYMBOL linew_select {
	    g[$1->gno].p[$1->setno].symlinew = $3;
	}
	| selectset SYMBOL lines_select {
	    g[$1->gno].p[$1->setno].symlines = $3;
	}
	| selectset SYMBOL FILL color_select {
	    g[$1->gno].p[$1->setno].symfillpen.color = $4;
	}
	| selectset SYMBOL FILL pattern_select {
	    g[$1->gno].p[$1->setno].symfillpen.pattern = $4;
	}
	| selectset SYMBOL SIZE expr {
	    g[$1->gno].p[$1->setno].symsize = $4;
	}
	| selectset SYMBOL CHAR NUMBER {
	    g[$1->gno].p[$1->setno].symchar = (int) $4;
	}
	| selectset SYMBOL CHAR font_select {
	    g[$1->gno].p[$1->setno].charfont = $4;
	}
	| selectset SYMBOL SKIP NUMBER {
	    g[$1->gno].p[$1->setno].symskip = (int) $4;
	}

	| selectset LINE TYPE NUMBER
        {
	    g[$1->gno].p[$1->setno].linet = (int) $4;
	}
	| selectset LINE lines_select
        {
	    g[$1->gno].p[$1->setno].lines = $3;
	}
	| selectset LINE linew_select
        {
	    g[$1->gno].p[$1->setno].linew = $3;
	}
	| selectset LINE color_select
        {
	    g[$1->gno].p[$1->setno].linepen.color = $3;
	}
	| selectset LINE pattern_select
        {
	    g[$1->gno].p[$1->setno].linepen.pattern = $3;
	}

	| selectset FILL TYPE NUMBER
        {
	    g[$1->gno].p[$1->setno].filltype = (int) $4;
	}
	| selectset FILL RULE NUMBER
        {
	    g[$1->gno].p[$1->setno].fillrule = (int) $4;
	}
	| selectset FILL color_select
        {
	    int prop = $3;

	    if (get_project_version() <= 40102 && get_project_version() >= 30000) {
                switch (filltype_obs) {
                case COLOR:
                    break;
                case PATTERN:
                    prop = 1;
                    break;
                default: /* NONE */
	            prop = 0;
                    break;
                }
	    }
	    g[$1->gno].p[$1->setno].setfillpen.color = prop;
	}
	| selectset FILL pattern_select
        {
	    int prop = $3;

	    if (get_project_version() <= 40102) {
                switch (filltype_obs) {
                case COLOR:
                    prop = 1;
                    break;
                case PATTERN:
                    break;
                default: /* NONE */
	            prop = 0;
                    break;
                }
	    }
	    g[$1->gno].p[$1->setno].setfillpen.pattern = prop;
	}

        
	| selectset BASELINE onoff
        {
	    g[$1->gno].p[$1->setno].baseline = $3;
	}
	| selectset BASELINE TYPE NUMBER
        {
	    g[$1->gno].p[$1->setno].baseline_type = (int) $4;
	}
        
	| selectset DROPLINE onoff
        {
	    g[$1->gno].p[$1->setno].dropline = $3;
	}

	| selectset AVALUE onoff
        {
	    g[$1->gno].p[$1->setno].avalue.active = $3;
	}
	| selectset AVALUE TYPE NUMBER
        {
	    g[$1->gno].p[$1->setno].avalue.type = (int) $4;
	}
	| selectset AVALUE CHAR SIZE NUMBER
        {
	    g[$1->gno].p[$1->setno].avalue.size = $5;
	}
	| selectset AVALUE font_select
        {
	    g[$1->gno].p[$1->setno].avalue.font = $3;
	}
	| selectset AVALUE color_select
        {
	    g[$1->gno].p[$1->setno].avalue.color = $3;
	}
	| selectset AVALUE ROT NUMBER
        {
	    g[$1->gno].p[$1->setno].avalue.angle = (int) $4;
	}
	| selectset AVALUE FORMAT formatchoice
        {
	    g[$1->gno].p[$1->setno].avalue.format = $4;
	}
	| selectset AVALUE PREC NUMBER
        {
	    g[$1->gno].p[$1->setno].avalue.prec = (int) $4;
	}
	| selectset AVALUE OFFSET expr ',' expr {
	    g[$1->gno].p[$1->setno].avalue.offset.x = $4;
	    g[$1->gno].p[$1->setno].avalue.offset.y = $6;
	}
	| selectset AVALUE PREPEND CHRSTR
        {
	    strcpy(g[$1->gno].p[$1->setno].avalue.prestr, $4);
	    free($4);
	}
	| selectset AVALUE APPEND CHRSTR
        {
	    strcpy(g[$1->gno].p[$1->setno].avalue.appstr, $4);
	    free($4);
	}

	| selectset ERRORBAR onoff {
	    g[$1->gno].p[$1->setno].errbar.active = $3;
	}
	| selectset ERRORBAR opchoice_sel {
	    g[$1->gno].p[$1->setno].errbar.ptype = $3;
	}
	| selectset ERRORBAR color_select {
	    g[$1->gno].p[$1->setno].errbar.pen.color = $3;
	}
	| selectset ERRORBAR pattern_select {
	    g[$1->gno].p[$1->setno].errbar.pen.pattern = $3;
	}
	| selectset ERRORBAR SIZE NUMBER {
            g[$1->gno].p[$1->setno].errbar.barsize = $4;
	}
	| selectset ERRORBAR linew_select {
            g[$1->gno].p[$1->setno].errbar.linew = $3;
	}
	| selectset ERRORBAR lines_select {
            g[$1->gno].p[$1->setno].errbar.lines = $3;
	}
	| selectset ERRORBAR RISER linew_select {
            g[$1->gno].p[$1->setno].errbar.riser_linew = $4;
	}
	| selectset ERRORBAR RISER lines_select {
            g[$1->gno].p[$1->setno].errbar.riser_lines = $4;
	}
	| selectset ERRORBAR RISER CLIP onoff {
            g[$1->gno].p[$1->setno].errbar.arrow_clip = $5;
	}
	| selectset ERRORBAR RISER CLIP LENGTH NUMBER {
            g[$1->gno].p[$1->setno].errbar.cliplen = $6;
	}

	| selectset COMMENT CHRSTR {
	    strncpy(g[$1->gno].p[$1->setno].comments, $3, MAX_STRING_LENGTH - 1);
	    free($3);
	}
        
	| selectset LEGEND CHRSTR {
	    strncpy(g[$1->gno].p[$1->setno].lstr, $3, MAX_STRING_LENGTH - 1);
	    free($3);
	}
	;


axisfeature:
	onoff {
	    g[whichgraph].t[naxis].active = $1;
	}
	| TYPE ZERO onoff {
	    g[whichgraph].t[naxis].zero = $3;
	}
	| TICKP tickattr {}
	| TICKP tickattr_obs {}
	| TICKLABEL ticklabelattr {}
	| TICKLABEL ticklabelattr_obs {}
	| LABEL axislabeldesc {}
	| LABEL axislabeldesc_obs {}
	| BAR axisbardesc {}
	| OFFSET expr ',' expr {
            g[whichgraph].t[naxis].offsx = $2;
	    g[whichgraph].t[naxis].offsy = $4;
	}
	;

tickattr:
	onoff {
	    g[whichgraph].t[naxis].t_flag = $1;
	}
	| MAJOR expr {
            g[whichgraph].t[naxis].tmajor = $2;
	}
	| MINOR TICKSP NUMBER {
	    g[whichgraph].t[naxis].nminor = (int) $3;
	}
	| PLACE ROUNDED onoff {
	    g[whichgraph].t[naxis].t_round = $3;
	}

	| OFFSETX expr {
            g[whichgraph].t[naxis].offsx = $2;
	}
	| OFFSETY expr {
            g[whichgraph].t[naxis].offsy = $2;
	}
	| DEFAULT NUMBER {
	    g[whichgraph].t[naxis].t_autonum = (int) $2;
	}
	| inoutchoice {
	    g[whichgraph].t[naxis].t_inout = $1;
	}
	| MAJOR SIZE NUMBER {
	    g[whichgraph].t[naxis].props.size = $3;
	}
	| MINOR SIZE NUMBER {
	    g[whichgraph].t[naxis].mprops.size = $3;
	}
	| color_select {
	    g[whichgraph].t[naxis].props.color = g[whichgraph].t[naxis].mprops.color = $1;
	}
	| MAJOR color_select {
	    g[whichgraph].t[naxis].props.color = $2;
	}
	| MINOR color_select {
	    g[whichgraph].t[naxis].mprops.color = $2;
	}
	| linew_select {
	    g[whichgraph].t[naxis].props.linew = g[whichgraph].t[naxis].mprops.linew = $1;
	}
	| MAJOR linew_select {
	    g[whichgraph].t[naxis].props.linew = $2;
	}
	| MINOR linew_select {
	    g[whichgraph].t[naxis].mprops.linew = $2;
	}
	| MAJOR lines_select {
	    g[whichgraph].t[naxis].props.lines = $2;
	}
	| MINOR lines_select {
	    g[whichgraph].t[naxis].mprops.lines = $2;
	}
	| MAJOR GRID onoff {
	    g[whichgraph].t[naxis].props.gridflag = $3;
	}
	| MINOR GRID onoff {
	    g[whichgraph].t[naxis].mprops.gridflag = $3;
	}
	| opchoice_sel {
	    g[whichgraph].t[naxis].t_op = $1;
	}
	| TYPE AUTO {
	    g[whichgraph].t[naxis].t_type = TYPE_AUTO;
	}
	| TYPE SPEC {
	    g[whichgraph].t[naxis].t_type = TYPE_SPEC;
	}
	| SPEC NUMBER {
	    g[whichgraph].t[naxis].nticks = (int) $2;
	}
	| MAJOR NUMBER ',' expr {
	    g[whichgraph].t[naxis].tloc[(int) $2].wtpos = $4;
	    g[whichgraph].t[naxis].tloc[(int) $2].type = TICK_TYPE_MAJOR;
	}
	| MINOR NUMBER ',' expr {
	    g[whichgraph].t[naxis].tloc[(int) $2].wtpos = $4;
	    g[whichgraph].t[naxis].tloc[(int) $2].type = TICK_TYPE_MINOR;
	}
	;

ticklabelattr:
	onoff {
	    g[whichgraph].t[naxis].tl_flag = $1;
	}
	| TYPE AUTO {
	    g[whichgraph].t[naxis].tl_type = TYPE_AUTO;
	}
	| TYPE SPEC {
	    g[whichgraph].t[naxis].tl_type = TYPE_SPEC;
	}
	| PREC NUMBER {
	    g[whichgraph].t[naxis].tl_prec = (int) $2;
	}
	| FORMAT formatchoice {
	    g[whichgraph].t[naxis].tl_format = $2;
	}
	| FORMAT NUMBER {
	    g[whichgraph].t[naxis].tl_format = $2;
	}
	| APPEND CHRSTR {
	    strcpy(g[whichgraph].t[naxis].tl_appstr, $2);
	    free($2);
	}
	| PREPEND CHRSTR {
	    strcpy(g[whichgraph].t[naxis].tl_prestr, $2);
	    free($2);
	}
	| ANGLE NUMBER {
	    g[whichgraph].t[naxis].tl_angle = (int) $2;
	}
	| SKIP NUMBER {
	    g[whichgraph].t[naxis].tl_skip = (int) $2;
	}
	| STAGGER NUMBER {
	    g[whichgraph].t[naxis].tl_staggered = (int) $2;
	}
	| opchoice_sel {
	    g[whichgraph].t[naxis].tl_op = $1;
	}
	| SIGN signchoice {
	    g[whichgraph].t[naxis].tl_sign = $2;
	}
	| START expr {
	    g[whichgraph].t[naxis].tl_start = $2;
	}
	| STOP expr {
	    g[whichgraph].t[naxis].tl_stop = $2;
	}
	| START TYPE SPEC {
	    g[whichgraph].t[naxis].tl_starttype = TYPE_SPEC;
	}
	| START TYPE AUTO {
	    g[whichgraph].t[naxis].tl_starttype = TYPE_AUTO;
	}
	| STOP TYPE SPEC {
	    g[whichgraph].t[naxis].tl_stoptype = TYPE_SPEC;
	}
	| STOP TYPE AUTO {
	    g[whichgraph].t[naxis].tl_stoptype = TYPE_AUTO;
	}
	| CHAR SIZE NUMBER {
	    g[whichgraph].t[naxis].tl_charsize = $3;
	}
	| font_select {
	    g[whichgraph].t[naxis].tl_font = $1;
	}
	| color_select {
	    g[whichgraph].t[naxis].tl_color = $1;
	}
	| NUMBER ',' CHRSTR {
	    g[whichgraph].t[naxis].tloc[(int) $1].label = 
                copy_string(g[whichgraph].t[naxis].tloc[(int) $1].label, $3);
	    free($3);
	}
	| OFFSET AUTO {
	    g[whichgraph].t[naxis].tl_gaptype = TYPE_AUTO;
	}
	| OFFSET SPEC {
	    g[whichgraph].t[naxis].tl_gaptype = TYPE_SPEC;
	}
	| OFFSET expr ',' expr {
	    g[whichgraph].t[naxis].tl_gap.x = $2;
	    g[whichgraph].t[naxis].tl_gap.y = $4;
	}
	;

axislabeldesc:
	CHRSTR {
	    set_plotstr_string(&g[whichgraph].t[naxis].label, $1);
	    free($1);
	}
	| LAYOUT PERP {
	    g[whichgraph].t[naxis].label_layout = LAYOUT_PERPENDICULAR;
	}
	| LAYOUT PARA {
	    g[whichgraph].t[naxis].label_layout = LAYOUT_PARALLEL;
	}
	| PLACE AUTO {
	    g[whichgraph].t[naxis].label_place = TYPE_AUTO;
	}
	| PLACE SPEC {
	    g[whichgraph].t[naxis].label_place = TYPE_SPEC;
	}
	| PLACE expr ',' expr {
	    g[whichgraph].t[naxis].label.x = $2;
	    g[whichgraph].t[naxis].label.y = $4;
	}
	| JUST justchoice {
	    g[whichgraph].t[naxis].label.just = $2;
	}
	| CHAR SIZE NUMBER {
	    g[whichgraph].t[naxis].label.charsize = $3;
	}
	| font_select {
	    g[whichgraph].t[naxis].label.font = $1;
	}
	| color_select {
	    g[whichgraph].t[naxis].label.color = $1;
	}
	| opchoice_sel {
	    g[whichgraph].t[naxis].label_op = $1;
	}
	;

axisbardesc:
	onoff {
	    g[whichgraph].t[naxis].t_drawbar = $1;
	}
	| color_select {
	    g[whichgraph].t[naxis].t_drawbarcolor = $1;
	}
	| lines_select {
	    g[whichgraph].t[naxis].t_drawbarlines = $1;
	}
	| linew_select {
	    g[whichgraph].t[naxis].t_drawbarlinew = $1;
	}
	;

nonlfitopts:
        TITLE CHRSTR { 
          strcpy(nonl_opts.title, $2);
	  free($2);
        }
        | FORMULA CHRSTR { 
          strcpy(nonl_opts.formula, $2);
	  free($2);
        }
        | WITH NUMBER PARAMETERS { 
            nonl_opts.parnum = (int) $2; 
        }
        | PREC NUMBER { 
            nonl_opts.tolerance = $2; 
        }
        ;

selectset:
	GRAPHNO '.' SETNUM
	{
	    int gno = $1, setno = $3;
            allocate_set(gno, setno);
            $$ = &trgt_pool[tgtn];
            $$->gno   = gno;
            $$->setno = setno;
            tgtn++;
	}
	| SETNUM
	{
	    int gno = whichgraph, setno = $1;
            allocate_set(gno, setno);
	    $$ = &trgt_pool[tgtn];
            $$->gno   = gno;
            $$->setno = setno;
            tgtn++;
	}
	;

setaxis:
	axis axisfeature {}
	| GRAPHNO axis axisfeature {}
	;

axis:
	XAXIS { naxis =  X_AXIS; }
	| YAXIS { naxis = Y_AXIS; }
	| ALTXAXIS { naxis = ZX_AXIS; }
	| ALTYAXIS { naxis = ZY_AXIS; }
	;

proctype:
        PROC_CONST        { $$ = CONSTANT; }
        | PROC_UNIT      { $$ = UCONSTANT; }
        | PROC_FUNC_I       { $$ = FUNC_I; }
	| PROC_FUNC_D       { $$ = FUNC_D; }
	| PROC_FUNC_ND     { $$ = FUNC_ND; }
	| PROC_FUNC_NN     { $$ = FUNC_NN; }
	| PROC_FUNC_DD     { $$ = FUNC_DD; }
	| PROC_FUNC_NND   { $$ = FUNC_NND; }
	| PROC_FUNC_PPD   { $$ = FUNC_PPD; }
	| PROC_FUNC_PPPD { $$ = FUNC_PPPD; }
	;


filtertype:
        IFILTER       { $$ = FILTER_INPUT; }
	| OFILTER    { $$ = FILTER_OUTPUT; }
	;
	
filtermethod:
        MAGIC         { $$ = FILTER_MAGIC; }
	| PATTERN   { $$ = FILTER_PATTERN; }
	;
	
xytype:
	XY { $$ = SET_XY; }
	| BAR { $$ = SET_BAR; }
	| BARDY { $$ = SET_BARDY; }
	| BARDYDY { $$ = SET_BARDYDY; }
	| XYZ { $$ = SET_XYZ; }
	| XYDX { $$ = SET_XYDX; }
	| XYDY { $$ = SET_XYDY; }
	| XYDXDX { $$ = SET_XYDXDX; }
	| XYDYDY { $$ = SET_XYDYDY; }
	| XYDXDY { $$ = SET_XYDXDY; }
	| XYDXDXDYDY { $$ = SET_XYDXDXDYDY; }
	| XYHILO { $$ = SET_XYHILO; }
	| XYR { $$ = SET_XYR; }
	| XYSTRING { $$ = SET_XYSTRING; }
	;

graphtype:
	XY { $$ = GRAPH_XY; }
	| CHART { $$ = GRAPH_CHART; }
	| POLAR { $$ = GRAPH_POLAR; }
	| SMITH { $$ = GRAPH_SMITH; }
	| FIXED { $$ = GRAPH_FIXED; }
	;
        
pagelayout:
        FREE { $$ = PAGE_FREE; }
        | FIXED { $$ = PAGE_FIXED; }
        ;

pageorient:
        LANDSCAPE  { $$ = PAGE_ORIENT_LANDSCAPE; }
        | PORTRAIT { $$ = PAGE_ORIENT_PORTRAIT;  }
        ;

regiontype:
	ABOVE { $$ = REGION_ABOVE; }
	|  BELOW { $$ = REGION_BELOW; }
	|  LEFT { $$ = REGION_TOLEFT; }
	|  RIGHT { $$ = REGION_TORIGHT; }
	|  POLYI { $$ = REGION_POLYI; }
	|  POLYO { $$ = REGION_POLYO; }
	|  HORIZI { $$ = REGION_HORIZI; }
	|  VERTI { $$ = REGION_VERTI; }
	|  HORIZO { $$ = REGION_HORIZO; }
	|  VERTO { $$ = REGION_VERTO; }
	;

scaletype: NORMAL { $$ = SCALE_NORMAL; }
	| LOGARITHMIC { $$ = SCALE_LOG; }
	| RECIPROCAL { $$ = SCALE_REC; }
	;

onoff: ON { $$ = TRUE; }
	| OFF { $$ = FALSE; }
	;

runtype: RUNAVG { $$ = RUN_AVG; }
	| RUNSTD { $$ = RUN_STD; }
	| RUNMED { $$ = RUN_MED; }
	| RUNMAX { $$ = RUN_MAX; }
	| RUNMIN { $$ = RUN_MIN; }
	;

sourcetype: 
        DISK { $$ = SOURCE_DISK; }
	| PIPE { $$ = SOURCE_PIPE; }
	;

justchoice: RIGHT { $$ = JUST_RIGHT; }
	| LEFT { $$ = JUST_LEFT; }
	| CENTER { $$ = JUST_CENTER; }
	;

inoutchoice: IN { $$ = TICKS_IN; }
	| OUT { $$ = TICKS_OUT; }
	| BOTH { $$ = TICKS_BOTH; }
	;

formatchoice: DECIMAL { $$ = FORMAT_DECIMAL; }
	| EXPONENTIAL { $$ = FORMAT_EXPONENTIAL; }
	| GENERAL { $$ = FORMAT_GENERAL; }
	| SCIENTIFIC { $$ = FORMAT_SCIENTIFIC; }
	| ENGINEERING { $$ = FORMAT_ENGINEERING; }
	| POWER { $$ = FORMAT_POWER; }
	| DDMMYY { $$ = FORMAT_DDMMYY; }
	| MMDDYY { $$ = FORMAT_MMDDYY; }
	| YYMMDD { $$ = FORMAT_YYMMDD; }
	| MMYY { $$ = FORMAT_MMYY; }
	| MMDD { $$ = FORMAT_MMDD; }
	| MONTHDAY { $$ = FORMAT_MONTHDAY; }
	| DAYMONTH { $$ = FORMAT_DAYMONTH; }
	| MONTHS { $$ = FORMAT_MONTHS; }
	| MONTHSY { $$ = FORMAT_MONTHSY; }
	| MONTHL { $$ = FORMAT_MONTHL; }
	| DAYOFWEEKS { $$ = FORMAT_DAYOFWEEKS; }
	| DAYOFWEEKL { $$ = FORMAT_DAYOFWEEKL; }
	| DAYOFYEAR { $$ = FORMAT_DAYOFYEAR; }
	| HMS { $$ = FORMAT_HMS; }
	| MMDDHMS { $$ = FORMAT_MMDDHMS; }
	| MMDDYYHMS { $$ = FORMAT_MMDDYYHMS; }
	| YYMMDDHMS { $$ = FORMAT_YYMMDDHMS; }
	| DEGREESLON { $$ = FORMAT_DEGREESLON; }
	| DEGREESMMLON { $$ = FORMAT_DEGREESMMLON; }
	| DEGREESMMSSLON { $$ = FORMAT_DEGREESMMSSLON; }
	| MMSSLON { $$ = FORMAT_MMSSLON; }
	| DEGREESLAT { $$ = FORMAT_DEGREESLAT; }
	| DEGREESMMLAT { $$ = FORMAT_DEGREESMMLAT; }
	| DEGREESMMSSLAT { $$ = FORMAT_DEGREESMMSSLAT; }
	| MMSSLAT { $$ = FORMAT_MMSSLAT; }
	;

signchoice: NORMAL { $$ = SIGN_NORMAL; }
	| ABSOLUTE { $$ = SIGN_ABSOLUTE; }
	| NEGATE { $$ = SIGN_NEGATE; }
	;

direction: UP { $$ = UP; }
	| DOWN { $$ = DOWN; }
	| RIGHT { $$ = RIGHT; }
	| LEFT { $$ = LEFT; }
	| IN { $$ = IN; }
	| OUT { $$ = OUT; }
	;

worldview: WORLD { $$ = COORD_WORLD; }
	| VIEW { $$ = COORD_VIEW; }
	;

datacolumn: X_TOK { $$ = DATA_X; }
	| Y_TOK { $$ = DATA_Y; }
	| X0 { $$ = DATA_X; }
	| Y0 { $$ = DATA_Y; }
	| Y1 { $$ = DATA_Y1; }
	| Y2 { $$ = DATA_Y2; }
	| Y3 { $$ = DATA_Y3; }
	| Y4 { $$ = DATA_Y4; }
	;

sortdir: ASCENDING { $$ = ASCENDING; }
	| DESCENDING { $$ = DESCENDING; }
	;

sorton: X_TOK { $$ = DATA_X; }
	| Y_TOK { $$ = DATA_Y; }
	;

ffttype: DFT { $$ = FFT_DFT; }
	| FFT { $$ = FFT_FFT; }
	| INVDFT { $$ = FFT_INVDFT; }
	| INVFFT { $$ = FFT_INVFFT; }
	;

fourierdata:
	REAL {$$=0;}
	| COMPLEX {$$=1;}
	;

fourierloadx:
	INDEX {$$=0;}
	| FREQUENCY {$$=1;}
	| PERIOD {$$=2;}
	;

fourierloady:
	MAGNITUDE {$$=0;}
	| PHASE {$$=1;}
	| COEFFICIENTS {$$=2;}
	;

windowtype:
	NONE {$$=0;}
	| TRIANGULAR {$$=1;}
	| HANNING {$$=2;}
	| WELCH {$$=3;}
	| HAMMING {$$=4;}
	| BLACKMAN {$$=5;}
	| PARZEN {$$=6;}
	;
	
extremetype: MINP { $$ = MINP; }
	| MAXP { $$ = MAXP; }
        | AVG { $$ = AVG; }
	| SD { $$ = SD; }
	;

font_select:
        FONTP NUMBER
        {
            $$ = get_mapped_font((int) $2);
        }
        | FONTP CHRSTR
        {
            $$ = get_font_by_name($2);
            free($2);
        }
        ;

lines_select:
        LINESTYLE NUMBER
        {
	    int lines = (int) $2;
            if (lines >= 0 && lines < number_of_linestyles()) {
	        $$ = lines;
	    } else {
	        errmsg("invalid linestyle");
	        $$ = 1;
	    }
        }
        ;

pattern_select:
        PATTERN NUMBER
        {
	    int patno = (int) $2;
            if (patno >= 0 && patno < number_of_patterns()) {
	        $$ = patno;
	    } else {
	        errmsg("invalid pattern number");
	        $$ = 1;
	    }
        }
        ;

color_select:
        COLOR NUMBER
        {
            int c = (int) $2;
            if (c >= 0 && c < number_of_colors()) {
                $$ = c;
            } else {
                errmsg("Invalid color ID");
                $$ = 1;
            }
        }
        | COLOR CHRSTR
        {
            int c = get_color_by_name($2);
            if (c == BAD_COLOR) {
                errmsg("Invalid color name");
                c = 1;
            }
            free($2);
            $$ = c;
        }
        | COLOR '(' NUMBER ',' NUMBER ',' NUMBER ')'
        {
            int c;
            CMap_entry cmap;
            cmap.rgb.red = (int) $3;
            cmap.rgb.green = (int) $5;
            cmap.rgb.blue = (int) $7;
            cmap.ctype = COLOR_MAIN;
            cmap.cname = NULL;
            c = add_color(cmap);
            if (c == BAD_COLOR) {
                errmsg("Can't allocate requested color");
                c = 1;
            }
            $$ = c;
        }
        ;

linew_select:
        LINEWIDTH NUMBER
        {
            double linew;
            linew = $2;
            if (linew < 0.0) {
                yyerror("Negative linewidth");
                linew = 0.0;
            } else if (linew > MAX_LINEWIDTH) {
                yyerror("Linewidth too large");
                linew = MAX_LINEWIDTH;
            }
            $$ = linew;
        }
        ;

opchoice_sel: PLACE opchoice
        {
            $$ = $2;
        }
        ;

opchoice: NORMAL { $$ = PLACEMENT_NORMAL; }
	| OPPOSITE { $$ = PLACEMENT_OPPOSITE; }
	| BOTH { $$ = PLACEMENT_BOTH; }
	;


parmset_obs:
        PAGE LAYOUT pageorient
        {
            Page_geometry pg;
            if ($3 == PAGE_ORIENT_LANDSCAPE) {
                pg.width =  792;
                pg.height = 612;
            } else {
                pg.width =  612;
                pg.height = 792;
            }
            pg.dpi_x = 72.0;
            pg.dpi_y = 72.0;
            set_page_geometry(pg);
        }
	| PAGE NUMBER {
	    scroll_proc((int) $2);
	}
	| PAGE INOUT NUMBER {
	    scrollinout_proc((int) $3);
	}

	| DEFAULT FONTP SOURCE NUMBER {
	}

	| STACK WORLD expr ',' expr ',' expr ',' expr TICKP expr ',' expr ',' expr ',' expr
	{
	    add_world(whichgraph, $3, $5, $7, $9);
	}

	| BOX FILL colpat_obs {filltype_obs = $3;}

	| ELLIPSE FILL colpat_obs {filltype_obs = $3;}

	| STRING linew_select { }

	| TIMESTAMP linew_select { }

	| TITLE linew_select { }
	| SUBTITLE linew_select { }

	| LEGEND BOX onoff {
	    if ($3 == FALSE && get_project_version() <= 40102) {
                g[whichgraph].l.boxpen.pattern = 0;
            }
	}
	| LEGEND BOX FILL onoff { }
	| LEGEND BOX FILL WITH colpat_obs {filltype_obs = $5;}
	| LEGEND lines_select { }
	| LEGEND linew_select { }

	| GRAPHNO LABEL onoff { }

	| GRAPHNO TYPE LOGX { 
	    g[$1].type = GRAPH_XY;
	    g[$1].xscale = SCALE_LOG;
	}
	| GRAPHNO TYPE LOGY { 
	    g[$1].type = GRAPH_XY;
	    g[$1].yscale = SCALE_LOG;
	}
	| GRAPHNO TYPE LOGXY
	{ 
	    g[$1].type = GRAPH_XY;
	    g[$1].xscale = SCALE_LOG;
	    g[$1].yscale = SCALE_LOG;
	}
	| GRAPHNO TYPE BAR
	{ 
	    g[$1].type = GRAPH_CHART;
	    g[$1].xyflip = FALSE;
	    g[$1].stacked = FALSE;
	}
	| GRAPHNO TYPE HBAR
	{ 
	    g[$1].type = GRAPH_CHART;
	    g[$1].xyflip = TRUE;
	}
	| GRAPHNO TYPE STACKEDBAR
	{ 
	    g[$1].type = GRAPH_CHART;
	    g[$1].stacked = TRUE;
	}
	| GRAPHNO TYPE STACKEDHBAR
	{ 
	    g[$1].type = GRAPH_CHART;
	    g[$1].stacked = TRUE;
	    g[$1].xyflip = TRUE;
	}

	| LEGEND LAYOUT NUMBER {
	}

	| FRAMEP FILL onoff { 
            g[whichgraph].f.fillpen.pattern = $3;
        }

	| GRAPHNO AUTOSCALE TYPE AUTO {
        }
	| GRAPHNO AUTOSCALE TYPE SPEC {
        }

	| LINE ARROW SIZE NUMBER {
	    line_asize = 2.0*$4;
	}

        | HARDCOPY DEVICE NUMBER { }
        | PS LINEWIDTH BEGIN NUMBER { }
        | PS LINEWIDTH INCREMENT NUMBER { }
        | PS linew_select { }
        ;


axislabeldesc_obs:
	linew_select { }
	| opchoice_sel_obs {
	    g[whichgraph].t[naxis].label_op = $1;
	}
        ;

setprop_obs:
	selectset SYMBOL FILL NUMBER {
	    switch ((int) $4){
	    case 0:
	        g[$1->gno].p[$1->setno].symfillpen.pattern = 0;
	        break;
	    case 1:
	        g[$1->gno].p[$1->setno].symfillpen.pattern = 1;
	        break;
	    case 2:
	        g[$1->gno].p[$1->setno].symfillpen.pattern = 1;
	        g[$1->gno].p[$1->setno].symfillpen.color = getbgcolor();
	        break;
	    }
	}
	| selectset SKIP NUMBER
        {
	    g[$1->gno].p[$1->setno].symskip = (int) $3;
	}
	| selectset FILL NUMBER
        {
	    switch ((int) $3) {
            case 0:
                g[$1->gno].p[$1->setno].filltype = SETFILL_NONE;
                break;
            case 1:
                g[$1->gno].p[$1->setno].filltype = SETFILL_POLYGON;
                break;
            case 2:
                g[$1->gno].p[$1->setno].filltype = SETFILL_BASELINE;
                g[$1->gno].p[$1->setno].baseline_type = BASELINE_TYPE_0;
                break;
            case 6:
                g[$1->gno].p[$1->setno].filltype = SETFILL_BASELINE;
                g[$1->gno].p[$1->setno].baseline_type = BASELINE_TYPE_GMIN;
                break;
            case 7:
                g[$1->gno].p[$1->setno].filltype = SETFILL_BASELINE;
                g[$1->gno].p[$1->setno].baseline_type = BASELINE_TYPE_GMAX;
                break;
            }
	}
	| selectset ERRORBAR TYPE opchoice_obs {
	    g[$1->gno].p[$1->setno].errbar.ptype = $4;
	}
	| selectset SYMBOL COLOR '-' NUMBER {
	    g[$1->gno].p[$1->setno].sympen.color = -1;
	}
	| selectset SYMBOL CENTER onoff { }
	| selectset lines_select {
	    g[$1->gno].p[$1->setno].lines = $2;
	}
	| selectset linew_select {
	    g[$1->gno].p[$1->setno].linew = $2;
	}
	| selectset color_select {
	    g[$1->gno].p[$1->setno].linepen.color = $2;
	}
	| selectset FILL WITH colpat_obs {filltype_obs = $4;}
	| selectset XYZ expr ',' expr { }
	| selectset ERRORBAR LENGTH NUMBER {
            g[$1->gno].p[$1->setno].errbar.barsize = $4;
	}
	| selectset ERRORBAR RISER onoff { }
        ;
        

tickattr_obs:
	MAJOR onoff {
	    /* <= xmgr-4.1 */
	    g[whichgraph].t[naxis].active = $2;
	}
	| MINOR onoff { }
	| ALT onoff   { }
	| MINP expr   { }
	| MAXP expr   { }
	| LOG onoff   { }
	| MINOR expr {
	    if ($2 != 0.0) {
                g[whichgraph].t[naxis].nminor = 
                            (int) rint(g[whichgraph].t[naxis].tmajor / $2 - 1);
            } else {
                g[whichgraph].t[naxis].nminor = 0;
            }
	}
	| SIZE NUMBER {
	    g[whichgraph].t[naxis].props.size = $2;
	}
	| NUMBER ',' expr {
	    g[whichgraph].t[naxis].tloc[(int) $1].wtpos = $3;
	    g[whichgraph].t[naxis].tloc[(int) $1].type = TICK_TYPE_MAJOR;
	}
	| opchoice_sel_obs {
	    g[whichgraph].t[naxis].t_op = $1;
	}
        ;

ticklabelattr_obs:
	linew_select { }
	| LAYOUT SPEC { }

	| LAYOUT HORIZONTAL {
	    g[whichgraph].t[naxis].tl_angle = 0;
	}
	| LAYOUT VERTICAL {
	    g[whichgraph].t[naxis].tl_angle = 90;
	}
	| PLACE ON TICKSP { }
	| PLACE BETWEEN TICKSP { }
	| opchoice_sel_obs {
	    g[whichgraph].t[naxis].tl_op = $1;
	}
        ;

colpat_obs: NONE
	| COLOR
	| PATTERN
	;

opchoice_sel_obs: OP opchoice_obs
        {
            $$ = $2;
        }
        ;

opchoice_obs: TOP { $$ = PLACEMENT_OPPOSITE; }
	| BOTTOM { $$ = PLACEMENT_NORMAL; }
	| LEFT { $$ = PLACEMENT_NORMAL; }
	| RIGHT { $$ = PLACEMENT_OPPOSITE; }
	| BOTH { $$ = PLACEMENT_BOTH; }
	;

%%

/* list of intrinsic functions and keywords */
symtab_entry ikey[] = {
	{"A", SCRARRAY, NULL},
	{"A0", FITPARM, NULL},
	{"A0MAX", FITPMAX, NULL},
	{"A0MIN", FITPMIN, NULL},
	{"A1", FITPARM, NULL},
	{"A1MAX", FITPMAX, NULL},
	{"A1MIN", FITPMIN, NULL},
	{"A2", FITPARM, NULL},
	{"A2MAX", FITPMAX, NULL},
	{"A2MIN", FITPMIN, NULL},
	{"A3", FITPARM, NULL},
	{"A3MAX", FITPMAX, NULL},
	{"A3MIN", FITPMIN, NULL},
	{"A4", FITPARM, NULL},
	{"A4MAX", FITPMAX, NULL},
	{"A4MIN", FITPMIN, NULL},
	{"A5", FITPARM, NULL},
	{"A5MAX", FITPMAX, NULL},
	{"A5MIN", FITPMIN, NULL},
	{"A6", FITPARM, NULL},
	{"A6MAX", FITPMAX, NULL},
	{"A6MIN", FITPMIN, NULL},
	{"A7", FITPARM, NULL},
	{"A7MAX", FITPMAX, NULL},
	{"A7MIN", FITPMIN, NULL},
	{"A8", FITPARM, NULL},
	{"A8MAX", FITPMAX, NULL},
	{"A8MIN", FITPMIN, NULL},
	{"A9", FITPARM, NULL},
	{"A9MAX", FITPMAX, NULL},
	{"A9MIN", FITPMIN, NULL},
	{"ABOVE", ABOVE, NULL},
	{"ABS", FUNC_D, fabs},
	{"ABSOLUTE", ABSOLUTE, NULL},
	{"ACOS", FUNC_D, acos},
	{"ACOSH", FUNC_D, acosh},
	{"AI", FUNC_D, ai_wrap},
	{"ALIAS", ALIAS, NULL},
	{"ALT", ALT, NULL},
	{"ALTXAXIS", ALTXAXIS, NULL},
	{"ALTYAXIS", ALTYAXIS, NULL},
	{"AND", AND, NULL},
	{"ANGLE", ANGLE, NULL},
	{"ANTIALIASING", ANTIALIASING, NULL},
	{"APPEND", APPEND, NULL},
	{"ARRANGE", ARRANGE, NULL},
	{"ARROW", ARROW, NULL},
	{"ASCENDING", ASCENDING, NULL},
	{"ASIN", FUNC_D, asin},
	{"ASINH", FUNC_D, asinh},
	{"ASPLINE", ASPLINE, NULL},
	{"ATAN", FUNC_D, atan},
	{"ATAN2", FUNC_DD, atan2},
	{"ATANH", FUNC_D, atanh},
	{"AUTO", AUTO, NULL},
	{"AUTOSCALE", AUTOSCALE, NULL},
	{"AUTOTICKS", AUTOTICKS, NULL},
	{"AVALUE", AVALUE, NULL},
	{"AVG", AVG, NULL},
	{"AXES", AXES, NULL},
	{"B", SCRARRAY, NULL},
	{"BACKGROUND", BACKGROUND, NULL},
	{"BAR", BAR, NULL},
	{"BARDY", BARDY, NULL},
	{"BARDYDY", BARDYDY, NULL},
	{"BASELINE", BASELINE, NULL},
	{"BATCH", BATCH, NULL},
        {"BEGIN", BEGIN, NULL},
	{"BELOW", BELOW, NULL},
	{"BETA", FUNC_DD, beta},
	{"BETWEEN", BETWEEN, NULL},
	{"BI", FUNC_D, bi_wrap},
	{"BLACKMAN", BLACKMAN, NULL},
	{"BLOCK", BLOCK, NULL},
	{"BOTH", BOTH, NULL},
	{"BOTTOM", BOTTOM, NULL},
	{"BOX", BOX, NULL},
	{"C", SCRARRAY, NULL},
	{"CD", CD, NULL},
	{"CEIL", FUNC_D, ceil},
	{"CENTER", CENTER, NULL},
	{"CHAR", CHAR, NULL},
	{"CHART", CHART, NULL},
	{"CHDTR", FUNC_DD, chdtr},
	{"CHDTRC", FUNC_DD, chdtrc},
	{"CHDTRI", FUNC_DD, chdtri},
	{"CHI", FUNC_D, chi_wrap},
	{"CHRSTR", CHRSTR, NULL},
	{"CI", FUNC_D, ci_wrap},
	{"CLEAR", CLEAR, NULL},
	{"CLICK", CLICK, NULL},
	{"CLIP", CLIP, NULL},
	{"CLOSE", CLOSE, NULL},
	{"COEFFICIENTS", COEFFICIENTS, NULL},
	{"COLOR", COLOR, NULL},
	{"COMMENT", COMMENT, NULL},
	{"COMPLEX", COMPLEX, NULL},
	{"CONST", PROC_CONST, NULL},
	{"CONSTRAINTS", CONSTRAINTS, NULL},
	{"COPY", COPY, NULL},
	{"COS", FUNC_D, cos},
	{"COSH", FUNC_D, cosh},
	{"CYCLE", CYCLE, NULL},
	{"D", SCRARRAY, NULL},
	{"DAWSN", FUNC_D, dawsn},
	{"DAYMONTH", DAYMONTH, NULL},
	{"DAYOFWEEKL", DAYOFWEEKL, NULL},
	{"DAYOFWEEKS", DAYOFWEEKS, NULL},
	{"DAYOFYEAR", DAYOFYEAR, NULL},
	{"DDMMYY", DDMMYY, NULL},
	{"DECIMAL", DECIMAL, NULL},
	{"DEF", DEF, NULL},
	{"DEFAULT", DEFAULT, NULL},
	{"DEFINE", DEFINE, NULL},
	{"DEG", UCONSTANT, deg_uconst},
	{"DEGREESLAT", DEGREESLAT, NULL},
	{"DEGREESLON", DEGREESLON, NULL},
	{"DEGREESMMLAT", DEGREESMMLAT, NULL},
	{"DEGREESMMLON", DEGREESMMLON, NULL},
	{"DEGREESMMSSLAT", DEGREESMMSSLAT, NULL},
	{"DEGREESMMSSLON", DEGREESMMSSLON, NULL},
	{"DESCENDING", DESCENDING, NULL},
	{"DESCRIPTION", DESCRIPTION, NULL},
	{"DEVICE", DEVICE, NULL},
	{"DFT", DFT, NULL},
	{"DIFF", DIFFERENCE, NULL},
	{"DIFFERENCE", DIFFERENCE, NULL},
	{"DISK", DISK, NULL},
	{"DOWN", DOWN, NULL},
	{"DPI", DPI, NULL},
	{"DROP", DROP, NULL},
	{"DROPLINE", DROPLINE, NULL},
	{"ECHO", ECHO, NULL},
	{"ELLIE", FUNC_DD, ellie},
	{"ELLIK", FUNC_DD, ellik},
	{"ELLIPSE", ELLIPSE, NULL},
	{"ELLPE", FUNC_D, ellpe},
	{"ELLPK", FUNC_D, ellpk},
	{"ENGINEERING", ENGINEERING, NULL},
	{"EQ", EQ, NULL},
	{"ER", ERRORBAR, NULL},
	{"ERF", FUNC_D, erf},
	{"ERFC", FUNC_D, erfc},
	{"ERRORBAR", ERRORBAR, NULL},
	{"EXIT", EXIT, NULL},
	{"EXP", FUNC_D, exp},
	{"EXPN", FUNC_ND, expn},
	{"EXPONENTIAL", EXPONENTIAL, NULL},
	{"FAC", FUNC_I, fac},
	{"FALSE", OFF, NULL},
	{"FDTR", FUNC_NND, fdtr},
	{"FDTRC", FUNC_NND, fdtrc},
	{"FDTRI", FUNC_NND, fdtri},
	{"FFT", FFT, NULL},
	{"FILE", FILEP, NULL},
	{"FILL", FILL, NULL},
	{"FIT", FIT, NULL},
	{"FIXED", FIXED, NULL},
	{"FIXEDPOINT", FIXEDPOINT, NULL},
	{"FLOOR", FUNC_D, floor},
	{"FLUSH", FLUSH, NULL},
	{"FOCUS", FOCUS, NULL},
	{"FOLLOWS", FOLLOWS, NULL},
	{"FONT", FONTP, NULL},
	{"FORCE", FORCE, NULL},
	{"FORMAT", FORMAT, NULL},
	{"FORMULA", FORMULA, NULL},
	{"FRAME", FRAMEP, NULL},
	{"FREE", FREE, NULL},
	{"FREQUENCY", FREQUENCY, NULL},
	{"FRESNLC", FUNC_D, fresnlc_wrap},
	{"FRESNLS", FUNC_D, fresnls_wrap},
	{"FROM", FROM, NULL},
	{"F_OF_D", PROC_FUNC_D, NULL},
	{"F_OF_DD", PROC_FUNC_DD, NULL},
        {"F_OF_I", PROC_FUNC_I, NULL},
	{"F_OF_ND", PROC_FUNC_ND, NULL},
	{"F_OF_NN", PROC_FUNC_NN, NULL},
	{"F_OF_NND", PROC_FUNC_NND, NULL},
	{"F_OF_PPD", PROC_FUNC_PPD, NULL},
	{"F_OF_PPPD", PROC_FUNC_PPPD, NULL},
	{"GAMMA", FUNC_D, true_gamma},
	{"GDTR", FUNC_PPD, gdtr},
	{"GDTRC", FUNC_PPD, gdtrc},
	{"GE", GE, NULL},
	{"GENERAL", GENERAL, NULL},
	{"GETP", GETP, NULL},
	{"GRID", GRID, NULL},
	{"GT", GT, NULL},
	{"HAMMING", HAMMING, NULL},
	{"HANNING", HANNING, NULL},
	{"HARDCOPY", HARDCOPY, NULL},
	{"HBAR", HBAR, NULL},
	{"HGAP", HGAP, NULL},
	{"HIDDEN", HIDDEN, NULL},
	{"HISTO", HISTO, NULL},
	{"HMS", HMS, NULL},
	{"HORIZI", HORIZI, NULL},
	{"HORIZO", HORIZO, NULL},
	{"HORIZONTAL", HORIZONTAL, NULL},
	{"HYP2F1", FUNC_PPPD, hyp2f1},
	{"HYPERG", FUNC_PPD, hyperg},
	{"HYPOT", FUNC_DD, hypot},
	{"I0E", FUNC_D, i0e},
	{"I1E", FUNC_D, i1e},
	{"ID", ID, NULL},
	{"IFILTER", IFILTER, NULL},
	{"IGAM", FUNC_DD, igam},
	{"IGAMC", FUNC_DD, igamc},
	{"IGAMI", FUNC_DD, igami},
	{"IN", IN, NULL},
	{"INCBET", FUNC_PPD, incbet},
	{"INCBI", FUNC_PPD, incbi},
	{"INCREMENT", INCREMENT, NULL},
	{"INDEX", INDEX, NULL},
	{"INOUT", INOUT, NULL},
	{"INTEGRATE", INTEGRATE, NULL},
	{"INTERP", INTERP, NULL},
	{"INVDFT", INVDFT, NULL},
	{"INVERT", INVERT, NULL},
	{"INVFFT", INVFFT, NULL},
	{"IRAND", FUNC_I, irand_wrap},
	{"IV", FUNC_DD, iv_wrap},
	{"JDAY", JDAY, NULL},
	{"JDAY0", JDAY0, NULL},
	{"JOIN", JOIN, NULL},
	{"JUST", JUST, NULL},
	{"JV", FUNC_DD, jv_wrap},
	{"K0E", FUNC_D, k0e},
	{"K1E", FUNC_D, k1e},
	{"KILL", KILL, NULL},
	{"KN", FUNC_ND, kn_wrap},
	{"LABEL", LABEL, NULL},
	{"LANDSCAPE", LANDSCAPE, NULL},
	{"LAYOUT", LAYOUT, NULL},
	{"LBETA", FUNC_DD, lbeta},
	{"LE", LE, NULL},
	{"LEFT", LEFT, NULL},
	{"LEGEND", LEGEND, NULL},
	{"LENGTH", LENGTH, NULL},
	{"LGAMMA", FUNC_D, lgamma},
	{"LINE", LINE, NULL},
	{"LINESTYLE", LINESTYLE, NULL},
	{"LINEWIDTH", LINEWIDTH, NULL},
	{"LINK", LINK, NULL},
	{"LN", FUNC_D, log},
	{"LOAD", LOAD, NULL},
	{"LOCTYPE", LOCTYPE, NULL},
	{"LOG", LOG, NULL},
	{"LOG10", FUNC_D, log10},
	{"LOG2", FUNC_D, log2},
	{"LOGARITHMIC", LOGARITHMIC, NULL},
	{"LOGX", LOGX, NULL},
	{"LOGXY", LOGXY, NULL},
	{"LOGY", LOGY, NULL},
	{"LT", LT, NULL},
	{"MAGIC", MAGIC, NULL},
	{"MAGNITUDE", MAGNITUDE, NULL},
	{"MAJOR", MAJOR, NULL},
	{"MAP", MAP, NULL},
	{"MAX", MAXP, NULL},
	{"MAXOF", FUNC_DD, max_wrap},
	{"MIN", MINP, NULL},
	{"MINOF", FUNC_DD, min_wrap},
	{"MINOR", MINOR, NULL},
	{"MMDD", MMDD, NULL},
	{"MMDDHMS", MMDDHMS, NULL},
	{"MMDDYY", MMDDYY, NULL},
	{"MMDDYYHMS", MMDDYYHMS, NULL},
	{"MMSSLAT", MMSSLAT, NULL},
	{"MMSSLON", MMSSLON, NULL},
	{"MMYY", MMYY, NULL},
	{"MOD", FUNC_DD, fmod},
	{"MONTHDAY", MONTHDAY, NULL},
	{"MONTHL", MONTHL, NULL},
	{"MONTHS", MONTHS, NULL},
	{"MONTHSY", MONTHSY, NULL},
	{"MOVE", MOVE, NULL},
	{"NDTR", FUNC_D, ndtr},
	{"NDTRI", FUNC_D, ndtri},
	{"NE", NE, NULL},
	{"NEGATE", NEGATE, NULL},
	{"NEW", NEW, NULL},
	{"NONE", NONE, NULL},
	{"NONLFIT", NONLFIT, NULL},
	{"NORM", FUNC_D, fx},
	{"NORMAL", NORMAL, NULL},
	{"NOT", NOT, NULL},
	{"NUMBER", NUMBER, NULL},
	{"NXY", NXY, NULL},
	{"OFF", OFF, NULL},
	{"OFFSET", OFFSET, NULL},
	{"OFFSETX", OFFSETX, NULL},
	{"OFFSETY", OFFSETY, NULL},
	{"OFILTER", OFILTER, NULL},
	{"ON", ON, NULL},
	{"OP", OP, NULL},
	{"OPPOSITE", OPPOSITE, NULL},
	{"OR", OR, NULL},
	{"OUT", OUT, NULL},
	{"PAGE", PAGE, NULL},
	{"PARA", PARA, NULL},
	{"PARAMETERS", PARAMETERS, NULL},
	{"PARZEN", PARZEN, NULL},
	{"PATTERN", PATTERN, NULL},
	{"PDTR", FUNC_ND, pdtr},
	{"PDTRC", FUNC_ND, pdtrc},
	{"PDTRI", FUNC_ND, pdtri},
	{"PERIOD", PERIOD, NULL},
	{"PERP", PERP, NULL},
	{"PHASE", PHASE, NULL},
	{"PI", CONSTANT, pi_const},
	{"PIPE", PIPE, NULL},
	{"PLACE", PLACE, NULL},
	{"POINT", POINT, NULL},
	{"POLAR", POLAR, NULL},
	{"POLYI", POLYI, NULL},
	{"POLYO", POLYO, NULL},
	{"POP", POP, NULL},
	{"PORTRAIT", PORTRAIT, NULL},
	{"POWER", POWER, NULL},
	{"PREC", PREC, NULL},
	{"PREPEND", PREPEND, NULL},
	{"PRINT", PRINT, NULL},
	{"PS", PS, NULL},
	{"PSI", FUNC_D, psi},
	{"PUSH", PUSH, NULL},
	{"PUTP", PUTP, NULL},
	{"RAD", UCONSTANT, rad_uconst},
	{"RAND", CONSTANT, drand48},
	{"READ", READ, NULL},
	{"REAL", REAL, NULL},
	{"RECIPROCAL", RECIPROCAL, NULL},
	{"REDRAW", REDRAW, NULL},
	{"REGRESS", REGRESS, NULL},
	{"REVERSE", REVERSE, NULL},
	{"RGAMMA", FUNC_D, rgamma},
	{"RIGHT", RIGHT, NULL},
	{"RINT", FUNC_D, rint},
	{"RISER", RISER, NULL},
	{"RNORM", FUNC_DD, rnorm},
	{"ROT", ROT, NULL},
	{"ROUNDED", ROUNDED, NULL},
	{"RULE", RULE, NULL},
	{"RUNAVG", RUNAVG, NULL},
	{"RUNMAX", RUNMAX, NULL},
	{"RUNMED", RUNMED, NULL},
	{"RUNMIN", RUNMIN, NULL},
	{"RUNSTD", RUNSTD, NULL},
	{"SAVEALL", SAVEALL, NULL},
	{"SCALE", SCALE, NULL},
	{"SCIENTIFIC", SCIENTIFIC, NULL},
	{"SCROLL", SCROLL, NULL},
	{"SD", SD, NULL},
	{"SET", SET, NULL},
	{"SFORMAT", SFORMAT, NULL},
	{"SHI", FUNC_D, shi_wrap},
	{"SI", FUNC_D, si_wrap},
	{"SIGN", SIGN, NULL},
	{"SIN", FUNC_D, sin},
	{"SINH", FUNC_D, sinh},
	{"SIZE", SIZE, NULL},
	{"SKIP", SKIP, NULL},
	{"SLEEP", SLEEP, NULL},
	{"SMITH", SMITH, NULL},
	{"SORT", SORT, NULL},
	{"SOURCE", SOURCE, NULL},
	{"SPEC", SPEC, NULL},
	{"SPENCE", FUNC_D, spence},
	{"SPLINE", SPLINE, NULL},
	{"SPLIT", SPLIT, NULL},
	{"SQR", FUNC_D, sqr_wrap},
	{"SQRT", FUNC_D, sqrt},
	{"STACK", STACK, NULL},
	{"STACKED", STACKED, NULL},
	{"STACKEDBAR", STACKEDBAR, NULL},
	{"STACKEDHBAR", STACKEDHBAR, NULL},
	{"STAGGER", STAGGER, NULL},
	{"START", START, NULL},
	{"STDTR", FUNC_ND, stdtr},
	{"STDTRI", FUNC_ND, stdtri},
	{"STOP", STOP, NULL},
	{"STRING", STRING, NULL},
	{"STRUVE", FUNC_DD, struve},
	{"SUBTITLE", SUBTITLE, NULL},
	{"SYMBOL", SYMBOL, NULL},
	{"TAN", FUNC_D, tan},
	{"TANH", FUNC_D, tanh},
	{"TARGET", TARGET, NULL},
	{"TICK", TICKP, NULL},
	{"TICKLABEL", TICKLABEL, NULL},
	{"TICKS", TICKSP, NULL},
	{"TIMER", TIMER, NULL},
	{"TIMESTAMP", TIMESTAMP, NULL},
	{"TITLE", TITLE, NULL},
	{"TO", TO, NULL},
	{"TOP", TOP, NULL},
	{"TRIANGULAR", TRIANGULAR, NULL},
	{"TRUE", ON, NULL},
	{"TYPE", TYPE, NULL},
	{"UNLINK", UNLINK, NULL},
	{"UNIT", PROC_UNIT, NULL},
	{"UP", UP, NULL},
	{"USE", USE, NULL},
	{"VERSION", VERSION, NULL},
	{"VERTI", VERTI, NULL},
	{"VERTICAL", VERTICAL, NULL},
	{"VERTO", VERTO, NULL},
	{"VGAP", VGAP, NULL},
	{"VIEW", VIEW, NULL},
	{"VX1", VX1, NULL},
	{"VX2", VX2, NULL},
	{"VXMAX", VXMAX, NULL},
	{"VY1", VY1, NULL},
	{"VY2", VY2, NULL},
	{"VYMAX", VYMAX, NULL},
	{"WELCH", WELCH, NULL},
	{"WITH", WITH, NULL},
	{"WORLD", WORLD, NULL},
	{"WRITE", WRITE, NULL},
	{"WX1", WX1, NULL},
	{"WX2", WX2, NULL},
	{"WY1", WY1, NULL},
	{"WY2", WY2, NULL},
	{"X", X_TOK, NULL},
	{"X0", X0, NULL},
	{"X1", X1, NULL},
	{"XAXES", XAXES, NULL},
	{"XAXIS", XAXIS, NULL},
	{"XCOR", XCOR, NULL},
	{"XMAX", XMAX, NULL},
	{"XMIN", XMIN, NULL},
	{"XY", XY, NULL},
	{"XYDX", XYDX, NULL},
	{"XYDXDX", XYDXDX, NULL},
	{"XYDXDXDYDY", XYDXDXDYDY, NULL},
	{"XYDXDY", XYDXDY, NULL},
	{"XYDY", XYDY, NULL},
	{"XYDYDY", XYDYDY, NULL},
	{"XYHILO", XYHILO, NULL},
	{"XYR", XYR, NULL},
	{"XYSTRING", XYSTRING, NULL},
	{"XYZ", XYZ, NULL},
	{"Y", Y_TOK, NULL},
	{"Y0", Y0, NULL},
	{"Y1", Y1, NULL},
	{"Y2", Y2, NULL},
	{"Y3", Y3, NULL},
	{"Y4", Y4, NULL},
	{"YAXES", YAXES, NULL},
	{"YAXIS", YAXIS, NULL},
	{"YMAX", YMAX, NULL},
	{"YMIN", YMIN, NULL},
	{"YV", FUNC_DD, yv_wrap},
	{"YYMMDD", YYMMDD, NULL},
	{"YYMMDDHMS", YYMMDDHMS, NULL},
	{"ZERO", ZERO, NULL},
	{"ZEROXAXIS", ALTXAXIS, NULL},
	{"ZEROYAXIS", ALTYAXIS, NULL},
	{"ZETA", FUNC_DD, zeta},
	{"ZETAC", FUNC_D, zetac}
};

static int maxfunc = sizeof(ikey) / sizeof(symtab_entry);

int get_parser_gno(void)
{
    return whichgraph;
}

int set_parser_gno(int gno)
{
    if (is_valid_gno(gno) == TRUE) {
        whichgraph = gno;
        return GRACE_EXIT_SUCCESS;
    } else {
        return GRACE_EXIT_FAILURE;
    }
}

int get_parser_setno(void)
{
    return whichset;
}

int set_parser_setno(int gno, int setno)
{
    if (is_valid_setno(gno, setno) == TRUE) {
        whichgraph = gno;
        whichset = setno;
        /* those will usually be overridden except when evaluating
           a _standalone_ vexpr */
        vasgn_gno = gno;
        vasgn_setno = setno;
        return GRACE_EXIT_SUCCESS;
    } else {
        return GRACE_EXIT_FAILURE;
    }
}

int init_array(double **a, int n)
{
    *a = xrealloc(*a, n * SIZEOF_DOUBLE);
    
    return *a == NULL ? 1 : 0;
}

int init_scratch_arrays(int n)
{
    if (!init_array(&ax, n)) {
	if (!init_array(&bx, n)) {
	    if (!init_array(&cx, n)) {
		if (!init_array(&dx, n)) {
		    maxarr = n;
		    return 0;
		}
		free(cx);
	    }
	    free(bx);
	}
	free(ax);
    }
    return 1;
}

double *get_scratch(int ind)
{
    switch (ind) {
    case 0:
        return ax;
        break;
    case 1:
        return bx;
        break;
    case 2:
        return cx;
        break;
    case 3:
        return dx;
        break;
    default:
        return NULL;
        break;
    }
}


#define PARSER_TYPE_VOID    0
#define PARSER_TYPE_EXPR    1
#define PARSER_TYPE_VEXPR   2

static int parser(char *s, int type)
{
    char *seekpos;
    int i;
    
    if (s == NULL || s[0] == '\0') {
        if (type == PARSER_TYPE_VOID) {
            /* don't consider an empty string as error for generic parser */
            return GRACE_EXIT_SUCCESS;
        } else {
            return GRACE_EXIT_FAILURE;
        }
    }
    
    strncpy(f_string, s, MAX_STRING_LENGTH - 2);
    f_string[MAX_STRING_LENGTH - 2] = '\0';
    strcat(f_string, " ");
    
    seekpos = f_string;

    while ((seekpos - f_string < MAX_STRING_LENGTH - 1) && (*seekpos == ' ' || *seekpos == '\t')) {
        seekpos++;
    }
    if (*seekpos == '\n' || *seekpos == '#') {
        if (type == PARSER_TYPE_VOID) {
            /* don't consider an empty string as error for generic parser */
            return GRACE_EXIT_SUCCESS;
        } else {
            return GRACE_EXIT_FAILURE;
        }
    }
    
    log_results(s);

    lowtoupper(f_string);
        
    pos = 0;
    interr = 0;
    expr_parsed  = FALSE;
    vexpr_parsed = FALSE;
    
    yyparse();

    /* free temp. arrays; for a vector expression keep the last one
     * (which is none but v_result), given there have been no errors
     * and it's what we've been asked for
     */
    if (vexpr_parsed && !interr && type == PARSER_TYPE_VEXPR) {
        for (i = 0; i < fcnt - 1; i++) {
            free_tmpvrbl(&(freelist[i]));
        }
    } else {
        for (i = 0; i < fcnt; i++) {
            free_tmpvrbl(&(freelist[i]));
        }
    }
    fcnt = 0;
    
    tgtn = 0;
    
    if ((type == PARSER_TYPE_VEXPR && !vexpr_parsed) ||
        (type == PARSER_TYPE_EXPR  && !expr_parsed)) {
        return GRACE_EXIT_FAILURE;
    } else {
        return (interr ? GRACE_EXIT_FAILURE:GRACE_EXIT_SUCCESS);
    }
}

int s_scanner(char *s, double *res)
{
    int retval = parser(s, PARSER_TYPE_EXPR);
    *res = s_result;
    return retval;
}

int v_scanner(char *s, int *reslen, double **vres)
{
    int retval = parser(s, PARSER_TYPE_VEXPR);
    *reslen = v_result->length;
    *vres = v_result->data;
    v_result->length = 0;
    v_result->data = NULL;
    return retval;
}

int scanner(char *s)
{
    int retval = parser(s, PARSER_TYPE_VOID);
    if (retval != GRACE_EXIT_SUCCESS) {
        return GRACE_EXIT_FAILURE;
    }
    
    if (gotparams) {
	gotparams = FALSE;
        getparms(paramfile);
    }
    
    if (gotread) {
	gotread = FALSE;
        getdata(whichgraph, readfile, cursource, LOAD_SINGLE);
    }
    
    if (gotnlfit) {
	gotnlfit = FALSE;
        do_nonlfit(nlfit_gno, nlfit_setno, nlfit_nsteps);
    }
    return retval;
}

static void free_tmpvrbl(grvar *vrbl)
{
    if (vrbl->type == GRVAR_TMP) {
        vrbl->length = 0;
        cxfree(vrbl->data);
    }
}

static void copy_vrbl(grvar *dest, grvar *src)
{
    dest->type = src->type;
    dest->data = malloc(src->length*SIZEOF_DOUBLE);
    if (dest->data == NULL) {
        errmsg("Malloc failed in copy_vrbl()");
    } else {
        memcpy(dest->data, src->data, src->length*SIZEOF_DOUBLE);
        dest->length = src->length;
    }
}

static int find_set_bydata(double *data, target *tgt)
{
    int gno, setno, ncol;
    
    if (data == NULL) {
        return GRACE_EXIT_FAILURE;
    } else {
        for (gno = 0; gno < number_of_graphs(); gno++) {
            for (setno = 0; setno < number_of_sets(gno); setno++) {
                for (ncol = 0; ncol < MAX_SET_COLS; ncol++) {
                    if (getcol(gno, setno, ncol) == data) {
                        tgt->gno   = gno;
                        tgt->setno = setno;
                        return GRACE_EXIT_SUCCESS;
                    }
                }
            }
        }
    }
    return GRACE_EXIT_FAILURE;
}

int findf(symtab_entry *keytable, char *s)
{

    int low, high, mid;

    low = 0;
    high = maxfunc - 1;
    while (low <= high) {
	mid = (low + high) / 2;
	if (strcmp(s, keytable[mid].s) < 0) {
	    high = mid - 1;
	} else {
	    if (strcmp(s, keytable[mid].s) > 0) {
		low = mid + 1;
	    } else {
		return (mid);
	    }
	}
    }
    return (-1);
}

int compare_keys (const void *a, const void *b)
{
  return (int) strcmp (((const symtab_entry*)a)->s, ((const symtab_entry*)b)->s);
}

/* add new entry to the symbol table */
int addto_symtab(symtab_entry newkey)
{
    int position;
    if ((position = findf(key, newkey.s)) < 0) {
        if ((key = (symtab_entry *) realloc(key, (maxfunc + 1)*sizeof(symtab_entry))) != NULL) {
	    key[maxfunc].type = newkey.type;
	    key[maxfunc].fnc = newkey.fnc;
	    key[maxfunc].s = malloc(strlen(newkey.s) + 1);
	    strcpy(key[maxfunc].s, newkey.s);
	    maxfunc++;
	    qsort(key, maxfunc, sizeof(symtab_entry), compare_keys);
	    return 0;
	} else {
	    errmsg ("Memory allocation failed in addto_symtab()!");
	    return -2;
	}
    } else if (alias_force == TRUE) { /* already exists but alias_force enabled */
        key[position].type = newkey.type;
	key[position].fnc = newkey.fnc;
	return 0;
    } else {
        return -1;
    }
}

/* initialize symbol table */
void init_symtab(void)
{
    int i;
    
    if ((key = (symtab_entry *) malloc(maxfunc*sizeof(symtab_entry))) != NULL) {
    	memcpy (key, ikey, maxfunc*sizeof(symtab_entry));
	for (i = 0; i < maxfunc; i++) {
	    key[i].s = malloc(strlen(ikey[i].s) + 1);
	    strcpy(key[i].s, ikey[i].s);
	}
	qsort(key, maxfunc, sizeof(symtab_entry), compare_keys);
	return;
    } else {
        errmsg ("Memory allocation failed in init_symtab()!");
	key = ikey;
	return;
    }
}

int getcharstr(void)
{
    if (pos >= strlen(f_string))
	 return EOF;
    return (f_string[pos++]);
}

void ungetchstr(void)
{
    if (pos > 0)
	pos--;
}

int yylex(void)
{
    int c, i;
    int found;
    static char s[MAX_STRING_LENGTH];
    char sbuf[MAX_STRING_LENGTH + 40];
    char *str;

    while ((c = getcharstr()) == ' ' || c == '\t');
    if (c == EOF) {
	return (0);
    }
    if (c == '"') {
	i = 0;
	while ((c = getcharstr()) != '"' && c != EOF) {
	    if (c == '\\') {
		int ctmp;
		ctmp = getcharstr();
		if (ctmp != '"') {
		    ungetchstr();
		}
		else {
		    c = ctmp;
		}
	    }
	    s[i] = c;
	    i++;
	}
	if (c == EOF) {
	    yyerror("Nonterminating string");
	    return 0;
	}
	s[i] = '\0';
	str = malloc(strlen(s) + 1);
	strcpy(str, s);
	yylval.sval = str;
	return CHRSTR;
    }
    if (c == '.' || isdigit(c)) {
	char stmp[80];
	double d;
	int i, gotdot = 0;

	i = 0;
	while (c == '.' || isdigit(c)) {
	    if (c == '.') {
		if (gotdot) {
		    yyerror("Reading number, too many dots");
	    	    return 0;
		} else {
		    gotdot = 1;
		}
	    }
	    stmp[i++] = c;
	    c = getcharstr();
	}
	if (c == 'E' || c == 'e') {
	    stmp[i++] = c;
	    c = getcharstr();
	    if (c == '+' || c == '-') {
		stmp[i++] = c;
		c = getcharstr();
	    }
	    while (isdigit(c)) {
		stmp[i++] = c;
		c = getcharstr();
	    }
	}
	if (gotdot && i == 1) {
	    ungetchstr();
	    return '.';
	}
	stmp[i] = '\0';
	ungetchstr();
	sscanf(stmp, "%lf", &d);
	yylval.dval = d;
	return NUMBER;
    }
/* graphs, sets, regions resp. */
    if (c == 'G' || c == 'S' || c == 'R') {
	char stmp[80];
	int i = 0, ctmp = c, gn, sn, rn;
	c = getcharstr();
	while (isdigit(c) || c == '$' || c == '_') {
	    stmp[i++] = c;
	    c = getcharstr();
	}
	if (i == 0) {
	    c = ctmp;
	    ungetchstr();
	} else {
	    ungetchstr();
	    if (ctmp == 'G') {
	        stmp[i] = '\0';
		if (i == 1 && stmp[0] == '_') {
                    gn = get_recent_gno();
                } else if (i == 1 && stmp[0] == '$') {
                    gn = whichgraph;
                } else {
                    gn = atoi(stmp);
                }
		if (set_graph_active(gn, TRUE) == GRACE_EXIT_SUCCESS) {
		    yylval.ival = gn;
		    /* set_parser_gno(gn); */
		    return GRAPHNO;
		}
	    } else if (ctmp == 'S') {
	        stmp[i] = '\0';
		if (i == 1 && stmp[0] == '_') {
                    sn = get_recent_setno();
                } else if (i == 1 && stmp[0] == '$') {
                    sn = whichset;
                } else {
		    sn = atoi(stmp);
                }
		yylval.ival = sn;
		return SETNUM;
	    } else if (ctmp == 'R') {
	        stmp[i] = '\0';
		rn = atoi(stmp);
		if (rn >= 0 && rn < MAXREGION) {
		    yylval.ival = rn;
		    return REGNUM;
		}
	    }
	}
    }
    if (isalpha(c)) {
	char *p = sbuf;

	do {
	    *p++ = c;
	} while ((c = getcharstr()) != EOF && (isalpha(c) || isdigit(c) ||
                  c == '_' || c == '$'));
	ungetchstr();
	*p = '\0';
#ifdef DEBUG
        if (debuglevel == 2) {
	    printf("->%s<-\n", sbuf);
	}
#endif
	found = -1;
	if ((found = findf(key, sbuf)) >= 0) {
	    if (key[found].type == SCRARRAY) {
		switch (sbuf[0]) {
		case 'A':
		    yylval.ival = 0;
		    return SCRARRAY;
		case 'B':
		    yylval.ival = 1;
		    return SCRARRAY;
		case 'C':
		    yylval.ival = 2;
		    return SCRARRAY;
		case 'D':
		    yylval.ival = 3;
		    return SCRARRAY;
		}
	    }
	    else if (key[found].type == FITPARM) {
		int index = sbuf[1] - '0';
		yylval.dval = index;
		return FITPARM;
	    }
	    else if (key[found].type == FITPMAX) {
		int index = sbuf[1] - '0';
		yylval.dval = index;
		return FITPMAX;
	    }
	    else if (key[found].type == FITPMIN) {
		int index = sbuf[1] - '0';
		yylval.dval = index;
		return FITPMIN;
	    }
	    else if (key[found].type == FUNC_I) {
		yylval.ival = found;
		return FUNC_I;
	    }
	    else if (key[found].type == CONSTANT) {
		yylval.ival = found;
		return CONSTANT;
	    }
	    else if (key[found].type == UCONSTANT) {
		yylval.ival = found;
		return UCONSTANT;
	    }
	    else if (key[found].type == FUNC_D) {
		yylval.ival = found;
		return FUNC_D;
	    }
	    else if (key[found].type == FUNC_ND) {
		yylval.ival = found;
		return FUNC_ND;
	    }
	    else if (key[found].type == FUNC_DD) {
		yylval.ival = found;
		return FUNC_DD;
	    }
	    else if (key[found].type == FUNC_NND) {
		yylval.ival = found;
		return FUNC_NND;
	    }
	    else if (key[found].type == FUNC_PPD) {
		yylval.ival = found;
		return FUNC_PPD;
	    }
	    else if (key[found].type == FUNC_PPPD) {
		yylval.ival = found;
		return FUNC_PPPD;
	    }
	    else {
	        yylval.ival = key[found].type;
	        return key[found].type;
	    }
	} else {
	    strcat(sbuf, ": No such function or variable");
	    yyerror(sbuf);
	    return 0;
	}
    }
    switch (c) {
    case '>':
	return follow('=', GE, GT);
    case '<':
	return follow('=', LE, LT);
    case '=':
	return follow('=', EQ, '=');
    case '!':
	return follow('=', NE, NOT);
    case '|':
	return follow('|', OR, '|');
    case '&':
	return follow('&', AND, '&');
    case '\n':
	return '\n';
    default:
	return c;
    }
}

int follow(int expect, int ifyes, int ifno)
{
    int c = getcharstr();

    if (c == expect) {
	return ifyes;
    }
    ungetchstr();
    return ifno;
}

void yyerror(char *s)
{
    int i;
    char buf[2*MAX_STRING_LENGTH + 40];
    sprintf(buf, "%s: %s", s, f_string);
    i = strlen(buf);
    buf[i - 1] = 0;
    errmsg(buf);
    interr = 1;
}


/* Wrappers for some functions*/

static double ai_wrap(double x)
{
    double retval, dummy1, dummy2, dummy3;
    (void) airy(x, &retval, &dummy1, &dummy2, &dummy3);
    return retval;
}

static double bi_wrap(double x)
{
    double retval, dummy1, dummy2, dummy3;
    (void) airy(x, &dummy1, &dummy2, &retval, &dummy3);
    return retval;
}

static double ci_wrap(double x)
{
    double retval, dummy1;
    (void) sici(x, &dummy1, &retval);
    return retval;
}

static double si_wrap(double x)
{
    double retval, dummy1;
    (void) sici(x, &retval, &dummy1);
    return retval;
}

static double chi_wrap(double x)
{
    double retval, dummy1;
    (void) shichi(x, &dummy1, &retval);
    return retval;
}

static double shi_wrap(double x)
{
    double retval, dummy1;
    (void) shichi(x, &retval, &dummy1);
    return retval;
}

static double fresnlc_wrap(double x)
{
    double retval, dummy1;
    (void) fresnl(x, &dummy1, &retval);
    return retval;
}

static double fresnls_wrap(double x)
{
    double retval, dummy1;
    (void) fresnl(x, &retval, &dummy1);
    return retval;
}

static double iv_wrap(double v, double x)
{
    double retval;
    if (v == 0) {
	retval = i0(x);
    } else if (v == 1) {
	retval = i1(x);
    } else {
	retval = iv(v, x);
    }
    return retval;
}

static double jv_wrap(double v, double x)
{
    double retval;
    if (v == rint(v)) {
	retval = jn((int) v, x);
    } else {
	retval = jv(v, x);
    }
    return retval;
}

static double kn_wrap(int n, double x)
{
    double retval;
    if (n == 0) {
	retval = k0(x);
    } else if (n == 1) {
	retval = k1(x);
    } else {
	retval = kn(n, x);
    }
    return retval;
}

static double yv_wrap(double v, double x)
{
    double retval;
    if (v == rint(v)) {
	retval = yn((int) v, x);
    } else {
	retval = yv(v, x);
    }
    return retval;
}

static double sqr_wrap(double x)
{
    return x*x;
}

static double max_wrap(double x, double y)
{
	    return (x >= y ? x : y);
}

static double min_wrap(double x, double y)
{
	    return (x <= y ? x : y);
}

static double irand_wrap(int x)
{
    return (double) (lrand48() % x);
}

static double pi_const(void)
{
    return M_PI;
}

static double deg_uconst(void)
{
    return M_PI / 180.0;
}

static double rad_uconst(void)
{
    return 1.0;
}

#define C1 0.1978977093962766
#define C2 0.1352915131768107

static double rnorm(double mean, double sdev)
{
    double u = drand48();

    return mean + sdev * (pow(u, C2) - pow(1.0 - u, C2)) / C1;
}

static double fx(double x)
{
    return 1.0 / sqrt(2.0 * M_PI) * exp(-x * x * 0.5);
}
