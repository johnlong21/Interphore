/*
** Extracted from the https://www.fossil-scm.org sources:
**
** Copyright (c) 2012 D. Richard Hipp
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the Simplified BSD License (also
** known as the "2-Clause License" or "FreeBSD License".)

** This program is distributed in the hope that it will be useful,
** but without any warranty; without even the implied warranty of
** merchantability or fitness for a particular purpose.
**
** Author contact information:
**   drh@hwaci.com
**   http://www.hwaci.com/drh/
**
*******************************************************************************
**
** This file contains code to parse a blob containing markdown text,
** using an external renderer.
*/

#ifndef JSI_LITE_ONLY
#if (defined(JSI__MARKDOWN) && JSI__MARKDOWN==1)

#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#ifndef JSI_AMALGAMATION
#include "jsiInt.h"
#endif

enum {
    MX_AUX = 5,
    MKD_CELL_ALIGN_CENTER  = 3,  /* LEFT | RIGHT */
    MKD_CELL_ALIGN_RIGHT   = 2,
    MKD_CELL_ALIGN_LEFT    = 1,
    MKD_CELL_ALIGN_MASK    = 3,
    MKD_CELL_HEAD          = 4,
    MKD_LIST_ORDERED = 1,
    MKD_CELL_ALIGN_DEFAULT = 0,
    MKD_LI_BLOCK     = 2,  /* <li> containing block data */
    MKD_LI_END = 8  /* internal list flag */
};

enum mkd_autolink {
    MKDA_NOT_AUTOLINK,    /* used internally when it is not an autolink*/
    MKDA_NORMAL,          /* normal http/http/ftp link */
    MKDA_EXPLICIT_EMAIL,  /* e-mail link with explicit mailto: */
    MKDA_IMPLICIT_EMAIL   /* e-mail link without mailto: */
};
typedef enum mkd_autolink mkd_autolink;

struct mkd_renderer {
    /* document level callbacks */
    void (*prolog)(Jsi_DString *ob, jsi_MarkdownOpts *opts);
    void (*epilog)(Jsi_DString *ob, jsi_MarkdownOpts *opts);

    /* block level callbacks - NULL skips the block */
    void (*blockcode)(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts);
    void (*blockquote)(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts);
    void (*blockhtml)(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts);
    void (*header)(Jsi_DString *ob, Jsi_DString *text,
                   int level, jsi_MarkdownOpts *opts);
    void (*hrule)(Jsi_DString *ob, jsi_MarkdownOpts *opts);
    void (*list)(Jsi_DString *ob, Jsi_DString *text, int flags, jsi_MarkdownOpts *opts);
    void (*listitem)(Jsi_DString *ob, Jsi_DString *text,
                     int flags, jsi_MarkdownOpts *opts);
    void (*paragraph)(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts);
    void (*table)(Jsi_DString *ob, Jsi_DString *head_row, Jsi_DString *rows,
                  jsi_MarkdownOpts *opts);
    void (*table_cell)(Jsi_DString *ob, Jsi_DString *text, int flags,
                       jsi_MarkdownOpts *opts);
    void (*table_row)(Jsi_DString *ob, Jsi_DString *cells, int flags,
                      jsi_MarkdownOpts *opts);

    /* span level callbacks - NULL or return 0 prints the span verbatim */
    int (*autolink)(Jsi_DString *ob, Jsi_DString *link,
                    enum mkd_autolink type, jsi_MarkdownOpts *opts);
    int (*codespan)(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts);
    int (*double_emphasis)(Jsi_DString *ob, Jsi_DString *text,
                           char c, jsi_MarkdownOpts *opts);
    int (*emphasis)(Jsi_DString *ob, Jsi_DString *text, char c, jsi_MarkdownOpts *opts);
    int (*image)(Jsi_DString *ob, Jsi_DString *link, Jsi_DString *title,
                 Jsi_DString *alt, jsi_MarkdownOpts *opts);
    int (*linebreak)(Jsi_DString *ob, jsi_MarkdownOpts *opts);
    int (*link)(Jsi_DString *ob, Jsi_DString *link, Jsi_DString *title,
                Jsi_DString *content, jsi_MarkdownOpts *opts);
    int (*raw_html_tag)(Jsi_DString *ob, Jsi_DString *tag, jsi_MarkdownOpts *opts);
    int (*triple_emphasis)(Jsi_DString *ob, Jsi_DString *text,
                           char c, jsi_MarkdownOpts *opts);

    /* low level callbacks - NULL copies input directly into the output */
    void (*entity)(Jsi_DString *ob, Jsi_DString *entity, jsi_MarkdownOpts *opts);
    void (*normal_text)(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts);

    /* renderer data */
    int max_work_stack; /* prevent arbitrary deep recursion, cf README */
    const char *emph_chars; /* chars that trigger emphasis rendering */
    jsi_MarkdownOpts *opts; /* opaque data send to every rendering callback */
};

typedef struct mkd_renderer mkd_renderer;
static void mk_markdown(Jsi_DString *ob,Jsi_DString *ib,const struct mkd_renderer *rndrer);

static char* jsi_DSInitAppend(Jsi_DString *dStr, const char *str, int len);
static int jsi_DSCompare(Jsi_DString *dStr1, Jsi_DString *dStr2);


/***************
 * LOCAL TYPES *
 ***************/

/* link_ref -- reference to a link */
struct link_ref {
    Jsi_DString id;
    Jsi_DString link;
    Jsi_DString title;
};


/* mk_char_trigger -- function pointer to render active chars */
/*   returns the number of chars taken care of */
/*   data is the pointer of the beginning of the span */
/*   offset is the number of valid chars before data */
struct mkrender;

typedef size_t (*mk_char_trigger)(
    Jsi_DString *ob,
    struct mkrender *rndr,
    char *data,
    size_t offset,
    size_t size);


/* render -- structure containing one particular render */
typedef struct mkrender {
    struct mkd_renderer make;
    Jsi_DString refs;
    mk_char_trigger active_char[256];
    int work_active;
    Jsi_DString *work;
} mk_render;


/* html_tag -- structure for quick HTML tag search (inspired from discount) */
typedef struct {
    const char *text;
    int size;
} mk_html_tag;



/********************
 * GLOBAL VARIABLES *
 ********************/

/* mk_block_tags -- recognised block tags, sorted by cmp_html_tag */
static const mk_html_tag mk_block_tags[] = {
    { "p",            1 },
    { "dl",           2 },
    { "h1",           2 },
    { "h2",           2 },
    { "h3",           2 },
    { "h4",           2 },
    { "h5",           2 },
    { "h6",           2 },
    { "ol",           2 },
    { "ul",           2 },
    { "del",          3 },
    { "div",          3 },
    { "ins",          3 },
    { "pre",          3 },
    { "form",         4 },
    { "math",         4 },
    { "table",        5 },
    { "iframe",       6 },
    { "script",       6 },
    { "fieldset",     8 },
    { "noscript",     8 },
    { "blockquote",  10 }
};

#define INS_TAG (mk_block_tags + 12)
#define DEL_TAG (mk_block_tags + 10)



/***************************
 * STATIC HELPER FUNCTIONS *
 ***************************/

/* build_ref_id -- collapse whitespace from input text to make it a ref id */
static int build_ref_id(Jsi_DString *id, const char *data, size_t size) {
    size_t beg, i;
    char *id_data;

    /* skip leading whitespace */
    while( size>0 && (data[0]==' ' || data[0]=='\t' || data[0]=='\n') ) {
        data++;
        size--;
    }

    /* skip trailing whitespace */
    while( size>0 && (data[size-1]==' '
                      || data[size-1]=='\t'
                      || data[size-1]=='\n')
         ) {
        size--;
    }
    if( size==0 ) return -1;

    /* making the ref id */
    i = 0;
    Jsi_DSFree(id);
    while( i<size ) {
        /* copy non-whitespace into the output buffer */
        beg = i;
        while( i<size && !(data[i]==' ' || data[i]=='\t' || data[i]=='\n') ) {
            i++;
        }
        Jsi_DSAppendLen(id, data+beg, i-beg);

        /* add a single space and skip all consecutive whitespace */
        if( i<size ) Jsi_DSAppendLen(id, " ", 1);
        while( i<size && (data[i]==' ' || data[i]=='\t' || data[i]=='\n') ) {
            i++;
        }
    }

    /* turn upper-case ASCII into their lower-case counterparts */
    id_data = Jsi_DSValue(id);
    for(i=0; i<Jsi_DSLength(id); i++) {
        if( id_data[i]>='A' && id_data[i]<='Z' ) id_data[i] += 'a' - 'A';
    }
    return 0;
}


/* cmp_link_ref -- comparison function for link_ref sorted arrays */
static int cmp_link_ref(const void *key, const void *array_entry) {
    struct link_ref *lr = (struct link_ref *)array_entry;
    return jsi_DSCompare((Jsi_DString *)key, &lr->id);
}


/* cmp_link_ref_sort -- comparison function for link_ref qsort */
static int cmp_link_ref_sort(const void *a, const void *b) {
    struct link_ref *lra = (struct link_ref *)a;
    struct link_ref *lrb = (struct link_ref *)b;
    return jsi_DSCompare(&lra->id, &lrb->id);
}


/* cmp_html_tag -- comparison function for bsearch() (stolen from discount) */
static int cmp_html_tag(const void *a, const void *b) {
    const mk_html_tag *hta = (const mk_html_tag*)a;
    const mk_html_tag *htb = (const mk_html_tag*)b;
    if( hta->size!=htb->size ) return hta->size-htb->size;
    return Jsi_Strncasecmp(hta->text, htb->text, hta->size);
}


/* find_block_tag -- returns the current block tag */
static const mk_html_tag *find_block_tag(const char *data, size_t size) {
    size_t i = 0;
    mk_html_tag key;

    /* looking for the word end */
    while( i<size
            && ((data[i]>='0' && data[i]<='9')
                || (data[i]>='A' && data[i]<='Z')
                || (data[i]>='a' && data[i]<='z'))
         ) {
        i++;
    }
    if( i>=size ) return 0;

    /* binary search of the tag */
    key.text = data;
    key.size = i;
    return (const mk_html_tag *)bsearch(&key,
                   mk_block_tags,
                   (sizeof(mk_block_tags)/sizeof(mk_block_tags[0])),
                   sizeof mk_block_tags[0],
                   cmp_html_tag);
}


/* new_work_buffer -- get a new working buffer from the stack or create one */
static Jsi_DString *new_work_buffer(mk_render *rndr) {
    Jsi_DString *ret = 0;

    if( rndr->work_active < rndr->make.max_work_stack ) {
        ret = rndr->work + rndr->work_active;
        rndr->work_active += 1;
        Jsi_DSFree(ret);
    }
    return ret;
}


/* release_work_buffer -- release the given working buffer */
static void release_work_buffer(mk_render *rndr, Jsi_DString *buf) {
    if( !buf ) return;
    assert(rndr->work_active>0 && buf==(rndr->work+rndr->work_active-1));
    rndr->work_active -= 1;
}



/****************************
 * INLINE PARSING FUNCTIONS *
 ****************************/

/* is_mail_autolink -- looks for the address part of a mail autolink and '>' */
/* this is less strict than the original markdown e-mail address matching */
static size_t is_mail_autolink(char *data, size_t size) {
    size_t i = 0, nb = 0;
    /* address is assumed to be: [-@._a-zA-Z0-9]+ with exactly one '@' */
    while( i<size && (data[i]=='-'
                      || data[i]=='.'
                      || data[i]=='_'
                      || data[i]=='@'
                      || (data[i]>='a' && data[i]<='z')
                      || (data[i]>='A' && data[i]<='Z')
                      || (data[i]>='0' && data[i]<='9'))
         ) {
        if( data[i]=='@' ) nb++;
        i++;
    }
    if( i>=size || data[i]!='>' || nb!=1 ) return 0;
    return i+1;
}


/* tag_length -- returns the length of the given tag, or 0 if it's not valid */
static size_t tag_length(char *data, size_t size, enum mkd_autolink *autolink) {
    size_t i, j;

    /* a valid tag can't be shorter than 3 chars */
    if( size<3 ) return 0;

    /* begins with a '<' optionally followed by '/', followed by letter */
    if( data[0]!='<' ) return 0;
    i = (data[1]=='/') ? 2 : 1;
    if( (data[i]<'a' || data[i]>'z') &&  (data[i]<'A' || data[i]>'Z') ) {
        return 0;
    }

    /* scheme test */
    *autolink = MKDA_NOT_AUTOLINK;
    if( size>6
            && Jsi_Strncasecmp(data+1, "http", 4)==0
            && (data[5]==':'
                || ((data[5]=='s' || data[5]=='S') && data[6]==':'))
      ) {
        i = (data[5]==':') ? 6 : 7;
        *autolink = MKDA_NORMAL;
    } else if( size>5 && Jsi_Strncasecmp(data+1, "ftp:", 4)==0 ) {
        i = 5;
        *autolink = MKDA_NORMAL;
    } else if( size>7 && Jsi_Strncasecmp(data+1, "mailto:", 7)==0 ) {
        i = 8;
        /* not changing *autolink to go to the address test */
    }

    /* completing autolink test: no whitespace or ' or " */
    if( i>=size || i=='>' ) {
        *autolink = MKDA_NOT_AUTOLINK;
    } else if( *autolink ) {
        j = i;
        while( i<size
                && data[i]!='>'
                && data[i]!='\''
                && data[i]!='"'
                && data[i]!=' '
                && data[i]!='\t'
                && data[i]!='\n'
             ) {
            i++;
        }
        if( i>=size ) return 0;
        if( i>j && data[i]=='>' ) return i+1;
        /* one of the forbidden chars has been found */
        *autolink = MKDA_NOT_AUTOLINK;
    } else if( (j = is_mail_autolink(data+i, size-i))!=0 ) {
        *autolink = (i==8) ? MKDA_EXPLICIT_EMAIL : MKDA_IMPLICIT_EMAIL;
        return i+j;
    }

    /* looking for something looking like a tag end */
    while( i<size && data[i]!='>' ) {
        i++;
    }
    if( i>=size ) return 0;
    return i+1;
}


/* parse_inline -- parses inline markdown elements */
static void parse_inline(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size
) {
    size_t i = 0, end = 0;
    mk_char_trigger action = 0;
    Jsi_DString work = {};

    while( i<size ) {
        /* copying inactive chars into the output */
        while( end<size
                && (action = rndr->active_char[(unsigned char)data[end]])==0
             ) {
            end++;
        }
        if( end>i ) {
            if( rndr->make.normal_text ) {
                jsi_DSInitAppend(&work, data+i, end-i);
                rndr->make.normal_text(ob, &work, rndr->make.opts);
            } else {
                Jsi_DSAppendLen(ob, data+i, end-i);
            }
        }
        if( end>=size ) break;
        i = end;

        /* calling the trigger */
        end = action(ob, rndr, data+i, i, size-i);
        if( !end ) {
            /* no action from the callback */
            end = i+1;
        } else {
            i += end;
            end = i;
        }
    }
}


/* find_emph_char -- looks for the next emph char, skipping other constructs */
static size_t find_emph_char(char *data, size_t size, char c) {
    size_t i = 1;

    while( i<size ) {
        while( i<size && data[i]!=c && data[i]!='`' && data[i]!='[' ) {
            i++;
        }
        if( i>=size ) return 0;

        /* not counting escaped chars */
        if( i && data[i-1]=='\\' ) {
            i++;
            continue;
        }

        if( data[i]==c ) return i;

        /* skipping a code span */
        if( data[i]=='`' ) {
            size_t span_nb = 0, bt;
            size_t tmp_i = 0;

            /* counting the number of opening backticks */
            while( i<size && data[i]=='`' ) {
                i++;
                span_nb++;
            }
            if( i>=size ) return 0;

            /* finding the matching closing sequence */
            bt = 0;
            while( i<size && bt<span_nb ) {
                if( !tmp_i && data[i]==c ) tmp_i = i;
                if( data[i]=='`' ) bt += 1;
                else bt = 0;
                i++;
            }
            if( i>=size ) return tmp_i;
            i++;

            /* skipping a link */
        } else if( data[i]=='[' ) {
            size_t tmp_i = 0;
            char cc;
            i++;
            while( i<size && data[i]!=']' ) {
                if( !tmp_i && data[i]==c ) tmp_i = i;
                i++;
            }
            i++;
            while( i<size && (data[i]==' ' || data[i]=='\t' || data[i]=='\n') ) {
                i++;
            }
            if( i>=size ) return tmp_i;
            if( data[i]!='[' && data[i]!='(' ) { /* not a link*/
                if( tmp_i ) return tmp_i;
                else continue;
            }
            cc = data[i];
            i++;
            while( i<size && data[i]!=cc ) {
                if( !tmp_i && data[i]==c ) tmp_i = i;
                i++;
            }
            if( i>=size ) return tmp_i;
            i++;
        }
    }
    return 0;
}


/* parse_emph1 -- parsing single emphasis */
/* closed by a symbol not preceded by whitespace and not followed by symbol */
static size_t parse_emph1(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size,
    char c
) {
    size_t i = 0, len;
    Jsi_DString *work = 0;
    int r;

    if( !rndr->make.emphasis ) return 0;

    /* skipping one symbol if coming from emph3 */
    if( size>1 && data[0]==c && data[1]==c ) i = 1;

    while( i<size ) {
        len = find_emph_char(data+i, size-i, c);
        if( !len ) return 0;
        i += len;
        if( i>=size ) return 0;

        if( i+1<size && data[i+1]==c ) {
            i++;
            continue;
        }
        if( data[i]==c
                && data[i-1]!=' '
                && data[i-1]!='\t'
                && data[i-1]!='\n'
          ) {
            work = new_work_buffer(rndr);
            if( !work ) return 0;
            parse_inline(work, rndr, data, i);
            r = rndr->make.emphasis(ob, work, c, rndr->make.opts);
            release_work_buffer(rndr, work);
            return r ? i+1 : 0;
        }
    }
    return 0;
}


/* parse_emph2 -- parsing single emphasis */
static size_t parse_emph2(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size,
    char c
) {
    size_t i = 0, len;
    Jsi_DString *work = 0;
    int r;

    if( !rndr->make.double_emphasis ) return 0;

    while( i<size ) {
        len = find_emph_char(data+i, size-i, c);
        if( !len ) return 0;
        i += len;
        if( i+1<size
                && data[i]==c
                && data[i+1]==c
                && i
                && data[i-1]!=' '
                && data[i-1]!='\t'
                && data[i-1]!='\n'
          ) {
            work = new_work_buffer(rndr);
            if( !work ) return 0;
            parse_inline(work, rndr, data, i);
            r = rndr->make.double_emphasis(ob, work, c, rndr->make.opts);
            release_work_buffer(rndr, work);
            return r ? i+2 : 0;
        }
        i++;
    }
    return 0;
}


/* parse_emph3 -- parsing single emphasis */
/* finds the first closing tag, and delegates to the other emph */
static size_t parse_emph3(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size,
    char c
) {
    size_t i = 0, len;
    int r;

    while( i<size ) {
        len = find_emph_char(data+i, size-i, c);
        if( !len ) return 0;
        i += len;

        /* skip whitespace preceded symbols */
        if( data[i]!=c || data[i-1]==' ' || data[i-1]=='\t' || data[i-1]=='\n' ) {
            continue;
        }

        if( i+2<size
                && data[i+1]==c
                && data[i+2] == c
                && rndr->make.triple_emphasis
          ) {
            /* triple symbol found */
            Jsi_DString *work = new_work_buffer(rndr);
            if( !work ) return 0;
            parse_inline(work, rndr, data, i);
            r = rndr->make.triple_emphasis(ob, work, c, rndr->make.opts);
            release_work_buffer(rndr, work);
            return r ? i+3 : 0;
        } else if( i+1<size && data[i+1]==c ) {
            /* double symbol found, handing over to emph1 */
            len = parse_emph1(ob, rndr, data-2, size+2, c);
            return len ? len-2 : 0;
        } else {
            /* single symbol found, handing over to emph2 */
            len = parse_emph2(ob, rndr, data-1, size+1, c);
            return len ? len-1 : 0;
        }
    }
    return 0;
}


/* char_emphasis -- single and double emphasis parsing */
static size_t char_emphasis(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t offset,
    size_t size
) {
    char c = data[0];
    size_t ret;

    if( size>2 && data[1]!=c ) {
        /* whitespace cannot follow an opening emphasis */
        if( data[1]==' '
                || data[1]=='\t'
                || data[1]=='\n'
                || (ret = parse_emph1(ob, rndr, data+1, size-1, c))==0
          ) {
            return 0;
        }
        return ret+1;
    }

    if( size>3 && data[1]==c && data[2]!=c ) {
        if( data[2]==' '
                || data[2]=='\t'
                || data[2]=='\n'
                || (ret = parse_emph2(ob, rndr, data+2, size-2, c))==0
          ) {
            return 0;
        }
        return ret+2;
    }

    if( size>4 && data[1]==c && data[2]==c && data[3]!=c ) {
        if( data[3]==' '
                || data[3]=='\t'
                || data[3]=='\n'
                || (ret = parse_emph3(ob, rndr, data+3, size-3, c))==0
          ) {
            return 0;
        }
        return ret+3;
    }
    return 0;
}


/* char_linebreak -- '\n' preceded by two spaces (assuming linebreak != 0) */
static size_t char_linebreak(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t offset,
    size_t size
) {
    if( offset<2 || data[-1]!=' ' || data[-2]!=' ' ) return 0;
    /* removing the last space from ob and rendering */\
    int dsl = Jsi_DSLength(ob);
    if( dsl>0 && Jsi_DSValue(ob)[dsl-1]==' ' ) Jsi_DSSetLength(ob, dsl-1);
    return rndr->make.linebreak(ob, rndr->make.opts) ? 1 : 0;
}


/* char_codespan -- '`' parsing a code span (assuming codespan != 0) */
static size_t char_codespan(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t offset,
    size_t size
) {
    size_t end, nb = 0, i, f_begin, f_end;

    /* counting the number of backticks in the delimiter */
    while( nb<size && data[nb]=='`' ) {
        nb++;
    }

    /* finding the next delimiter */
    i = 0;
    for(end=nb; end<size && i<nb; end++) {
        if( data[end]=='`' ) i++;
        else i = 0;
    }
    if( i<nb && end>=size ) return 0; /* no matching delimiter */

    /* trimming outside whitespaces */
    f_begin = nb;
    while( f_begin<end && (data[f_begin]==' ' || data[f_begin]=='\t') ) {
        f_begin++;
    }
    f_end = end-nb;
    while( f_end>nb && (data[f_end-1]==' ' || data[f_end-1]=='\t') ) {
        f_end--;
    }

    /* real code span */
    if( f_begin<f_end ) {
        Jsi_DString work = {};
        jsi_DSInitAppend(&work, data+f_begin, f_end-f_begin);
        if( !rndr->make.codespan(ob, &work, rndr->make.opts) ) end = 0;
    } else {
        if( !rndr->make.codespan(ob, 0, rndr->make.opts) ) end = 0;
    }
    return end;
}


/* char_escape -- '\\' backslash escape */
static size_t char_escape(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t offset,
    size_t size
) {
    Jsi_DString work = {};
    if( size>1 ) {
        if( rndr->make.normal_text ) {
            jsi_DSInitAppend(&work, data+1,1);
            rndr->make.normal_text(ob, &work, rndr->make.opts);
        } else {
            Jsi_DSAppendLen(ob, data+1, 1);
        }
    }
    return 2;
}


/* char_entity -- '&' escaped when it doesn't belong to an entity */
/* valid entities are assumed to be anything matching &#?[A-Za-z0-9]+; */
static size_t char_entity(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t offset,
    size_t size
) {
    size_t end = 1;
    Jsi_DString work = {};
    if( end<size && data[end]=='#' ) end++;
    while( end<size
            && ((data[end]>='0' && data[end]<='9')
                || (data[end]>='a' && data[end]<='z')
                || (data[end]>='A' && data[end]<='Z'))
         ) {
        end++;
    }
    if( end<size && data[end]==';' ) {
        /* real entity */
        end++;
    } else {
        /* lone '&' */
        return 0;
    }
    if( rndr->make.entity ) {
        jsi_DSInitAppend(&work, data, end);
        rndr->make.entity(ob, &work, rndr->make.opts);
    } else {
        Jsi_DSAppendLen(ob, data, end);
    }
    return end;
}


/* char_langle_tag -- '<' when tags or autolinks are allowed */
static size_t char_langle_tag(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t offset,
    size_t size
) {
    enum mkd_autolink altype = MKDA_NOT_AUTOLINK;
    size_t end = tag_length(data, size, &altype);
    Jsi_DString work = {};
    int ret = 0;
    if( end ) {
        if( rndr->make.autolink && altype!=MKDA_NOT_AUTOLINK ) {
            jsi_DSInitAppend(&work, data+1, end-2);
            ret = rndr->make.autolink(ob, &work, altype, rndr->make.opts);
        } else if( rndr->make.raw_html_tag ) {
            jsi_DSInitAppend(&work, data, end);
            ret = rndr->make.raw_html_tag(ob, &work, rndr->make.opts);
        }
    }

    if( !ret ) {
        return 0;
    } else {
        return end;
    }
}


/* get_link_inline -- extract inline-style link and title from
** parenthesed data
*/
static int get_link_inline(
    Jsi_DString *link,
    Jsi_DString *title,
    char *data,
    size_t size
) {
    size_t i = 0, mark;
    size_t link_b, link_e;
    size_t title_b = 0, title_e = 0;

    /* skipping initial whitespace */
    while( i<size && (data[i]==' ' || data[i]=='\t' || data[i]=='\n') ) {
        i++;
    }
    link_b = i;

    /* looking for link end: ' " */
    while( i<size && data[i]!='\'' && data[i]!='"' ) {
        i++;
    }
    link_e = i;

    /* looking for title end if present */
    if( data[i]=='\'' || data[i]=='"' ) {
        i++;
        title_b = i;

        /* skipping whitespaces after title */
        title_e = size-1;
        while( title_e>title_b
                && (data[title_e]==' '
                    || data[title_e]=='\t'
                    || data[title_e]=='\n')
             ) {
            title_e--;
        }

        /* checking for closing quote presence */
        if (data[title_e] != '\'' &&  data[title_e] != '"') {
            title_b = title_e = 0;
            link_e = i;
        }
    }

    /* remove whitespace at the end of the link */
    while( link_e>link_b
            && (data[link_e-1]==' '
                || data[link_e-1]=='\t'
                || data[link_e-1]=='\n')
         ) {
        link_e--;
    }

    /* remove optional angle brackets around the link */
    if( data[link_b]=='<' ) link_b += 1;
    if( data[link_e-1]=='>' ) link_e -= 1;

    /* escape backslashed character from link */
    Jsi_DSFree(link);
    i = link_b;
    while( i<link_e ) {
        mark = i;
        while( i<link_e && data[i]!='\\' ) {
            i++;
        }
        Jsi_DSAppendLen(link, data+mark, i-mark);
        while( i<link_e && data[i]=='\\' ) {
            i++;
        }
    }

    /* handing back title */
    Jsi_DSFree(title);
    if( title_e>title_b ) Jsi_DSAppendLen(title, data+title_b, title_e-title_b);

    /* this function always succeed */
    return 0;
}


/* get_link_ref -- extract referenced link and title from id */
static int get_link_ref(
    mk_render *rndr,
    Jsi_DString *link,
    Jsi_DString *title,
    char *data,
    size_t size
) {
    struct link_ref *lr;

    /* find the link from its id (stored temporarily in link) */
    Jsi_DSFree(link);
    if( build_ref_id(link, data, size)<0 ) return -1;
    lr = (struct link_ref *)bsearch(link,
                 Jsi_DSValue(&rndr->refs),
                 Jsi_DSLength(&rndr->refs)/sizeof(struct link_ref),
                 sizeof (struct link_ref),
                 cmp_link_ref);
    if( !lr ) return -1;

    /* fill the output buffers */
    Jsi_DSFree(link);
    Jsi_DSFree(title);
    Jsi_DSAppendLen(link, Jsi_DSValue(&lr->link), Jsi_DSLength(&lr->link));
    Jsi_DSAppendLen(title, Jsi_DSValue(&lr->title), Jsi_DSLength(&lr->title));
    return 0;
}


/* char_link -- '[': parsing a link or an image */
static size_t char_link(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t offset,
    size_t size
) {
    int is_img = (offset && data[-1] == '!'), level;
    size_t i = 1, txt_e;
    Jsi_DString *content = 0;
    Jsi_DString *link = 0;
    Jsi_DString *title = 0;
    int ret;

    /* checking whether the correct renderer exists */
    if( (is_img && !rndr->make.image) || (!is_img && !rndr->make.link) ) {
        return 0;
    }

    /* looking for the matching closing bracket */
    for(level=1; i<size; i++) {
        if( data[i]=='\n' )        /* do nothing */;
        else if( data[i-1]=='\\' ) continue;
        else if( data[i]=='[' )    level += 1;
        else if( data[i]==']' ) {
            level--;
            if( level<=0 ) break;
        }
    }
    if( i>=size ) return 0;
    txt_e = i;
    i++;

    /* skip any amount of whitespace or newline */
    /* (this is much more laxist than original markdown syntax) */
    while( i<size && (data[i]==' ' || data[i]=='\t' || data[i]=='\n') ) {
        i++;
    }

    /* allocate temporary buffers to store content, link and title */
    content = new_work_buffer(rndr);
    link = new_work_buffer(rndr);
    title = new_work_buffer(rndr);
    if( !title ) return 0;
    ret = 0; /* error if we don't get to the callback */

    /* inline style link */
    if( i<size && data[i]=='(' ) {
        size_t span_end = i;
        while( span_end<size
                && !(data[span_end]==')' && (span_end==i || data[span_end-1]!='\\'))
             ) {
            span_end++;
        }

        if( span_end>=size
                || get_link_inline(link, title, data+i+1, span_end-(i+1))<0
          ) {
            goto char_link_cleanup;
        }

        i = span_end+1;

        /* reference style link */
    } else if( i<size && data[i]=='[' ) {
        char *id_data;
        size_t id_size, id_end = i;

        while( id_end<size && data[id_end]!=']' ) {
            id_end++;
        }

        if( id_end>=size ) goto char_link_cleanup;

        if( i+1==id_end ) {
            /* implicit id - use the contents */
            id_data = data+1;
            id_size = txt_e-1;
        } else {
            /* explicit id - between brackets */
            id_data = data+i+1;
            id_size = id_end-(i+1);
        }

        if( get_link_ref(rndr, link, title, id_data, id_size)<0 ) {
            goto char_link_cleanup;
        }

        i = id_end+1;

        /* shortcut reference style link */
    } else {
        if( get_link_ref(rndr, link, title, data+1, txt_e-1)<0 ) {
            goto char_link_cleanup;
        }

        /* rewinding the whitespace */
        i = txt_e+1;
    }

    /* building content: img alt is escaped, link content is parsed */
    if( txt_e>1 ) {
        if( is_img ) Jsi_DSAppendLen(content, data+1, txt_e-1);
        else parse_inline(content, rndr, data+1, txt_e-1);
    }

    /* calling the relevant rendering function */
    if( is_img ) {
        int dsl = Jsi_DSLength(ob);
        if( dsl>0 && Jsi_DSValue(ob)[dsl-1]=='!' ) Jsi_DSSetLength(ob, dsl-1);
        ret = rndr->make.image(ob, link, title, content, rndr->make.opts);
    } else {
        ret = rndr->make.link(ob, link, title, content, rndr->make.opts);
    }

    /* cleanup */
char_link_cleanup:
    release_work_buffer(rndr, title);
    release_work_buffer(rndr, link);
    release_work_buffer(rndr, content);
    return ret ? i : 0;
}



/*********************************
 * BLOCK-LEVEL PARSING FUNCTIONS *
 *********************************/

/* is_empty -- returns the line length when it is empty, 0 otherwise */
static size_t is_empty(const char *data, size_t size) {
    size_t i;
    for(i=0; i<size && data[i]!='\n'; i++) {
        if( data[i]!=' ' && data[i]!='\t' ) return 0;
    }
    return i+1;
}


/* is_hrule -- returns whether a line is a horizontal rule */
static int is_hrule(char *data, size_t size) {
    size_t i = 0, n = 0;
    char c;

    /* skipping initial spaces */
    if( size<3 ) return 0;
    if( data[0]==' ' ) {
        i++;
        if( data[1]==' ' ) {
            i++;
            if( data[2]==' ' ) {
                i++;
            }
        }
    }

    /* looking at the hrule char */
    if( i+2>=size || (data[i]!='*' && data[i]!='-' && data[i]!='_') ) return 0;
    c = data[i];

    /* the whole line must be the char or whitespace */
    while (i < size && data[i] != '\n') {
        if( data[i]==c ) {
            n += 1;
        } else if( data[i]!=' ' && data[i]!='\t' ) {
            return 0;
        }
        i++;
    }

    return n>=3;
}


/* is_headerline -- returns whether the line is a setext-style hdr underline */
static int is_headerline(char *data, size_t size) {
    size_t i = 0;

    /* test of level 1 header */
    if( data[i]=='=' ) {
        for(i=1; i<size && data[i]=='='; i++);
        while( i<size && (data[i]==' ' || data[i]=='\t') ) {
            i++;
        }
        return (i>=size || data[i]=='\n') ? 1 : 0;
    }

    /* test of level 2 header */
    if( data[i]=='-' ) {
        for(i=1; i<size && data[i]=='-'; i++);
        while( i<size && (data[i]==' ' || data[i]=='\t') ) {
            i++;
        }
        return (i>=size || data[i]=='\n') ? 2 : 0;
    }

    return 0;
}


/* is_table_sep -- returns whether there is a table separator at pos */
static int is_table_sep(char *data, size_t pos) {
    return data[pos]=='|' && (pos==0 || data[pos-1]!='\\');
}


/* is_tableline -- returns the number of column tables in the given line */
static int is_tableline(char *data, size_t size) {
    size_t i = 0;
    int n_sep = 0, outer_sep = 0;

    /* skip initial blanks */
    while( i<size && (data[i]==' ' || data[i]=='\t') ) {
        i++;
    }

    /* check for initial '|' */
    if( i<size && data[i]=='|') outer_sep++;

    /* count the number of pipes in the line */
    for(n_sep=0; i<size && data[i]!='\n'; i++) {
        if( is_table_sep(data, i) ) n_sep++;
    }

    /* march back to check for optional last '|' before blanks and EOL */
    while( i && (data[i-1]==' ' || data[i-1]=='\t' || data[i-1]=='\n') ) {
        i--;
    }
    if( i && is_table_sep(data, i-1) ) outer_sep += 1;

    /* return the number of column or 0 if it's not a table line */
    return (n_sep>0) ? (n_sep-outer_sep+1) : 0;
}


/* prefix_quote -- returns blockquote prefix length */
static size_t prefix_quote(char *data, size_t size) {
    size_t i = 0;
    if( i<size && data[i]==' ' ) i++;
    if( i<size && data[i]==' ' ) i++;
    if( i<size && data[i]==' ' ) i++;
    if( i<size && data[i]=='>' ) {
        if( i+1<size && (data[i+1]==' ' || data[i+1]=='\t') ) {
            return i + 2;
        } else {
            return i + 1;
        }
    } else {
        return 0;
    }
}


/* prefix_code -- returns prefix length for block code */
static size_t prefix_code(char *data, size_t size) {
    if( size>0 && data[0]=='\t' ) return 1;
    if( size>3 && data[0]==' ' && data[1]==' ' && data[2]==' ' && data[3]==' ' ) {
        return 4;
    }
    return 0;
}

/* prefix_oli -- returns ordered list item prefix */
static size_t prefix_oli(char *data, size_t size) {
    size_t i = 0;
    if( i<size && data[i]==' ') i++;
    if( i<size && data[i]==' ') i++;
    if( i<size && data[i]==' ') i++;

    if( i>=size || data[i]<'0' || data[i]>'9' ) return 0;
    while( i<size && data[i]>='0' && data[i]<='9' ) {
        i++;
    }

    if( i+1>=size
            || data[i]!='.'
            || (data[i+1]!=' ' && data[i+1]!='\t')
      ) {
        return 0;
    }
    i = i+2;
    while( i<size && (data[i]==' ' || data[i]=='\t') ) {
        i++;
    }
    return i;
}


/* prefix_uli -- returns ordered list item prefix */
static size_t prefix_uli(char *data, size_t size) {
    size_t i = 0;
    if( i<size && data[i]==' ') i++;
    if( i<size && data[i]==' ') i++;
    if( i<size && data[i]==' ') i++;
    if( i+1>=size
            || (data[i]!='*' && data[i]!='+' && data[i]!='-')
            || (data[i+1]!=' ' && data[i+1]!='\t')
      ) {
        return 0;
    }
    i = i+2;
    while( i<size && (data[i]==' ' || data[i]=='\t') ) {
        i++;
    }
    return i;
}


/* parse_block predeclaration */
static void parse_block(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size);


/* parse_blockquote -- handles parsing of a blockquote fragment */
static size_t parse_blockquote(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size
) {
    size_t beg, end = 0, pre, work_size = 0;
    char *work_data = 0;
    Jsi_DString *out = new_work_buffer(rndr);

    beg = 0;
    while( beg<size ) {
        for(end=beg+1; end<size && data[end-1]!='\n'; end++);
        pre = prefix_quote(data+beg, end-beg);
        if( pre ) {
            beg += pre; /* skipping prefix */
        } else if( is_empty(data+beg, end-beg)
                   && (end>=size
                       || (prefix_quote(data+end, size-end)==0
                           && !is_empty(data+end, size-end)))
                 ) {
            /* empty line followed by non-quote line */
            break;
        }
        if( beg<end ) { /* copy into the in-place working buffer */
            if( !work_data ) {
                work_data = data+beg;
            } else if( (data+beg)!=(work_data+work_size) ) {
                memmove(work_data+work_size, data+beg, end-beg);
            }
            work_size += end-beg;
        }
        beg = end;
    }

    if( rndr->make.blockquote ) {
        Jsi_DString fallback = {};
        if( out ) {
            parse_block(out, rndr, work_data, work_size);
        } else {
            jsi_DSInitAppend(&fallback, work_data, work_size);
        }
        rndr->make.blockquote(ob, out ? out : &fallback, rndr->make.opts);
    }
    release_work_buffer(rndr, out);
    return end;
}


/* parse_paragraph -- handles parsing of a regular paragraph */
static size_t parse_paragraph(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size
) {
    size_t i = 0, end = 0;
    int level = 0;
    char *work_data = data;
    size_t work_size = 0;
    Jsi_DString fallback = {};

    while( i<size ) {
        for(end=i+1; end<size && data[end-1]!='\n'; end++);
        if( is_empty(data+i, size-i)
                || (level = is_headerline(data+i, size-i))!= 0
          ) {
            break;
        }
        if( (i && data[i]=='#') || is_hrule(data+i, size-i) ) {
            end = i;
            break;
        }
        i = end;
    }

    work_size = i;
    while( work_size && data[work_size-1]=='\n' ) {
        work_size--;
    }

    if( !level ) {
        if( rndr->make.paragraph ) {
            Jsi_DString *tmp = new_work_buffer(rndr);
            if( tmp ) {
                parse_inline(tmp, rndr, work_data, work_size);
            } else {
                jsi_DSInitAppend(&fallback, work_data, work_size);
            }
            rndr->make.paragraph(ob, tmp ? tmp : &fallback, rndr->make.opts);
            release_work_buffer(rndr, tmp);
        }
    } else {
        if( work_size ) {
            size_t beg;
            i = work_size;
            work_size -= 1;
            while( work_size && data[work_size]!='\n' ) {
                work_size--;
            }
            beg = work_size+1;
            while( work_size && data[work_size-1]=='\n') {
                work_size--;
            }
            if( work_size ) {
                Jsi_DString *tmp = new_work_buffer(rndr);
                if( tmp ) {
                    parse_inline(tmp, rndr, work_data, work_size);
                } else {
                    jsi_DSInitAppend (&fallback, work_data, work_size);
                }
                if( rndr->make.paragraph ) {
                    rndr->make.paragraph(ob, tmp ? tmp : &fallback, rndr->make.opts);
                }
                release_work_buffer(rndr, tmp);
                work_data += beg;
                work_size = i - beg;
            } else {
                work_size = i;
            }
        }

        if( rndr->make.header ) {
            Jsi_DString *span = new_work_buffer(rndr);
            if( span ) {
                parse_inline(span, rndr, work_data, work_size);
                rndr->make.header(ob, span, level, rndr->make.opts);
            } else {
                jsi_DSInitAppend(&fallback, work_data, work_size);
                rndr->make.header(ob, &fallback, level, rndr->make.opts);
            }
            release_work_buffer(rndr, span);
        }
    }
    return end;
}


/* parse_blockcode -- handles parsing of a block-level code fragment */
static size_t parse_blockcode(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size
) {
    size_t beg, end, pre;
    Jsi_DString *work = new_work_buffer(rndr);
    if( !work ) work = ob;

    beg = 0;
    while( beg<size ) {
        for(end=beg+1; end<size && data[end-1]!='\n'; end++);
        pre = prefix_code(data+beg, end-beg);
        if( pre ) {
            beg += pre; /* skipping prefix */
        } else if( !is_empty(data+beg, end-beg) ) {
            /* non-empty non-prefixed line breaks the pre */
            break;
        }
        if( beg<end ) {
            /* verbatim copy to the working buffer, escaping entities */
            if( is_empty(data + beg, end - beg) ) {
                Jsi_DSAppendLen(work, "\n", 1);
            } else {
                Jsi_DSAppendLen(work, data+beg, end-beg);
            }
        }
        beg = end;
    }

    end = Jsi_DSLength(work);
    while( end>0 && Jsi_DSValue(work)[end-1]=='\n' ) {
        Jsi_DSSetLength(work, --end);
    }
    Jsi_DSAppendLen(work, "\n", 1);

    if( work!=ob ) {
        if( rndr->make.blockcode ) {
            rndr->make.blockcode(ob, work, rndr->make.opts);
        }
        release_work_buffer(rndr, work);
    }
    return beg;
}


/* parse_listitem -- parsing of a single list item */
/*  assuming initial prefix is already removed */
static size_t parse_listitem(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size,
    int *flags
) {
    Jsi_DString fallback = {};
    Jsi_DString *work = 0, *inter = 0;
    size_t beg = 0, end, pre, sublist = 0, orgpre = 0, i;
    int in_empty = 0, has_inside_empty = 0;

    /* keeping track of the first indentation prefix */
    if( size>1 && data[0]==' ' ) {
        orgpre = 1;
        if( size>2 && data[1]==' ' ) {
            orgpre = 2;
            if( size>3 && data[2]==' ' ) {
                orgpre = 3;
            }
        }
    }
    beg = prefix_uli(data, size);
    if( !beg ) beg = prefix_oli(data, size);
    if( !beg ) return 0;
    /* skipping to the beginning of the following line */
    end = beg;
    while( end<size && data[end-1]!='\n' ) {
        end++;
    }

    /* getting working buffers */
    work = new_work_buffer(rndr);
    inter = new_work_buffer(rndr);
    if( !work ) work = &fallback;

    /* putting the first line into the working buffer */
    Jsi_DSAppendLen(work, data+beg, end-beg);
    beg = end;

    /* process the following lines */
    while( beg<size ) {
        end++;
        while( end<size && data[end-1]!='\n' ) {
            end++;
        }

        /* process an empty line */
        if( is_empty(data+beg, end-beg) ) {
            in_empty = 1;
            beg = end;
            continue;
        }

        /* computing the indentation */
        i = 0;
        if( end-beg>1 && data[beg]==' ' ) {
            i = 1;
            if( end-beg>2 && data[beg+1]==' ' ) {
                i = 2;
                if( end-beg>3 && data[beg+2]==' ' ) {
                    i = 3;
                    if( end-beg>3 && data[beg+3]==' ' ) {
                        i = 4;
                    }
                }
            }
        }
        pre = i;
        if( data[beg]=='\t' ) {
            i = 1;
            pre = 8;
        }

        /* checking for a new item */
        if( (prefix_uli(data+beg+i, end-beg-i) && !is_hrule(data+beg+i, end-beg-i))
                || prefix_oli(data+beg+i, end-beg-i)
          ) {
            if( in_empty ) has_inside_empty = 1;
            if( pre == orgpre ) { /* the following item must have */
                break;             /* the same indentation */
            }
            if( !sublist ) sublist = Jsi_DSLength(work);

            /* joining only indented stuff after empty lines */
        } else if( in_empty && i<4 && data[beg]!='\t' ) {
            *flags |= MKD_LI_END;
            break;
        } else if( in_empty ) {
            Jsi_DSAppendLen(work, "\n", 1);
            has_inside_empty = 1;
        }
        in_empty = 0;

        /* adding the line without prefix into the working buffer */
        Jsi_DSAppendLen(work, data+beg+i, end-beg-i);
        beg = end;
    }

    /* non-recursive fallback when working buffer stack is full */
    if( !inter ) {
        if( rndr->make.listitem ) {
            rndr->make.listitem(ob, work, *flags, rndr->make.opts);
        }
        if( work!=&fallback ) release_work_buffer(rndr, work);
        Jsi_DSFree(&fallback);
        return beg;
    }

    /* render of li contents */
    if( has_inside_empty ) *flags |= MKD_LI_BLOCK;
    if( *flags & MKD_LI_BLOCK ) {
        /* intermediate render of block li */
        if( sublist && sublist<Jsi_DSLength(work) ) {
            parse_block(inter, rndr, Jsi_DSValue(work), sublist);
            parse_block(inter,
                        rndr,
                        Jsi_DSValue(work)+sublist,
                        Jsi_DSLength(work)-sublist);
        } else {
            parse_block(inter, rndr, Jsi_DSValue(work), Jsi_DSLength(work));
        }
    } else {
        /* intermediate render of inline li */
        if( sublist && sublist<Jsi_DSLength(work) ) {
            parse_inline(inter, rndr, Jsi_DSValue(work), sublist);
            parse_block(inter,
                        rndr,
                        Jsi_DSValue(work)+sublist,
                        Jsi_DSLength(work)-sublist);
        } else {
            parse_inline(inter, rndr, Jsi_DSValue(work), Jsi_DSLength(work));
        }
    }

    /* render of li itself */
    if( rndr->make.listitem ) {
        rndr->make.listitem(ob, inter, *flags, rndr->make.opts);
    }
    release_work_buffer(rndr, inter);
    if( work!=&fallback ) release_work_buffer(rndr, work);
    Jsi_DSFree(&fallback);
    return beg;
}


/* parse_list -- parsing ordered or unordered list block */
static size_t parse_list(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size,
    int flags
) {
    Jsi_DString fallback = {};
    Jsi_DString *work = new_work_buffer(rndr);
    size_t i = 0, j;
    if( !work ) work = &fallback;

    while( i<size ) {
        j = parse_listitem(work, rndr, data+i, size-i, &flags);
        i += j;
        if( !j || (flags & MKD_LI_END) ) break;
    }

    if( rndr->make.list ) rndr->make.list(ob, work, flags, rndr->make.opts);
    if( work!=&fallback ) {
        release_work_buffer(rndr, work);
        Jsi_DSFree(work);
    }
    Jsi_DSFree(&fallback);
    return i;
}


/* parse_atxheader -- parsing of atx-style headers */
static size_t parse_atxheader(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size
) {
    int level = 0;
    size_t i, end, skip, span_beg, span_size;

    if( !size || data[0]!='#' ) return 0;

    while( (size_t)level<size && level<6 && data[level]=='#' ) {
        level++;
    }
    for(i=level; i<size && (data[i]==' ' || data[i]=='\t'); i++);
    span_beg = i;

    for(end=i; end<size && data[end]!='\n'; end++);
    skip = end;
    if( end<=i ) return parse_paragraph(ob, rndr, data, size);
    while( end && data[end-1]=='#' ) {
        end--;
    }
    while( end && (data[end-1]==' ' || data[end-1]=='\t') ) {
        end--;
    }
    if( end<=i ) return parse_paragraph(ob, rndr, data, size);

    span_size = end-span_beg;
    if( rndr->make.header ) {
        Jsi_DString fallback = {};
        Jsi_DString *span = new_work_buffer(rndr);

        if( span ) {
            parse_inline(span, rndr, data+span_beg, span_size);
        } else {
            jsi_DSInitAppend(&fallback, data+span_beg, span_size);
        }
        rndr->make.header(ob, span ? span : &fallback, level, rndr->make.opts);
        release_work_buffer(rndr, span);
    }
    return skip;
}


/* htmlblock_end -- checking end of HTML block : </tag>[ \t]*\n[ \t*]\n */
/*  returns the length on match, 0 otherwise */
static size_t htmlblock_end(
    const mk_html_tag *tag,
    const char *data,
    size_t size
) {
    size_t i, w;

    /* assuming data[0]=='<' && data[1]=='/' already tested */

    /* checking tag is a match */
    if( (tag->size+3)>=(int)size
            || Jsi_Strncasecmp(data+2, tag->text, tag->size)
            || data[tag->size+2]!='>'
      ) {
        return 0;
    }

    /* checking white lines */
    i = tag->size + 3;
    w = 0;
    if( i<size && (w = is_empty(data+i, size-i))==0 ) {
        return 0; /* non-blank after tag */
    }
    i += w;
    w = 0;

    if( i<size && (w = is_empty(data + i, size - i))==0 ) {
        return 0; /* non-blank line after tag line */
    }
    return i+w;
}


/* parse_htmlblock -- parsing of inline HTML block */
static size_t parse_htmlblock(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size
) {
    size_t i, j = 0;
    const mk_html_tag *curtag;
    int found;
    size_t work_size = 0;
    Jsi_DString work = {};

    /* identification of the opening tag */
    if( size<2 || data[0]!='<' ) return 0;
    curtag = find_block_tag(data+1, size-1);

    /* handling of special cases */
    if( !curtag ) {

        /* HTML comment, laxist form */
        if( size>5 && data[1]=='!' && data[2]=='-' && data[3]=='-' ) {
            i = 5;
            while( i<size && !(data[i-2]=='-' && data[i-1]=='-' && data[i]=='>') ) {
                i++;
            }
            i++;
            if( i<size ) {
                j = is_empty(data+i, size-i);
                if( j ) {
                    work_size = i+j;
                    if( !rndr->make.blockhtml ) return work_size;
                    jsi_DSInitAppend(&work, data, work_size);
                    rndr->make.blockhtml(ob, &work, rndr->make.opts);
                    return work_size;
                }
            }
        }

        /* HR, which is the only self-closing block tag considered */
        if( size>4
                && (data[1]=='h' || data[1]=='H')
                && (data[2]=='r' || data[2]=='R')
          ) {
            i = 3;
            while( i<size && data[i]!='>' ) {
                i++;
            }
            if( i+1<size ) {
                i += 1;
                j = is_empty(data+i, size-i);
                if( j ) {
                    work_size = i+j;
                    if( !rndr->make.blockhtml ) return work_size;
                    jsi_DSInitAppend(&work, data, work_size);
                    rndr->make.blockhtml(ob, &work, rndr->make.opts);
                    return work_size;
                }
            }
        }

        /* no special case recognised */
        return 0;
    }

    /* looking for an unindented matching closing tag */
    /*  followed by a blank line */
    i = 1;
    found = 0;
#if 0
    while( i<size ) {
        i++;
        while( i<size && !(data[i-2]=='\n' && data[i-1]=='<' && data[i]=='/') ) {
            i++;
        }
        if( (i+2+curtag->size)>=size ) break;
        j = htmlblock_end(curtag, data+i-1, size-i+1);
        if (j) {
            i += j-1;
            found = 1;
            break;
        }
    }
#endif

    /* if not found, trying a second pass looking for indented match */
    /* but not if tag is "ins" or "del" (following original Markdown.pl) */
    if( !found && curtag!=INS_TAG && curtag!=DEL_TAG ) {
        i = 1;
        while( i<size ) {
            i++;
            while( i<size && !(data[i-1]=='<' && data[i]=='/') ) {
                i++;
            }
            if( (i+2+curtag->size)>=size ) break;
            j = htmlblock_end(curtag, data+i-1, size-i+1);
            if (j) {
                i += j-1;
                found = 1;
                break;
            }
        }
    }

    if( !found ) return 0;

    /* the end of the block has been found */
    jsi_DSInitAppend(&work, data, i);
    if( rndr->make.blockhtml ) {
        rndr->make.blockhtml(ob, &work, rndr->make.opts);
    }
    return i;
}


/* parse_table_cell -- parse a cell inside a table */
static void parse_table_cell(
    Jsi_DString *ob,     /* output blob */
    mk_render *rndr, /* renderer description */
    char *data,          /* input text */
    size_t size,         /* input text size */
    int flags            /* table flags */
) {
    Jsi_DString fallback = {};
    Jsi_DString *span = new_work_buffer(rndr);

    if( span ) {
        parse_inline(span, rndr, data, size);
    } else {
        jsi_DSInitAppend(&fallback, data, size);
    }
    rndr->make.table_cell(ob, span ? span : &fallback, flags, rndr->make.opts);
    release_work_buffer(rndr, span);
}

static char *jsi_DSInitAppend(Jsi_DString *dStr, const char *str, int len)
{
    Jsi_DSInit(dStr);
    return Jsi_DSAppendLen(dStr, str, len);
}

static int jsi_DSCompare(Jsi_DString *dStr1, Jsi_DString *dStr2)
{
    int l1 = Jsi_DSLength(dStr1);
    int l2 = Jsi_DSLength(dStr2);
    char *c1 = Jsi_DSValue(dStr1);
    char *c2 = Jsi_DSValue(dStr2);
    if (l1==l2) return memcmp(c1, c2, l1);
    int c = memcmp(c1, c2, (l1<l2?l1:l2));
    if (c) return c;
    return (l1<l2?-1:1);
}

static void jsi_DSzero(Jsi_DString *da, int n) {
    int i;
    for(i=0; i<n; i++) {
        Jsi_DString *dStr1 = da+i;
        int l1 = Jsi_DSLength(dStr1);
        char *c1 = Jsi_DSValue(dStr1);
        memset(c1, 0, l1);
    }
}


/* parse_table_row -- parse an input line into a table row */
static size_t parse_table_row(
    Jsi_DString *ob,        /* output blob for rendering */
    mk_render *rndr,    /* renderer description */
    char *data,             /* input text */
    size_t size,            /* input text size */
    int *aligns,            /* array of default alignment for columns */
    size_t align_size,      /* number of columns with default alignment */
    int flags               /* table flags */
) {
    size_t i = 0, col = 0;
    size_t beg, end, total = 0;
    Jsi_DString *cells = new_work_buffer(rndr);
    int align;

    /* skip leading blanks and separator */
    while( i<size && (data[i]==' ' || data[i]=='\t') ) {
        i++;
    }
    if( i<size && data[i]=='|' ) i++;

    /* go over all the cells */
    while( i<size && total==0 ) {
        /* check optional left/center align marker */
        align = 0;
        if( data[i]==':' ) {
            align |= MKD_CELL_ALIGN_LEFT;
            i++;
        }

        /* skip blanks */
        while( i<size && (data[i]==' ' || data[i]=='\t') ) {
            i++;
        }
        beg = i;

        /* forward to the next separator or EOL */
        while( i<size && !is_table_sep(data, i) && data[i]!='\n' ) {
            i++;
        }
        end = i;
        if( i<size ) {
            i++;
            if( data[i-1]=='\n' ) total = i;
        }

        /* check optional right/center align marker */
        if( i>beg && data[end-1]==':' ) {
            align |= MKD_CELL_ALIGN_RIGHT;
            end--;
        }

        /* remove trailing blanks */
        while( end>beg && (data[end-1]==' ' || data[end-1]=='\t') ) {
            end--;
        }

        /* skip the last cell if it was only blanks */
        /* (because it is only the optional end separator) */
        if( total && end<=beg ) continue;

        /* fallback on default alignment if not explicit */
        if( align==0 && aligns && col<align_size ) align = aligns[col];

        /* render cells */
        if( cells ) parse_table_cell(cells, rndr, data+beg, end-beg, align|flags);

        col++;
    }

    /* render the whole row and clean up */
    if( cells ) {
        rndr->make.table_row(ob, cells, flags, rndr->make.opts);
    } else {
        Jsi_DString fallback = {};
        jsi_DSInitAppend(&fallback, data, total ? total : size);
        rndr->make.table_row(ob, &fallback, flags, rndr->make.opts);
    }
    release_work_buffer(rndr, cells);
    return total ? total : size;
}


/* parse_table -- parsing of a whole table */
static size_t parse_table(
    Jsi_DString *ob,
    mk_render *rndr,
    char *data,
    size_t size
) {
    size_t i = 0, head_end, col;
    size_t align_size = 0;
    int *aligns = 0;
    Jsi_DString fallback = {};
    Jsi_DString *head = 0;
    Jsi_DString *rows = new_work_buffer(rndr);
    if( !rows ) rows = &fallback;

    /* skip the first (presumably header) line */
    while( i<size && data[i]!='\n' ) {
        i++;
    }
    head_end = i;

    /* fallback on end of input */
    if( i>=size ) {
        parse_table_row(rows, rndr, data, size, 0, 0, 0);
        rndr->make.table(ob, 0, rows, rndr->make.opts);
        if( rows!=&fallback ) release_work_buffer(rndr, rows);
        return i;
    }

    /* attempt to parse a table rule, i.e. blanks, dash, colons and sep */
    i++;
    col = 0;
    while( i<size
            && (data[i]==' '
                || data[i]=='\t'
                || data[i]=='-'
                || data[i] == ':'
                || data[i] =='|')
         ) {
        if( data[i] == '|' ) align_size++;
        if( data[i] == ':' ) col = 1;
        i += 1;
    }

    if( i<size && data[i]=='\n' ) {
        align_size++;

        /* render the header row */
        head = new_work_buffer(rndr);
        if( head ) {
            parse_table_row(head, rndr, data, head_end, 0, 0, MKD_CELL_HEAD);
        }

        /* parse alignments if provided */
        if( col && (aligns=(int*)malloc(align_size * sizeof *aligns))!=0 ) {
            for(i=0; i<align_size; i++) aligns[i] = 0;
            col = 0;
            i = head_end+1;

            /* skip initial white space and optional separator */
            while( i<size && (data[i]==' ' || data[i]=='\t') ) {
                i++;
            }
            if( data[i]=='|' ) i++;

            /* compute default alignment for each column */
            while (i < size && data[i] != '\n') {
                if (data[i] == ':')
                    aligns[col] |= MKD_CELL_ALIGN_LEFT;
                while (i < size
                        && data[i] != '|' && data[i] != '\n')
                    i += 1;
                if (data[i - 1] == ':')
                    aligns[col] |= MKD_CELL_ALIGN_RIGHT;
                if (i < size && data[i] == '|')
                    i += 1;
                col += 1;
            }
        }

        /* point i to the beginning of next line/row */
        i++;

    } else {
        /* there is no valid ruler, continuing without header */
        i = 0;
    }

    /* render the table body lines */
    while( i<size && is_tableline(data + i, size - i) ) {
        i += parse_table_row(rows, rndr, data+i, size-i, aligns, align_size, 0);
    }

    /* render the full table */
    rndr->make.table(ob, head, rows, rndr->make.opts);

    /* cleanup */
    if( head ) release_work_buffer(rndr, head);
    if( rows!=&fallback ) release_work_buffer(rndr, rows);
    free(aligns);
    return i;
}


/* parse_block -- parsing of one block, returning next char to parse */
static void parse_block(
    Jsi_DString *ob,        /* output blob */
    mk_render *rndr,    /* renderer internal state */
    char *data,             /* input text */
    size_t size             /* input text size */
) {
    size_t beg, end, i;
    char *txt_data;
    int has_table = (rndr->make.table
                     && rndr->make.table_row
                     && rndr->make.table_cell);

    beg = 0;
    while( beg<size ) {
        txt_data = data+beg;
        end = size-beg;
        if( data[beg]=='#' ) {
            beg += parse_atxheader(ob, rndr, txt_data, end);
        } else if( data[beg]=='<'
                   && rndr->make.blockhtml
                   && (i = parse_htmlblock(ob, rndr, txt_data, end))!=0
                 ) {
            beg += i;
        } else if( (i=is_empty(txt_data, end))!=0 ) {
            beg += i;
        } else if( is_hrule(txt_data, end) ) {
            if( rndr->make.hrule ) rndr->make.hrule(ob, rndr->make.opts);
            while( beg<size && data[beg]!='\n' ) {
                beg++;
            }
            beg++;
        } else if( prefix_quote(txt_data, end) ) {
            beg += parse_blockquote(ob, rndr, txt_data, end);
        } else if( prefix_code(txt_data, end) ) {
            beg += parse_blockcode(ob, rndr, txt_data, end);
        } else if( prefix_uli(txt_data, end) ) {
            beg += parse_list(ob, rndr, txt_data, end, 0);
        } else if( prefix_oli(txt_data, end) ) {
            beg += parse_list(ob, rndr, txt_data, end, MKD_LIST_ORDERED);
        } else if( has_table && is_tableline(txt_data, end) ) {
            beg += parse_table(ob, rndr, txt_data, end);
        } else {
            beg += parse_paragraph(ob, rndr, txt_data, end);
        }
    }
}



/*********************
 * REFERENCE PARSING *
 *********************/

/* is_ref -- returns whether a line is a reference or not */
static int is_ref(
    char *data,         /* input text */
    size_t beg,         /* offset of the beginning of the line */
    size_t end,         /* offset of the end of the text */
    size_t *last,       /* last character of the link */
    Jsi_DString *refs   /* array of link references */
) {
    size_t i = 0;
    size_t id_offset, id_end;
    size_t link_offset, link_end;
    size_t title_offset, title_end;
    size_t line_end;
    struct link_ref lr = {
        {},
        {},
        {}
    };

    /* up to 3 optional leading spaces */
    if( beg+3>=end ) return 0;
    if( data[beg]==' ' ) {
        i = 1;
        if( data[beg+1]==' ' ) {
            i = 2;
            if( data[beg+2]==' ' ) {
                i = 3;
                if( data[beg+3]==' ' ) return 0;
            }
        }
    }
    i += beg;

    /* id part: anything but a newline between brackets */
    if( data[i]!='[' ) return 0;
    i++;
    id_offset = i;
    while( i<end && data[i]!='\n' && data[i]!='\r' && data[i]!=']' ) {
        i++;
    }
    if( i>=end || data[i]!=']' ) return 0;
    id_end = i;

    /* spacer: colon (space | tab)* newline? (space | tab)* */
    i++;
    if( i>=end || data[i]!=':' ) return 0;
    i++;
    while( i<end && (data[i]==' ' || data[i]=='\t') ) {
        i++;
    }
    if( i<end && (data[i]=='\n' || data[i]=='\r') ) {
        i++;
        if( i<end && data[i]=='\r' && data[i-1] == '\n' ) i++;
    }
    while( i<end && (data[i]==' ' || data[i]=='\t') ) {
        i++;
    }
    if( i>=end ) return 0;

    /* link: whitespace-free sequence, optionally between angle brackets */
    if( data[i]=='<' ) i++;
    link_offset = i;
    while( i<end
            && data[i]!=' '
            && data[i]!='\t'
            && data[i]!='\n'
            && data[i]!='\r'
         ) {
        i += 1;
    }
    if( data[i-1]=='>' ) link_end = i-1;
    else link_end = i;

    /* optional spacer: (space | tab)* (newline | '\'' | '"' | '(' ) */
    while( i<end && (data[i]==' ' || data[i]=='\t') ) {
        i++;
    }
    if( i<end
            && data[i]!='\n'
            && data[i]!='\r'
            && data[i]!='\''
            && data[i]!='"'
            && data[i]!='('
      ) {
        return 0;
    }
    line_end = 0;
    /* computing end-of-line */
    if( i>=end || data[i]=='\r' || data[i]=='\n' ) line_end = i;
    if( i+1<end && data[i]=='\n' && data[i+1]=='\r' ) line_end = i+1;

    /* optional (space|tab)* spacer after a newline */
    if( line_end ) {
        i = line_end+1;
        while( i<end && (data[i]==' ' || data[i]=='\t') ) {
            i++;
        }
    }

    /* optional title: any non-newline sequence enclosed in '"()
            alone on its line */
    title_offset = title_end = 0;
    if( i+1<end && (data[i]=='\'' || data[i]=='"' || data[i]=='(') ) {
        i += 1;
        title_offset = i;
        /* looking for EOL */
        while( i<end && data[i]!='\n' && data[i]!='\r' ) {
            i++;
        }
        if( i+1<end && data[i]=='\n' && data[i+1]=='\r' ) {
            title_end = i + 1;
        } else {
            title_end = i;
        }
        /* stepping back */
        i--;
        while( i>title_offset && (data[i]==' ' || data[i]=='\t') ) {
            i--;
        }
        if( i>title_offset && (data[i]=='\'' || data[i]=='"' || data[i]==')') ) {
            line_end = title_end;
            title_end = i;
        }
    }
    if( !line_end ) return 0; /* garbage after the link */

    /* a valid ref has been found, filling-in return structures */
    if( last ) *last = line_end;
    if( !refs ) return 1;
    if( build_ref_id(&lr.id, data+id_offset, id_end-id_offset)<0 ) return 0;
    Jsi_DSAppendLen(&lr.link, data+link_offset, link_end-link_offset);
    if( title_end>title_offset ) {
        Jsi_DSAppendLen(&lr.title, data+title_offset, title_end-title_offset);
    }
    Jsi_DSAppendLen(refs, (char *)&lr, sizeof lr);
    return 1;
}

/* markdown -- parses the input buffer and renders it into the output buffer */
static void mk_markdown(
    Jsi_DString *ob,                   /* output blob for rendered text */
    Jsi_DString *ib,                   /* input blob in markdown */
    const struct mkd_renderer *rndrer  /* renderer descriptor (callbacks) */
) {
    struct link_ref *lr;
    Jsi_DString text = {};
    size_t i, beg, end = 0;
    mk_render rndr;
    char *ib_data;
    jsi_MarkdownOpts *opts = rndrer->opts;
     
    /* filling the render structure */
    if( !rndrer ) return;
    rndr.make = *rndrer;
    if( rndr.make.max_work_stack<1 ) rndr.make.max_work_stack = 1;
    rndr.work_active = 0;
    rndr.work = (Jsi_DString*)Jsi_Calloc(rndr.make.max_work_stack, sizeof *rndr.work);
    rndr.refs = text;
    for(i=0; i<256; i++) rndr.active_char[i] = 0;
    if( (rndr.make.emphasis
            || rndr.make.double_emphasis
            || rndr.make.triple_emphasis)
            && rndr.make.emph_chars
      ) {
        for(i=0; rndr.make.emph_chars[i]; i++) {
            rndr.active_char[(unsigned char)rndr.make.emph_chars[i]] = char_emphasis;
        }
    }
    if( rndr.make.codespan ) rndr.active_char['`'] = char_codespan;
    if( rndr.make.linebreak ) rndr.active_char['\n'] = char_linebreak;
    if( rndr.make.image || rndr.make.link ) rndr.active_char['['] = char_link;
    rndr.active_char['<'] = char_langle_tag;
    rndr.active_char['\\'] = char_escape;
    rndr.active_char['&'] = char_entity;

    /* first pass: looking for references, copying everything else */
    beg = 0;
    ib_data = Jsi_DSValue(ib);
    while( beg<Jsi_DSLength(ib) ) { /* iterating over lines */
        if( is_ref(ib_data, beg, Jsi_DSLength(ib), &end, &rndr.refs) ) {
            beg = end;
        } else { /* skipping to the next line */
            end = beg;
            while( end<Jsi_DSLength(ib) && ib_data[end]!='\n' && ib_data[end]!='\r' ) {
                end += 1;
            }
            /* adding the line body if present */
            if( end>beg ) Jsi_DSAppendLen(&text, ib_data + beg, end - beg);
            while( end<Jsi_DSLength(ib) && (ib_data[end]=='\n' || ib_data[end]=='\r') ) {
                /* add one \n per newline */
                if( ib_data[end]=='\n'
                        || (end+1<Jsi_DSLength(ib) && ib_data[end+1]!='\n')
                  ) {
                    Jsi_DSAppendLen(&text, "\n", 1);
                }
                end += 1;
            }
            beg = end;
        }
    }

    /* sorting the reference array */
    if( Jsi_DSLength(&rndr.refs) ) {
        qsort(Jsi_DSValue(&rndr.refs),
              Jsi_DSLength(&rndr.refs)/sizeof(struct link_ref),
              sizeof(struct link_ref),
              cmp_link_ref_sort);
    }

    /* second pass: actual rendering */
    if( rndr.make.prolog ) rndr.make.prolog(ob, opts);
    parse_block(ob, &rndr, Jsi_DSValue(&text), Jsi_DSLength(&text));
    if( rndr.make.epilog ) rndr.make.epilog(ob, opts);

    /* clean-up */
    Jsi_DSFree(&text);
    lr = (struct link_ref *)Jsi_DSValue(&rndr.refs);
    end = Jsi_DSLength(&rndr.refs)/sizeof(struct link_ref);
    for(i=0; i<end; i++) {
        Jsi_DSFree(&lr[i].id);
        Jsi_DSFree(&lr[i].link);
        Jsi_DSFree(&lr[i].title);
    }
    Jsi_DSFree(&rndr.refs);
    jsi_DSzero(rndr.work, rndr.make.max_work_stack);
    free(rndr.work);
}

/* INTER_BLOCK -- skip a line between block level elements */
#define INTER_BLOCK(ob) \
  do { if( Jsi_DSLength(ob)>0 ) Jsi_DSAppendLen(ob, "\n", 1); } while (0)

/* BLOB_APPEND_LITERAL -- append a string literal to a blob */
#define BLOB_APPEND_LITERAL(blob, literal) \
  Jsi_DSAppendLen((blob), "" literal, (sizeof literal)-1)
/*
 * The empty string in the second argument leads to a syntax error
 * when the macro is not used with a string literal. Unfortunately
 * the error is not overly explicit.
 */

/* BLOB_APPEND_BLOB -- append blob contents to another */
#define BLOB_APPEND_BLOB(dest, src) \
  Jsi_DSAppendLen((dest), Jsi_DSValue(src), Jsi_DSLength(src))


/* HTML escape */

static void html_escape(Jsi_DString *ob, const char *data, size_t size) {
    size_t beg = 0, i = 0;
    while( i<size ) {
        beg = i;
        while( i<size
                && data[i]!='<'
                && data[i]!='>'
                && data[i]!='"'
                && data[i]!='&'
             ) {
            i++;
        }
        Jsi_DSAppendLen(ob, data+beg, i-beg);
        while( i<size ) {
            if( data[i]=='<' ) {
                BLOB_APPEND_LITERAL(ob, "&lt;");
            } else if( data[i]=='>' ) {
                BLOB_APPEND_LITERAL(ob, "&gt;");
            } else if( data[i]=='&' ) {
                BLOB_APPEND_LITERAL(ob, "&amp;");
            } else if( data[i]=='"' ) {
                BLOB_APPEND_LITERAL(ob, "&quot;");
            } else if( data[i]=='\'' ) {
                BLOB_APPEND_LITERAL(ob, "&#39;");
            } else {
                break;
            }
            i++;
        }
    }
}


/* HTML block tags */

/* Size of the prolog: "<div class='markdown'>\n" */
#define PROLOG_SIZE 23

static void html_prolog(Jsi_DString *ob, jsi_MarkdownOpts *opts) {
    INTER_BLOCK(ob);
    BLOB_APPEND_LITERAL(ob, "<div class=\"markdown\">\n");
    assert( Jsi_DSLength(ob)==PROLOG_SIZE );
}

static void html_epilog(Jsi_DString *ob, jsi_MarkdownOpts *opts) {
    INTER_BLOCK(ob);
    BLOB_APPEND_LITERAL(ob, "</div>\n");
}

static int htmlTagLength(const char *z) {
    int n = 1;
    int inparen = 0;
    int c;
    if( z[n]=='/' ) {
        n++;
    }
    if( !isalpha(z[n]) ) return 0;
    while( isalnum(z[n]) || z[n]=='-' ) {
        n++;
    }
    c = z[n];
    if( c=='/' && z[n+1]=='>' ) {
        return n+2;
    }
    if( c!='>' && !isspace(c) ) return 0;
    while( (c = z[n])!=0 && (c!='>' || inparen) ) {
        if( c==inparen ) {
            inparen = 0;
        } else if( inparen==0 && (c=='"' || c=='\'') ) {
            inparen = c;
        }
        n++;
    }
    if( z[n]!='>' ) return 0;
    return n+1;
}


static void html_raw_block(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts) {
    char *data = Jsi_DSValue(text);
    size_t size = Jsi_DSLength(text);
//    Jsi_DString *title = (Jsi_DString*)opaque;
    while( size>0 && isspace(data[0]) ) {
        data++;
        size--;
    }
    while( size>0 && isspace(data[size-1]) ) {
        size--;
    }
    /* If the first raw block is an <h1> element, then use it as the title. */
    if( Jsi_DSLength(ob)<=PROLOG_SIZE
            && size>9
            && opts->getTitle
            && Jsi_Strncasecmp("<h1",data,3)==0
            && Jsi_Strncasecmp("</h1>", &data[size-5],5)==0
      ) {
        int nTag = htmlTagLength(data);
        opts->getTitle = 0;
        Jsi_DSAppendLen(&opts->titleStr, data+nTag, size - nTag - 5);
        return;
    }
    INTER_BLOCK(ob);
    Jsi_DSAppendLen(ob, data, size);
    BLOB_APPEND_LITERAL(ob, "\n");
}

static void html_blockcode(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts) {
    INTER_BLOCK(ob);
    BLOB_APPEND_LITERAL(ob, "<pre><code>");
    html_escape(ob, Jsi_DSValue(text), Jsi_DSLength(text));
    BLOB_APPEND_LITERAL(ob, "</code></pre>\n");
}

static void html_blockquote(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts) {
    INTER_BLOCK(ob);
    BLOB_APPEND_LITERAL(ob, "<blockquote>\n");
    BLOB_APPEND_BLOB(ob, text);
    BLOB_APPEND_LITERAL(ob, "</blockquote>\n");
}

static void html_header(
    Jsi_DString *ob,
    Jsi_DString *text,
    int level,
    jsi_MarkdownOpts *opts
) {
    /* The first header at the beginning of a text is considered as
     * a title and not output. */
    if( Jsi_DSLength(ob)<=PROLOG_SIZE && opts->getTitle) {
        BLOB_APPEND_BLOB(&opts->titleStr, text);
        opts->getTitle = 0;
        return;
    }
    INTER_BLOCK(ob);
    Jsi_DSPrintf(ob, "<h%d>", level);
    BLOB_APPEND_BLOB(ob, text);
    Jsi_DSPrintf(ob, "</h%d>", level);
}

static void html_hrule(Jsi_DString *ob, jsi_MarkdownOpts *opts) {
    INTER_BLOCK(ob);
    BLOB_APPEND_LITERAL(ob, "<hr />\n");
}


static void html_list(
    Jsi_DString *ob,
    Jsi_DString *text,
    int flags,
    jsi_MarkdownOpts *opts
) {
    char ol[] = "ol";
    char ul[] = "ul";
    char *tag = (flags & MKD_LIST_ORDERED) ? ol : ul;
    INTER_BLOCK(ob);
    Jsi_DSPrintf(ob, "<%s>\n", tag);
    BLOB_APPEND_BLOB(ob, text);
    Jsi_DSPrintf(ob, "</%s>\n", tag);
}

static void html_list_item(
    Jsi_DString *ob,
    Jsi_DString *text,
    int flags,
    jsi_MarkdownOpts *opts
) {
    char *text_data = Jsi_DSValue(text);
    size_t text_size = Jsi_DSLength(text);
    while( text_size>0 && text_data[text_size-1]=='\n' ) text_size--;
    BLOB_APPEND_LITERAL(ob, "<li>");
    Jsi_DSAppendLen(ob, text_data, text_size);
    BLOB_APPEND_LITERAL(ob, "</li>\n");
}

static void html_paragraph(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts) {
    INTER_BLOCK(ob);
    BLOB_APPEND_LITERAL(ob, "<p>");
    BLOB_APPEND_BLOB(ob, text);
    BLOB_APPEND_LITERAL(ob, "</p>\n");
}


static void html_table(
    Jsi_DString *ob,
    Jsi_DString *head_row,
    Jsi_DString *rows,
    jsi_MarkdownOpts *opts
) {
    INTER_BLOCK(ob);
    BLOB_APPEND_LITERAL(ob, "<table>\n");
    if( head_row && Jsi_DSLength(head_row)>0 ) {
        BLOB_APPEND_LITERAL(ob, "<thead>\n");
        BLOB_APPEND_BLOB(ob, head_row);
        BLOB_APPEND_LITERAL(ob, "</thead>\n<tbody>\n");
    }
    if( rows ) {
        BLOB_APPEND_BLOB(ob, rows);
    }
    if( head_row && Jsi_DSLength(head_row)>0 ) {
        BLOB_APPEND_LITERAL(ob, "</tbody>\n");
    }
    BLOB_APPEND_LITERAL(ob, "</table>\n");
}

static void html_table_cell(
    Jsi_DString *ob,
    Jsi_DString *text,
    int flags,
    jsi_MarkdownOpts *opts
) {
    if( flags & MKD_CELL_HEAD ) {
        BLOB_APPEND_LITERAL(ob, "    <th");
    } else {
        BLOB_APPEND_LITERAL(ob, "    <td");
    }
    switch( flags & MKD_CELL_ALIGN_MASK ) {
    case MKD_CELL_ALIGN_LEFT: {
        BLOB_APPEND_LITERAL(ob, " align=\"left\"");
        break;
    }
    case MKD_CELL_ALIGN_RIGHT: {
        BLOB_APPEND_LITERAL(ob, " align=\"right\"");
        break;
    }
    case MKD_CELL_ALIGN_CENTER: {
        BLOB_APPEND_LITERAL(ob, " align=\"center\"");
        break;
    }
    }
    BLOB_APPEND_LITERAL(ob, ">");
    BLOB_APPEND_BLOB(ob, text);
    if( flags & MKD_CELL_HEAD ) {
        BLOB_APPEND_LITERAL(ob, "</th>\n");
    } else {
        BLOB_APPEND_LITERAL(ob, "</td>\n");
    }
}

static void html_table_row(
    Jsi_DString *ob,
    Jsi_DString *cells,
    int flags,
    jsi_MarkdownOpts *opts
) {
    BLOB_APPEND_LITERAL(ob, "  <tr>\n");
    BLOB_APPEND_BLOB(ob, cells);
    BLOB_APPEND_LITERAL(ob, "  </tr>\n");
}



/* HTML span tags */

static int html_raw_span(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts) {
    /* If the document begins with a <h1> markup, take that as the header. */
    BLOB_APPEND_BLOB(ob, text);
    return 1;
}

static int html_autolink(
    Jsi_DString *ob,
    Jsi_DString *link,
    enum mkd_autolink type,
    jsi_MarkdownOpts *opts
) {
    if( !link || Jsi_DSLength(link)<=0 ) return 0;
    BLOB_APPEND_LITERAL(ob, "<a href=\"");
    if( type==MKDA_IMPLICIT_EMAIL ) BLOB_APPEND_LITERAL(ob, "mailto:");
    html_escape(ob, Jsi_DSValue(link), Jsi_DSLength(link));
    BLOB_APPEND_LITERAL(ob, "\">");
    if( type==MKDA_EXPLICIT_EMAIL && Jsi_DSLength(link)>7 ) {
        /* remove "mailto:" from displayed text */
        html_escape(ob, Jsi_DSValue(link)+7, Jsi_DSLength(link)-7);
    } else {
        html_escape(ob, Jsi_DSValue(link), Jsi_DSLength(link));
    }
    BLOB_APPEND_LITERAL(ob, "</a>");
    return 1;
}

static int html_code_span(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts) {
    BLOB_APPEND_LITERAL(ob, "<code>");
    html_escape(ob, Jsi_DSValue(text), Jsi_DSLength(text));
    BLOB_APPEND_LITERAL(ob, "</code>");
    return 1;
}

static int html_double_emphasis(
    Jsi_DString *ob,
    Jsi_DString *text,
    char c,
    jsi_MarkdownOpts *opts
) {
    BLOB_APPEND_LITERAL(ob, "<strong>");
    BLOB_APPEND_BLOB(ob, text);
    BLOB_APPEND_LITERAL(ob, "</strong>");
    return 1;
}

static int html_emphasis(
    Jsi_DString *ob,
    Jsi_DString *text,
    char c,
    jsi_MarkdownOpts *opts
) {
    BLOB_APPEND_LITERAL(ob, "<em>");
    BLOB_APPEND_BLOB(ob, text);
    BLOB_APPEND_LITERAL(ob, "</em>");
    return 1;
}

static int html_image(
    Jsi_DString *ob,
    Jsi_DString *link,
    Jsi_DString *title,
    Jsi_DString *alt,
    jsi_MarkdownOpts *opts
) {
    BLOB_APPEND_LITERAL(ob, "<img src=\"");
    html_escape(ob, Jsi_DSValue(link), Jsi_DSLength(link));
    BLOB_APPEND_LITERAL(ob, "\" alt=\"");
    html_escape(ob, Jsi_DSValue(alt), Jsi_DSLength(alt));
    if( title && Jsi_DSLength(title)>0 ) {
        BLOB_APPEND_LITERAL(ob, "\" title=\"");
        html_escape(ob, Jsi_DSValue(title), Jsi_DSLength(title));
    }
    BLOB_APPEND_LITERAL(ob, "\" />");
    return 1;
}

static int html_line_break(Jsi_DString *ob, jsi_MarkdownOpts *opts) {
    BLOB_APPEND_LITERAL(ob, "<br />\n");
    return 1;
}

static int html_link(
    Jsi_DString *ob,
    Jsi_DString *link,
    Jsi_DString *title,
    Jsi_DString *content,
    jsi_MarkdownOpts *opts
) {
    char *zLink = Jsi_DSValue(link);
    BLOB_APPEND_LITERAL(ob, "<a href=\"");
    if( zLink && zLink[0]=='/' && opts->topLink ) {
        /* For any hyperlink that begins with "/", make it refer to the root */
        Jsi_DSAppendLen(ob, opts->topLink, -1);
    }
    html_escape(ob, Jsi_DSValue(link), Jsi_DSLength(link));
    if( title && Jsi_DSLength(title)>0 ) {
        BLOB_APPEND_LITERAL(ob, "\" title=\"");
        html_escape(ob, Jsi_DSValue(title), Jsi_DSLength(title));
    }
    BLOB_APPEND_LITERAL(ob, "\">");
    BLOB_APPEND_BLOB(ob, content);
    BLOB_APPEND_LITERAL(ob, "</a>");
    return 1;
}

static int html_triple_emphasis(
    Jsi_DString *ob,
    Jsi_DString *text,
    char c,
    jsi_MarkdownOpts *opts
) {
    BLOB_APPEND_LITERAL(ob, "<strong><em>");
    BLOB_APPEND_BLOB(ob, text);
    BLOB_APPEND_LITERAL(ob, "</em></strong>");
    return 1;
}


static void html_normal_text(Jsi_DString *ob, Jsi_DString *text, jsi_MarkdownOpts *opts) {
    html_escape(ob, Jsi_DSValue(text), Jsi_DSLength(text));
}

/*
** Convert markdown into HTML.
**
** The document title is placed in output_title if not NULL.  Or if
** output_title is NULL, the document title appears in the body.
*/
void jsi_markdown_to_html(
    Jsi_DString *input_markdown,   /* Markdown content to be rendered */
    Jsi_DString *output_body,      /* Put document body here. */
    jsi_MarkdownOpts *opts
) {
    struct mkd_renderer html_renderer = {
        /* prolog and epilog */
        html_prolog,
        html_epilog,

        /* block level elements */
        html_blockcode,
        html_blockquote,
        html_raw_block,
        html_header,
        html_hrule,
        html_list,
        html_list_item,
        html_paragraph,
        html_table,
        html_table_cell,
        html_table_row,

        /* span level elements */
        html_autolink,
        html_code_span,
        html_double_emphasis,
        html_emphasis,
        html_image,
        html_line_break,
        html_link,
        html_raw_span,
        html_triple_emphasis,

        /* low level elements */
        0,  /* entities are copied verbatim */
        html_normal_text,

        /* misc. parameters */
        64, /* maximum stack */
        "*_", /* emphasis characters */
        opts /* opaque data */
    };
    Jsi_DSFree(output_body);
    mk_markdown(output_body, input_markdown, &html_renderer);
}

#endif
#endif //JSI_LITE_ONLY
