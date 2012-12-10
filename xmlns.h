#ifndef MOD_XMLNS_H
#define MOD_XMLNS_H

/* These defines are for the implementations.
 * Namespace providers should leave them undefined
 */
#ifndef XML_CHAR_T
typedef char            xml_char_t ;
#endif

#define XMLNS_VERSION 20060220

#include <util_filter.h>
#include <apr_optional.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xmlns_attr_t xmlns_attr_t ;

/** A context parameter passed to handlers
 *
 *  This is the public part of a bigger implementation-dependent struct.
 *  use accessor functions below, esp.
 *      xmlns_get_appdata
 *      xmlns_set_appdata
 */
typedef struct xmlns_public {
  ap_filter_t* f ;              /* f and bb are the current filter and  */
  apr_bucket_brigade* bb ;      /* brigade, for handlers to use in      */
                                /* ap_fputs, ap_fprintf, etc            */
} xmlns_public ;

/** ParsedName struct is a basic SAX2-equivalent.
 *
 * Note that the strings are NOT necessarily NULL-terminated - it is
 * implementation-dependent.   Always use the length parameters with them.
 */
typedef struct {
  int nparts ;                  /* number of fields defined in this struct:
                                        1: Only elt is defined
                                        2: elt and ns are defined
                                        3: elt, ns and prefix are defined
                                */
  const xml_char_t* ns ;        /* xmlns prefix for this namespace */
  size_t nslen ;                /* Length of ns */
  const xml_char_t* elt ;       /* Element name */
  size_t eltlen ;               /* Length of elt */
  const xml_char_t* prefix ;    /* Currently-defined prefix for namespace */
  size_t prefixlen ;            /* Length of prefix */
} parsedname ;

/** xmlns struct defines callbacks
 *
 * Those returning int may return OK to suppress default handler-absent
 * behaviour, or DECLINED to request default behaviour (print it).
 * Error codes may mean something in future.
 */
typedef struct xmlns {
  int version;

  int (*StartElement) ( xmlns_public*, const parsedname*, const xmlns_attr_t*) ;
  int (*EndElement) ( xmlns_public*, const parsedname*) ;
  void (*StartNamespace) ( xmlns_public*,
        const xml_char_t*, const xml_char_t* ) ;
  void (*EndNamespace) ( xmlns_public*, const xml_char_t* ) ;

/* Allow a comment handler - 'cos so many people put function in comments.
   The parser will dispatch to this comment handler if this prefix is
   non-null and the start of the comment matches this prefix.
*/
  const char* comment_prefix ;
  int (*CommentHandler) (xmlns_public*, const xml_char_t*) ;

  int (*CharactersHandler)(xmlns_public*, const xml_char_t*, int len) ;
  int (*CdataHandler)(xmlns_public*, const xml_char_t*, int len) ;

} xmlns ;

/********************************************************************
 *
 * Accessor Functions
 *
 * CHANGES FOR XMLNS2
 *
 * Use optional functions to dispense with the problem having more
 * than one xmlns implementation (mod_xmlns and mod_publisher) loaded.
 *
 * Dispense with appdata - modules can use request config instead.
 *
 * Replace enable/disable functions with provider-supplied callbacks
 * (can we do the same with suppress?)
 *
 ********************************************************************
 */

/* xmlns_parsename:
 * parses a name into a struct parsedname.
 * Handlers may call this to deal with namespaced attributes
 */
//extern void xmlns_parsename(const xml_char_t* name, parsedname* p) ;
APR_DECLARE_OPTIONAL_FN(void, mod_xmlns_parsename,
		(const xml_char_t* name, parsedname* p));
APR_DECLARE_OPTIONAL_FN(void, mod_publisher_parsename,
		(const xml_char_t* name, parsedname* p));

/** xmlns_suppress_output suppresses all output until it is turned off
 *  e.g. for SSI conditionals (<!--#if ...-->, etc).
 */
//extern void xmlns_suppress_output(xmlns_public*, int onoff) ;
APR_DECLARE_OPTIONAL_FN(void, mod_xmlns_suppress_output,
		(xmlns_public*, int onoff));
APR_DECLARE_OPTIONAL_FN(void, mod_publisher_suppress_output,
		(xmlns_public*, int onoff));

/** xmlns_get_attr_name and xmlns_get_attr_val are accessor functions
 *  for getting attribute names and values respectively in
 *  StartElement handlers.  When there are no more attributes,
 *  they return NULL.
 *
 *  Usage:
 *  for ( i = 0 ; ; ++i ) {
 *    name = xmlns_get_attr_name(attrs, i) ;
 *    if ( name == NULL )       // no more attributes
 *      break ;
 *    val = xmlns_get_attr_val(attrs, i) ;
 *    // Do something with name and val
 *  }
 */
//const xml_char_t* xmlns_get_attr_name(const xmlns_attr_t*, int) ;
//const xml_char_t* xmlns_get_attr_val(const xmlns_attr_t*, int) ;
APR_DECLARE_OPTIONAL_FN(const xml_char_t*, mod_xmlns_get_attr_name,
		(const xmlns_attr_t* attrs, int attnum));
APR_DECLARE_OPTIONAL_FN(const xml_char_t*, mod_xmlns_get_attr_val,
		(const xmlns_attr_t* attrs, int attnum));
APR_DECLARE_OPTIONAL_FN(const xml_char_t*, mod_publisher_get_attr_name,
		(const xmlns_attr_t* attrs, int attnum));
APR_DECLARE_OPTIONAL_FN(const xml_char_t*, mod_publisher_get_attr_val,
		(const xmlns_attr_t* attrs, int attnum));

/** xmlns_get_attr_parsed is an alternative to xmlns_get_attr_name that
 *  parses the attribute into a parsedname instead of returning it as-is.
 *  Return value is 1 if an attribute is parsed; 0 if there are no more
 *  attributes, so it works in a loop in the same form as above.
 */
//int xmlns_get_attr_parsed(const xmlns_attr_t*, int, parsedname*) ;
APR_DECLARE_OPTIONAL_FN(int, mod_xmlns_get_attr_parsed,
		(const xmlns_attr_t* attrs, int attnum, parsedname* res));
APR_DECLARE_OPTIONAL_FN(int, mod_publisher_get_attr_parsed,
		(const xmlns_attr_t* attrs, int attnum, parsedname* res));


#ifdef __cplusplus
}
#endif

#endif

