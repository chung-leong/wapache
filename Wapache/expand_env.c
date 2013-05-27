#include "apr.h"
#include "apr_strings.h"
#include "apr_tables.h"

/*

	Chung: Code stolen from mod_rewrite and like totally butchered 	

*/

/*
** +-------------------------------------------------------+
** |                                                       |
** |             environment variable support
** |                                                       |
** +-------------------------------------------------------+
*/

static char *lookup_variable(apr_pool_t * pool, apr_table_t *env, char *var)
{
    const char *result;

    result = NULL;

    /* all other env-variables from the parent Apache process */
    if (strlen(var) > 4 && strncasecmp(var, "ENV:", 4) == 0) {
        /* first try the env array */
        result = apr_table_get(env, var+4);
        /* second try the external OS env */
        if (result == NULL) {
            result = getenv(var+4);
        }
    }

    if (result == NULL) {
        return apr_pstrdup(pool, "");
    }
    else {
        return apr_pstrdup(pool, result);
    }
}


/*
**
**  Bracketed expression handling
**  s points after the opening bracket
**
*/

static char *find_closing_bracket(char *s, int left, int right)
{
    int depth;

    for (depth = 1; *s; ++s) {
        if (*s == right && --depth == 0) {
            return s;
        }
        else if (*s == left) {
            ++depth;
        }
    }
    return NULL;
}


/*
**
**  perform all the expansions on the input string
**  leaving the result in the supplied buffer
**
*/


void do_expand(apr_pool_t *pool, apr_table_t *env, char *input, char *buffer, int nbuf)
{
    char *inp, *outp;
    apr_size_t span, space;

    /*
     * for security reasons this expansion must be performed in a
     * single pass, otherwise an attacker can arrange for the result
     * of an earlier expansion to include expansion specifiers that
     * are interpreted by a later expansion, producing results that
     * were not intended by the administrator.
     */

    inp = input;
    outp = buffer;
    space = nbuf - 1; /* room for '\0' */

    for (;;) {
        span = strcspn(inp, "\\%");
        if (span > space) {
            span = space;
        }
        memcpy(outp, inp, span);
        inp += span;
        outp += span;
        space -= span;
        if (space == 0 || *inp == '\0') {
            break;
        }
        /* now we have a '\' or '%' */
        if (inp[0] == '\\') {
            if (inp[1] != '\0') {
                inp++;
                goto skip;
            }
        }
        else if (inp[1] == '{') {
            char *endp;
            endp = find_closing_bracket(inp+2, '{', '}');
            if (endp == NULL) {
                goto skip;
            }
            if (inp[0] == '%') {
                /* %{...} variable lookup expansion */
                char *var;
                var  = apr_pstrndup(pool, inp+2, endp-inp-2);
                span = apr_cpystrn(outp, lookup_variable(pool, env, var), space) - outp;
            }
            else {
                span = 0;
            }
            inp = endp+1;
            outp += span;
            space -= span;
            continue;
        }
        skip:
        *outp++ = *inp++;
        space--;
    }
    *outp++ = '\0';
}