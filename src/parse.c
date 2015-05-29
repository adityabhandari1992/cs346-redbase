/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 1 "parse.y" /* yacc.c:339  */

/*
 * parser.y: yacc specification for RQL
 *
 * Authors: Dallan Quass
 *          Jan Jannink
 *          Jason McHugh
 *
 * originally by: Mark McAuliffe, University of Wisconsin - Madison, 1991
 *
 * 1997: Added "print buffer", "print io", "reset io" and the "*" in
 * SFW Query.
 * 1998: Added "reset buffer", "resize buffer [int]", "queryplans on",
 * and "queryplans off".
 * 2000: Added "const" to yyerror-header
 *
 */

#include <cstdio>
#include <iostream>
#include <sys/types.h>
#include <cstdlib>
#include <unistd.h>
#include "redbase.h"
#include "parser_internal.h"
#include "pf.h"     // for PF_PrintError
#include "rm.h"     // for RM_PrintError
#include "ix.h"     // for IX_PrintError
#include "sm.h"
#include "ql.h"
#include "ex.h"

using namespace std;

  // Added by Wendy Tobagus
#ifndef yyrestart
void yyrestart(FILE*);
#endif

// The PF_STATS indicates that we will be tracking statistics for the PF
// Layer.  The Manager is defined within pf_buffermgr.cc.
// We include it within the parser so that a system command can display
// statistics about the DB.
#ifdef PF_STATS
#include "statistics.h"

// This is defined within the pf_buffermgr.cc
extern StatisticsMgr *pStatisticsMgr;

#endif    // PF_STATS

/*
 * string representation of tokens; provided by scanner
 */
extern char *yytext;

/*
 * points to root of parse tree
 */
static NODE *parse_tree;

int bExit;                 // when to return from RBparse

int bQueryPlans;           // When to print the query plans

PF_Manager *pPfm;          // PF component manager
SM_Manager *pSmm;          // SM component manager
QL_Manager *pQlm;          // QL component manager


#line 137 "y.tab.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_Y_TAB_H_INCLUDED
# define YY_YY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    RW_CREATE = 258,
    RW_DROP = 259,
    RW_TABLE = 260,
    RW_INDEX = 261,
    RW_LOAD = 262,
    RW_SET = 263,
    RW_HELP = 264,
    RW_PRINT = 265,
    RW_EXIT = 266,
    RW_SELECT = 267,
    RW_FROM = 268,
    RW_WHERE = 269,
    RW_INSERT = 270,
    RW_DELETE = 271,
    RW_UPDATE = 272,
    RW_AND = 273,
    RW_INTO = 274,
    RW_VALUES = 275,
    T_EQ = 276,
    T_LT = 277,
    T_LE = 278,
    T_GT = 279,
    T_GE = 280,
    T_NE = 281,
    T_EOF = 282,
    NOTOKEN = 283,
    RW_RESET = 284,
    RW_IO = 285,
    RW_BUFFER = 286,
    RW_RESIZE = 287,
    RW_QUERY_PLAN = 288,
    RW_ON = 289,
    RW_OFF = 290,
    RW_DISTRIBUTED = 291,
    T_INT = 292,
    T_REAL = 293,
    T_STRING = 294,
    T_QSTRING = 295,
    T_SHELL_CMD = 296
  };
#endif
/* Tokens.  */
#define RW_CREATE 258
#define RW_DROP 259
#define RW_TABLE 260
#define RW_INDEX 261
#define RW_LOAD 262
#define RW_SET 263
#define RW_HELP 264
#define RW_PRINT 265
#define RW_EXIT 266
#define RW_SELECT 267
#define RW_FROM 268
#define RW_WHERE 269
#define RW_INSERT 270
#define RW_DELETE 271
#define RW_UPDATE 272
#define RW_AND 273
#define RW_INTO 274
#define RW_VALUES 275
#define T_EQ 276
#define T_LT 277
#define T_LE 278
#define T_GT 279
#define T_GE 280
#define T_NE 281
#define T_EOF 282
#define NOTOKEN 283
#define RW_RESET 284
#define RW_IO 285
#define RW_BUFFER 286
#define RW_RESIZE 287
#define RW_QUERY_PLAN 288
#define RW_ON 289
#define RW_OFF 290
#define RW_DISTRIBUTED 291
#define T_INT 292
#define T_REAL 293
#define T_STRING 294
#define T_QSTRING 295
#define T_SHELL_CMD 296

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 72 "parse.y" /* yacc.c:355  */

    int ival;
    CompOp cval;
    float rval;
    char *sval;
    NODE *n;

#line 267 "y.tab.c" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_Y_TAB_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 282 "y.tab.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  65
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   117

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  48
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  39
/* YYNRULES -- Number of rules.  */
#define YYNRULES  80
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  144

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   296

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      43,    44,    46,     2,    45,     2,    47,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    42,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   165,   165,   170,   184,   190,   199,   200,   201,   202,
     209,   210,   211,   212,   216,   217,   218,   219,   223,   224,
     225,   226,   227,   228,   229,   230,   234,   240,   251,   259,
     264,   272,   283,   296,   303,   310,   317,   324,   332,   339,
     346,   353,   361,   368,   375,   382,   389,   393,   400,   407,
     408,   415,   419,   426,   430,   437,   441,   448,   455,   459,
     466,   470,   477,   484,   488,   495,   499,   506,   510,   514,
     521,   525,   532,   536,   540,   544,   548,   552,   559,   563,
     569
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "RW_CREATE", "RW_DROP", "RW_TABLE",
  "RW_INDEX", "RW_LOAD", "RW_SET", "RW_HELP", "RW_PRINT", "RW_EXIT",
  "RW_SELECT", "RW_FROM", "RW_WHERE", "RW_INSERT", "RW_DELETE",
  "RW_UPDATE", "RW_AND", "RW_INTO", "RW_VALUES", "T_EQ", "T_LT", "T_LE",
  "T_GT", "T_GE", "T_NE", "T_EOF", "NOTOKEN", "RW_RESET", "RW_IO",
  "RW_BUFFER", "RW_RESIZE", "RW_QUERY_PLAN", "RW_ON", "RW_OFF",
  "RW_DISTRIBUTED", "T_INT", "T_REAL", "T_STRING", "T_QSTRING",
  "T_SHELL_CMD", "';'", "'('", "')'", "','", "'*'", "'.'", "$accept",
  "start", "command", "ddl", "dml", "utility", "queryplans", "buffer",
  "statistics", "createtable", "createindex", "droptable", "dropindex",
  "load", "set", "help", "print", "exit", "query", "insert", "delete",
  "update", "non_mt_attrtype_list", "attrtype", "non_mt_select_clause",
  "non_mt_relattr_list", "relattr", "non_mt_relation_list", "relation",
  "opt_where_clause", "non_mt_cond_list", "condition", "relattr_or_value",
  "non_mt_value_list", "value", "opt_relname", "op", "opt_distributed",
  "nothing", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,    59,    40,    41,    44,    42,    46
};
# endif

#define YYPACT_NINF -133

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-133)))

#define YYTABLE_NINF -81

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
       9,  -133,    34,    53,   -34,   -32,   -28,    -2,  -133,   -31,
       4,    14,    -4,  -133,    30,    15,    28,  -133,    64,    23,
    -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,
    -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,
      27,    29,    32,    33,    24,    48,  -133,  -133,  -133,  -133,
    -133,  -133,    26,  -133,    57,  -133,    35,    36,    37,    66,
    -133,  -133,    40,  -133,  -133,  -133,  -133,    38,    39,  -133,
      41,    43,    45,    47,    49,    50,    58,    65,    50,  -133,
      51,    52,    54,    55,  -133,  -133,  -133,    65,    42,  -133,
      59,    50,  -133,  -133,    71,    56,    60,    61,    63,    67,
    -133,  -133,    49,     7,    31,  -133,    76,    -6,  -133,    62,
      51,  -133,  -133,  -133,  -133,  -133,  -133,    68,    69,  -133,
    -133,  -133,  -133,  -133,  -133,    -6,    50,  -133,    65,  -133,
      74,  -133,  -133,  -133,  -133,     7,  -133,  -133,  -133,    72,
    -133,     7,    73,  -133
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     4,     0,     0,     0,     0,    80,     0,    41,     0,
       0,     0,     0,     5,     0,     0,     0,     3,     0,     0,
       6,     7,     8,    25,    23,    24,    10,    11,    12,    13,
      18,    20,    21,    22,    19,    14,    15,    16,    17,     9,
       0,     0,     0,     0,     0,     0,    70,    39,    71,    31,
      29,    40,    54,    50,     0,    49,    52,     0,     0,     0,
      32,    28,     0,    26,    27,     1,     2,     0,     0,    35,
       0,     0,     0,     0,     0,     0,     0,    80,     0,    30,
       0,     0,     0,     0,    38,    53,    57,    80,    56,    51,
       0,     0,    44,    59,     0,     0,     0,    47,     0,     0,
      37,    42,     0,     0,     0,    58,    61,     0,    48,    80,
       0,    34,    36,    55,    68,    69,    67,     0,    66,    76,
      72,    73,    74,    75,    77,     0,     0,    63,    80,    64,
       0,    33,    79,    46,    43,     0,    62,    60,    45,     0,
      65,     0,     0,    78
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,
    -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,  -133,
    -133,  -133,   -14,  -133,  -133,    22,   -77,    -1,  -133,   -85,
     -26,  -133,   -22,  -132,  -103,  -133,  -133,  -133,     0
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    96,    97,    54,    55,    56,    87,    88,    92,
     105,   106,   128,   117,   118,    47,   125,   131,    93
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      39,    94,   101,   140,   129,    44,    48,    45,    52,   142,
       1,    46,     2,     3,   104,    53,     4,     5,     6,     7,
       8,     9,   129,    57,    10,    11,    12,    58,    49,    50,
     127,   114,   115,    52,   116,    59,    13,    51,    14,    40,
      41,    15,    16,   138,   114,   115,    62,   116,   127,   104,
      17,   -80,   119,   120,   121,   122,   123,   124,    42,    43,
      60,    61,    63,    64,    65,    66,    67,    71,    68,    72,
      74,    69,    70,    73,    78,    76,    77,    79,    90,    91,
      75,    80,    81,    83,    82,    84,    85,   102,    86,    52,
      95,    98,   107,    99,   126,   108,   133,    89,   130,   100,
     137,   113,   103,   136,   109,     0,   110,   111,     0,   132,
       0,   112,   134,   139,   135,   141,     0,   143
};

static const yytype_int16 yycheck[] =
{
       0,    78,    87,   135,   107,    39,     6,    39,    39,   141,
       1,    39,     3,     4,    91,    46,     7,     8,     9,    10,
      11,    12,   125,    19,    15,    16,    17,    13,    30,    31,
     107,    37,    38,    39,    40,    39,    27,    39,    29,     5,
       6,    32,    33,   128,    37,    38,    31,    40,   125,   126,
      41,    42,    21,    22,    23,    24,    25,    26,     5,     6,
      30,    31,    34,    35,     0,    42,    39,    43,    39,    21,
      13,    39,    39,    47,     8,    39,    39,    37,    20,    14,
      45,    43,    43,    40,    43,    40,    39,    45,    39,    39,
      39,    39,    21,    39,    18,    39,   110,    75,    36,    44,
     126,   102,    43,   125,    44,    -1,    45,    44,    -1,   109,
      -1,    44,    44,    39,    45,    43,    -1,    44
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     1,     3,     4,     7,     8,     9,    10,    11,    12,
      15,    16,    17,    27,    29,    32,    33,    41,    49,    50,
      51,    52,    53,    54,    55,    56,    57,    58,    59,    60,
      61,    62,    63,    64,    65,    66,    67,    68,    69,    86,
       5,     6,     5,     6,    39,    39,    39,    83,    86,    30,
      31,    39,    39,    46,    72,    73,    74,    19,    13,    39,
      30,    31,    31,    34,    35,     0,    42,    39,    39,    39,
      39,    43,    21,    47,    13,    45,    39,    39,     8,    37,
      43,    43,    43,    40,    40,    39,    39,    75,    76,    73,
      20,    14,    77,    86,    74,    39,    70,    71,    39,    39,
      44,    77,    45,    43,    74,    78,    79,    21,    39,    44,
      45,    44,    44,    75,    37,    38,    40,    81,    82,    21,
      22,    23,    24,    25,    26,    84,    18,    74,    80,    82,
      36,    85,    86,    70,    44,    45,    80,    78,    77,    39,
      81,    43,    81,    44
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    48,    49,    49,    49,    49,    50,    50,    50,    50,
      51,    51,    51,    51,    52,    52,    52,    52,    53,    53,
      53,    53,    53,    53,    53,    53,    54,    54,    55,    55,
      55,    56,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    66,    67,    68,    69,    70,    70,    71,    72,
      72,    73,    73,    74,    74,    75,    75,    76,    77,    77,
      78,    78,    79,    80,    80,    81,    81,    82,    82,    82,
      83,    83,    84,    84,    84,    84,    84,    84,    85,    85,
      86
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     2,     2,     2,     2,
       3,     2,     2,     7,     6,     3,     6,     5,     4,     2,
       2,     1,     5,     7,     4,     7,     3,     1,     2,     1,
       1,     3,     1,     3,     1,     3,     1,     1,     2,     1,
       3,     1,     3,     1,     1,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     5,     1,
       0
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 166 "parse.y" /* yacc.c:1646  */
    {
      parse_tree = (yyvsp[-1].n);
      YYACCEPT;
   }
#line 1466 "y.tab.c" /* yacc.c:1646  */
    break;

  case 3:
#line 171 "parse.y" /* yacc.c:1646  */
    {
      if (!isatty(0)) {
        cout << ((yyvsp[0].sval)) << "\n";
        cout.flush();
      }
      int exitstatus = system((yyvsp[0].sval));
      if (exitstatus != 0) {
        cout << "Shell command exited with non-zero status: " << exitstatus << "\n";
        cout.flush();
      }
      parse_tree = NULL;
      YYACCEPT;
   }
#line 1484 "y.tab.c" /* yacc.c:1646  */
    break;

  case 4:
#line 185 "parse.y" /* yacc.c:1646  */
    {
      reset_scanner();
      parse_tree = NULL;
      YYACCEPT;
   }
#line 1494 "y.tab.c" /* yacc.c:1646  */
    break;

  case 5:
#line 191 "parse.y" /* yacc.c:1646  */
    {
      parse_tree = NULL;
      bExit = 1;
      YYACCEPT;
   }
#line 1504 "y.tab.c" /* yacc.c:1646  */
    break;

  case 9:
#line 203 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = NULL;
   }
#line 1512 "y.tab.c" /* yacc.c:1646  */
    break;

  case 26:
#line 235 "parse.y" /* yacc.c:1646  */
    {
      bQueryPlans = 1;
      cout << "Query plan display turned on.\n";
      (yyval.n) = NULL;
   }
#line 1522 "y.tab.c" /* yacc.c:1646  */
    break;

  case 27:
#line 241 "parse.y" /* yacc.c:1646  */
    {
      bQueryPlans = 0;
      cout << "Query plan display turned off.\n";
      (yyval.n) = NULL;
   }
#line 1532 "y.tab.c" /* yacc.c:1646  */
    break;

  case 28:
#line 252 "parse.y" /* yacc.c:1646  */
    {
      if (pPfm->ClearBuffer())
         cout << "Trouble clearing buffer!  Things may be pinned.\n";
      else
         cout << "Everything kicked out of Buffer!\n";
      (yyval.n) = NULL;
   }
#line 1544 "y.tab.c" /* yacc.c:1646  */
    break;

  case 29:
#line 260 "parse.y" /* yacc.c:1646  */
    {
      pPfm->PrintBuffer();
      (yyval.n) = NULL;
   }
#line 1553 "y.tab.c" /* yacc.c:1646  */
    break;

  case 30:
#line 265 "parse.y" /* yacc.c:1646  */
    {
      pPfm->ResizeBuffer((yyvsp[0].ival));
      (yyval.n) = NULL;
   }
#line 1562 "y.tab.c" /* yacc.c:1646  */
    break;

  case 31:
#line 273 "parse.y" /* yacc.c:1646  */
    {
      #ifdef PF_STATS
         cout << "Statistics\n";
         cout << "----------\n";
         pStatisticsMgr->Print();
      #else
         cout << "Statitisics not compiled.\n";
      #endif
      (yyval.n) = NULL;
   }
#line 1577 "y.tab.c" /* yacc.c:1646  */
    break;

  case 32:
#line 284 "parse.y" /* yacc.c:1646  */
    {
      #ifdef PF_STATS
         cout << "Statistics reset.\n";
         pStatisticsMgr->Reset();
      #else
         cout << "Statitisics not compiled.\n";
      #endif
      (yyval.n) = NULL;
   }
#line 1591 "y.tab.c" /* yacc.c:1646  */
    break;

  case 33:
#line 297 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = create_table_node((yyvsp[-4].sval), (yyvsp[-2].n), (yyvsp[0].n));
   }
#line 1599 "y.tab.c" /* yacc.c:1646  */
    break;

  case 34:
#line 304 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = create_index_node((yyvsp[-3].sval), (yyvsp[-1].sval));
   }
#line 1607 "y.tab.c" /* yacc.c:1646  */
    break;

  case 35:
#line 311 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = drop_table_node((yyvsp[0].sval));
   }
#line 1615 "y.tab.c" /* yacc.c:1646  */
    break;

  case 36:
#line 318 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = drop_index_node((yyvsp[-3].sval), (yyvsp[-1].sval));
   }
#line 1623 "y.tab.c" /* yacc.c:1646  */
    break;

  case 37:
#line 325 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = load_node((yyvsp[-3].sval), (yyvsp[-1].sval));
   }
#line 1631 "y.tab.c" /* yacc.c:1646  */
    break;

  case 38:
#line 333 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = set_node((yyvsp[-2].sval), (yyvsp[0].sval));
   }
#line 1639 "y.tab.c" /* yacc.c:1646  */
    break;

  case 39:
#line 340 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = help_node((yyvsp[0].sval));
   }
#line 1647 "y.tab.c" /* yacc.c:1646  */
    break;

  case 40:
#line 347 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = print_node((yyvsp[0].sval));
   }
#line 1655 "y.tab.c" /* yacc.c:1646  */
    break;

  case 41:
#line 354 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = NULL;
      bExit = 1;
   }
#line 1664 "y.tab.c" /* yacc.c:1646  */
    break;

  case 42:
#line 362 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = query_node((yyvsp[-3].n), (yyvsp[-1].n), (yyvsp[0].n));
   }
#line 1672 "y.tab.c" /* yacc.c:1646  */
    break;

  case 43:
#line 369 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = insert_node((yyvsp[-4].sval), (yyvsp[-1].n));
   }
#line 1680 "y.tab.c" /* yacc.c:1646  */
    break;

  case 44:
#line 376 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = delete_node((yyvsp[-1].sval), (yyvsp[0].n));
   }
#line 1688 "y.tab.c" /* yacc.c:1646  */
    break;

  case 45:
#line 383 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = update_node((yyvsp[-5].sval), (yyvsp[-3].n), (yyvsp[-1].n), (yyvsp[0].n));
   }
#line 1696 "y.tab.c" /* yacc.c:1646  */
    break;

  case 46:
#line 390 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = prepend((yyvsp[-2].n), (yyvsp[0].n));
   }
#line 1704 "y.tab.c" /* yacc.c:1646  */
    break;

  case 47:
#line 394 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = list_node((yyvsp[0].n));
   }
#line 1712 "y.tab.c" /* yacc.c:1646  */
    break;

  case 48:
#line 401 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = attrtype_node((yyvsp[-1].sval), (yyvsp[0].sval));
   }
#line 1720 "y.tab.c" /* yacc.c:1646  */
    break;

  case 50:
#line 409 "parse.y" /* yacc.c:1646  */
    {
       (yyval.n) = list_node(relattr_node(NULL, (char*)"*"));
   }
#line 1728 "y.tab.c" /* yacc.c:1646  */
    break;

  case 51:
#line 416 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = prepend((yyvsp[-2].n), (yyvsp[0].n));
   }
#line 1736 "y.tab.c" /* yacc.c:1646  */
    break;

  case 52:
#line 420 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = list_node((yyvsp[0].n));
   }
#line 1744 "y.tab.c" /* yacc.c:1646  */
    break;

  case 53:
#line 427 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = relattr_node((yyvsp[-2].sval), (yyvsp[0].sval));
   }
#line 1752 "y.tab.c" /* yacc.c:1646  */
    break;

  case 54:
#line 431 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = relattr_node(NULL, (yyvsp[0].sval));
   }
#line 1760 "y.tab.c" /* yacc.c:1646  */
    break;

  case 55:
#line 438 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = prepend((yyvsp[-2].n), (yyvsp[0].n));
   }
#line 1768 "y.tab.c" /* yacc.c:1646  */
    break;

  case 56:
#line 442 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = list_node((yyvsp[0].n));
   }
#line 1776 "y.tab.c" /* yacc.c:1646  */
    break;

  case 57:
#line 449 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = relation_node((yyvsp[0].sval));
   }
#line 1784 "y.tab.c" /* yacc.c:1646  */
    break;

  case 58:
#line 456 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = (yyvsp[0].n);
   }
#line 1792 "y.tab.c" /* yacc.c:1646  */
    break;

  case 59:
#line 460 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = NULL;
   }
#line 1800 "y.tab.c" /* yacc.c:1646  */
    break;

  case 60:
#line 467 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = prepend((yyvsp[-2].n), (yyvsp[0].n));
   }
#line 1808 "y.tab.c" /* yacc.c:1646  */
    break;

  case 61:
#line 471 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = list_node((yyvsp[0].n));
   }
#line 1816 "y.tab.c" /* yacc.c:1646  */
    break;

  case 62:
#line 478 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = condition_node((yyvsp[-2].n), (yyvsp[-1].cval), (yyvsp[0].n));
   }
#line 1824 "y.tab.c" /* yacc.c:1646  */
    break;

  case 63:
#line 485 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = relattr_or_value_node((yyvsp[0].n), NULL);
   }
#line 1832 "y.tab.c" /* yacc.c:1646  */
    break;

  case 64:
#line 489 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = relattr_or_value_node(NULL, (yyvsp[0].n));
   }
#line 1840 "y.tab.c" /* yacc.c:1646  */
    break;

  case 65:
#line 496 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = prepend((yyvsp[-2].n), (yyvsp[0].n));
   }
#line 1848 "y.tab.c" /* yacc.c:1646  */
    break;

  case 66:
#line 500 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = list_node((yyvsp[0].n));
   }
#line 1856 "y.tab.c" /* yacc.c:1646  */
    break;

  case 67:
#line 507 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = value_node(STRING, (void *) (yyvsp[0].sval));
   }
#line 1864 "y.tab.c" /* yacc.c:1646  */
    break;

  case 68:
#line 511 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = value_node(INT, (void *)& (yyvsp[0].ival));
   }
#line 1872 "y.tab.c" /* yacc.c:1646  */
    break;

  case 69:
#line 515 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = value_node(FLOAT, (void *)& (yyvsp[0].rval));
   }
#line 1880 "y.tab.c" /* yacc.c:1646  */
    break;

  case 70:
#line 522 "parse.y" /* yacc.c:1646  */
    {
      (yyval.sval) = (yyvsp[0].sval);
   }
#line 1888 "y.tab.c" /* yacc.c:1646  */
    break;

  case 71:
#line 526 "parse.y" /* yacc.c:1646  */
    {
      (yyval.sval) = NULL;
   }
#line 1896 "y.tab.c" /* yacc.c:1646  */
    break;

  case 72:
#line 533 "parse.y" /* yacc.c:1646  */
    {
      (yyval.cval) = LT_OP;
   }
#line 1904 "y.tab.c" /* yacc.c:1646  */
    break;

  case 73:
#line 537 "parse.y" /* yacc.c:1646  */
    {
      (yyval.cval) = LE_OP;
   }
#line 1912 "y.tab.c" /* yacc.c:1646  */
    break;

  case 74:
#line 541 "parse.y" /* yacc.c:1646  */
    {
      (yyval.cval) = GT_OP;
   }
#line 1920 "y.tab.c" /* yacc.c:1646  */
    break;

  case 75:
#line 545 "parse.y" /* yacc.c:1646  */
    {
      (yyval.cval) = GE_OP;
   }
#line 1928 "y.tab.c" /* yacc.c:1646  */
    break;

  case 76:
#line 549 "parse.y" /* yacc.c:1646  */
    {
      (yyval.cval) = EQ_OP;
   }
#line 1936 "y.tab.c" /* yacc.c:1646  */
    break;

  case 77:
#line 553 "parse.y" /* yacc.c:1646  */
    {
      (yyval.cval) = NE_OP;
   }
#line 1944 "y.tab.c" /* yacc.c:1646  */
    break;

  case 78:
#line 560 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = distribute_node((yyvsp[-3].sval), (yyvsp[-1].n));
   }
#line 1952 "y.tab.c" /* yacc.c:1646  */
    break;

  case 79:
#line 564 "parse.y" /* yacc.c:1646  */
    {
      (yyval.n) = NULL;
   }
#line 1960 "y.tab.c" /* yacc.c:1646  */
    break;


#line 1964 "y.tab.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 573 "parse.y" /* yacc.c:1906  */


//
// PrintError
//
// Desc: Print an error message by calling the proper component-specific
//       print-error function
//
void PrintError(RC rc)
{
   if (abs(rc) <= END_PF_WARN)
      PF_PrintError(rc);
   else if (abs(rc) <= END_RM_WARN)
      RM_PrintError(rc);
   else if (abs(rc) <= END_IX_WARN)
      IX_PrintError(rc);
   else if (abs(rc) <= END_SM_WARN)
      SM_PrintError(rc);
   else if (abs(rc) <= END_QL_WARN)
      QL_PrintError(rc);
   else if (abs(rc) <= END_EX_WARN)
      EX_PrintError(rc);
   else
      cerr << "Error code out of range: " << rc << "\n";
}

//
// RBparse
//
// Desc: Parse redbase commands
//
void RBparse(PF_Manager &pfm, SM_Manager &smm, QL_Manager &qlm)
{
   RC rc;

   // Set up global variables to their defaults
   pPfm  = &pfm;
   pSmm  = &smm;
   pQlm  = &qlm;
   bExit = 0;
   bQueryPlans = 0;

   /* Do forever */
   while (!bExit) {

      /* Reset parser and scanner for a new query */
      new_query();

      /* Print a prompt */
      cout << PROMPT;

      /* Get the prompt to actually show up on the screen */
      cout.flush();

      /* If a query was successfully read, interpret it */
      if(yyparse() == 0 && parse_tree != NULL)
         if ((rc = interp(parse_tree))) {
            PrintError(rc);
            if (rc < 0)
               bExit = TRUE;
         }
   }
}

//
// Functions for printing the various structures to an output stream
//
ostream &operator<<(ostream &s, const AttrInfo &ai)
{
   return
      s << " attrName=" << ai.attrName
      << " attrType=" <<
      (ai.attrType == INT ? "INT" :
       ai.attrType == FLOAT ? "FLOAT" : "STRING")
      << " attrLength=" << ai.attrLength;
}

ostream &operator<<(ostream &s, const RelAttr &qa)
{
   return
      s << (qa.relName ? qa.relName : "NULL")
      << "." << qa.attrName;
}

ostream &operator<<(ostream &s, const Condition &c)
{
   s << "\n      lhsAttr:" << c.lhsAttr << "\n"
      << "      op=" << c.op << "\n";
   if (c.bRhsIsAttr)
      s << "      bRhsIsAttr=TRUE \n      rhsAttr:" << c.rhsAttr;
   else
      s << "      bRshIsAttr=FALSE\n      rhsValue:" << c.rhsValue;
   return s;
}

ostream &operator<<(ostream &s, const Value &v)
{
   s << "AttrType: " << v.type;
   switch (v.type) {
      case INT:
         s << " *(int *)data=" << *(int *)v.data;
         break;
      case FLOAT:
         s << " *(float *)data=" << *(float *)v.data;
         break;
      case STRING:
         s << " (char *)data=" << (char *)v.data;
         break;
   }
   return s;
}

ostream &operator<<(ostream &s, const CompOp &op)
{
   switch(op){
      case EQ_OP:
         s << " =";
         break;
      case NE_OP:
         s << " <>";
         break;
      case LT_OP:
         s << " <";
         break;
      case LE_OP:
         s << " <=";
         break;
      case GT_OP:
         s << " >";
         break;
      case GE_OP:
         s << " >=";
         break;
      case NO_OP:
         s << " NO_OP";
         break;
   }
   return s;
}

ostream &operator<<(ostream &s, const AttrType &at)
{
   switch(at){
      case INT:
         s << "INT";
         break;
      case FLOAT:
         s << "FLOAT";
         break;
      case STRING:
         s << "STRING";
         break;
   }
   return s;
}

/*
 * Required by yacc
 */
void yyerror(char const *s) // New in 2000
{
   puts(s);
}

#if 0
/*
 * Sometimes required
 */
int yywrap(void)
{
   return 1;
}
#endif
