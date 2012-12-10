/*
        Copyright (c) 2003-4, WebThing Ltd
        Author: Nick Kew <nick@webthing.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

/*
        mod_xmlns
        Adds XML Namespace processing to the Apache Webserver
        http://apache.webthing.com/mod_xmlns/

        This module doesn't directly do anything.  It's a harness
        for plugging in XMLNS processors.  A processor for XHTML
        ( xmlns="http://www.w3.org/1999/xhtml") is bundled,
        and others are available from WebThing - see the webpage
        for details.  You can also develop new processors to handle
        other namespaces or provide alternative behaviour - see below.
*/

/*      Note to Users

        You are requested to register as a user, at
        http://apache.webthing.com/registration.html

        This entitles you to support from the developer.
        I'm unlikely to reply to help/support requests from
        non-registered users, unless you're paying and/or offering
        constructive feedback such as bug reports or sensible
        suggestions for further development.

        It also makes a small contribution to the effort
        that's gone into developing this work.
*/

/*      Note to Developers and Distributors

        You are encouraged to develop namespace processors to work
        with this module.  Please let me know if you do!
        A good starting-point would be to look at the simple
        processor for XHTML bundled with this module.

*/

/*      This is one of the SAX Parser family of output filter
        modules from http://apache.webthing.com/

SAX Filter modules for XML (and XHTML):
        mod_xmlns:              Free, pre-stable
                                Simple harness for XML Namespace processing

SAX Filter modules for HTML (and XHTML):
        mod_accessibility:      Commercial
                                HTML cleanup and correction, data discovery,
                                user empowerment and presentation options.

        mod_proxy_html:         Free, stable
                                Rewriting of HTML links, specifically
                                for use in a reverse proxy.

Universal SAX Filter module
        mod_publisher:          Work in progress
                                Extensive HTML manipulation: templating,
                                substitutions, variables, SSI.
                                DTD validation of outgoing markup.
                                XML Namespace and XSLT support

Namespace Modules
        mod_xhtml               ensure XHTML conforms to Appendix C
                                for compatibility with HTML browsers.
        mod_esi                 Parser for Edge Side Includes
        mod_annot               Work in progress.
                                Manage blog/wiki-style annotations
                                (as margin-notes in XHTML)

Other markup processing modules for Apache from WebThing:
        XSLT output filter              (free, stable)
        SAX and DOM APIs                (free, experimental)
        XMTP and XMLRPC filter          (free, experimental)
        XML and HTML Validation         (free, stable)
        HTML Accessibility Analysis     (commercial)
*/

#include <ctype.h>

/* apache */
#include <http_protocol.h>
#include <http_config.h>
#include <http_log.h>
#include <apr_strings.h>
#include <util_filter.h>
#include <apr_hash.h>
#include <ap_provider.h>

module AP_MODULE_DECLARE_DATA xmlns_module ;

#include <expat.h>

#define MOD_XMLNS_EXPAT
#include "xmlns.h"

/* convenience defines */

#define CTX ((xmlns_ctx*)ctx)

#define F ((xmlns_ctx*)ctx)->public->f->next
#define BB ((xmlns_ctx*)ctx)->public->bb

#define SUPPRESS_COMMENTS       0x01
#define SUPPRESS_ALL            0x02

#define COMMENT_HANDLERS 0x01
#define COMMENT_SET_HANDLERS 0x10
#define COMMENT_PRESERVE 0x02
#define COMMENT_SET_PRESERVE 0x20

typedef struct xmlns_cfg {
  apr_hash_t* namespaces ;
  unsigned int comments ;
  const char* defaultns ;
  int use_tagpath;
} xmlns_cfg ;

typedef struct xmlns_rec {
  int onoff ;
  xmlns* handler ;
} xmlns_rec ;

typedef struct xmlns_active {
  const char* url ;
  struct xmlns_active* next ;
  xmlns* handler ;
  int newns ;
  const char* prefix ;
} xmlns_active ;

/*
typedef struct {
  ap_filter_t* f ;
  apr_bucket_brigade* bb ;
} xmlns_private ;
*/

typedef struct xmlns_ctx {
  xmlns_active* activens ;
  XML_Parser parser ;
  xmlns_public* public ;
  unsigned int suppressed ;
  const char* defaultns;
  int CdataSize;
  int CdataAlloc;
  char* CdataBuf;
  apr_array_header_t* tagpath;
} xmlns_ctx ;

static xmlns_active* lookup_ns(xmlns_ctx* ctx, const parsedname* name3) {
  xmlns_active* p = NULL ;
  for ( p = ctx->activens ; p ; p = p->next )
    if ( name3->nparts >= 2 )
      if ( !strncmp(p->url, name3->ns, name3->nslen) )
        break ;
      else
        continue ;
    else
      if ( ctx->defaultns && !strcmp(p->url, ctx->defaultns) )
        break ;
      else
        continue ;
  return p ;
}
#define PREFIX (prefix?prefix:NULLPREFIX)
static xmlns_active* lookup_prefix(xmlns_ctx* ctx,
        const char* prefix, const char* uri) {

  static const char* NULLPREFIX = "" ;
  xmlns_active* p ;
  for ( p = ctx->activens ; p ; p = p->next )
    if ( uri ) {
      if ( !strcmp(p->url, uri) ) {
        p->prefix = apr_pstrdup(ctx->public->f->r->pool, PREFIX) ;
        break ;
      }
    } else {
      if ( p->prefix && !strcmp(p->prefix, PREFIX) ) {
        p->prefix = NULL ;
        break ;
      }
    }
  return p ;
}
static void mod_xmlns_parsename(const xml_char_t* name, parsedname* p) {
  char* sp = strchr(name, ' ') ;
  if ( sp ) {
    p->ns = name ;
    p->nslen = ( sp - name ) ;
    p->elt = name + p->nslen + 1 ;
    sp = strchr(p->elt, ' ') ;
    if ( sp ) {
      p->eltlen = ( sp - p->elt ) ;
      p->prefix = p->elt + p->eltlen + 1 ;
      p->prefixlen = strlen(p->prefix) ;
      p->nparts = 3 ;
    } else {
      p->eltlen = strlen(p->elt) ;
      p->prefix = (void*) ( p->prefixlen = 0 ) ;
      p->nparts = 2 ;
    }
  } else {
    p->elt = name ;
    p->eltlen = strlen(name) ;
    p->prefix = p->ns = (void*) ( p->prefixlen = p->nslen = 0 ) ;
    p->nparts = 1 ;
  }
}
static void xstartElement(void* ctx, const XML_Char* name,
                const XML_Char** atts) {
  parsedname name3 ;
  xmlns_active* ns ;
  xmlns** elt;
  mod_xmlns_parsename(name, &name3) ;

  ns = lookup_ns((xmlns_ctx*)ctx, &name3) ;
  if (ns && ns->handler) {
    if (CTX->tagpath != NULL) {
      elt = apr_array_push(CTX->tagpath);
      *elt = ns->handler;
    }

    if ( ns->handler->StartElement ) {
      if ( ns->handler->StartElement((xmlns_public*)CTX->public, &name3,
          (const xmlns_attr_t*)atts) != DECLINED )
        return  ;
    }
  }

/* Default: either no handler, or it returned 0 */
  ap_fputc(F, BB, '<') ;
  if ( name3.nparts == 3 ) {
    ap_fwrite(F, BB, name3.prefix, name3.prefixlen) ;
    ap_fputc(F, BB, ':') ;
  }
  ap_fwrite(F, BB, name3.elt, name3.eltlen) ;

  if ( ns && ns->newns ) {
    if ( name3.nparts == 3 ) {
      ap_fputs(F, BB, " xmlns:") ;
      ap_fwrite(F, BB, name3.prefix, name3.prefixlen) ;
      ap_fputs(F, BB, "=\"") ;
      ap_fwrite(F, BB, name3.ns, name3.nslen) ;
      ap_fputc(F, BB, '"') ;
    } else if ( name3.nparts == 2 ) {
      ap_fputs(F, BB, " xmlns=\"") ;
      ap_fwrite(F, BB, name3.ns, name3.nslen) ;
      ap_fputc(F, BB, '"') ;
    }
    ns->newns = 0 ;
  }
  if ( atts ) {
    const XML_Char** a ;
    for ( a = atts ; *a ; a += 2 ) {
      parsedname a3 ;
      mod_xmlns_parsename(*a, &a3) ;
      switch ( a3.nparts ) {
        case 1:
          ap_fputstrs(F, BB, " ", a[0], "=\"", a[1], "\"", NULL) ;
          break ;
        case 2:
          ap_fputc(F, BB, ' ') ;
          ap_fwrite(F, BB, a3.ns, a3.nslen) ;
          ap_fputc(F, BB, ':') ;
          ap_fwrite(F, BB, a3.elt, a3.eltlen) ;
          ap_fputstrs(F, BB, "=\"", a[1], "\"", NULL) ;
          break ;
        case 3:
          ap_fputc(F, BB, ' ') ;
          ap_fwrite(F, BB, a3.prefix, a3.prefixlen) ;
          ap_fputc(F, BB, ':') ;
          ap_fwrite(F, BB, a3.elt, a3.eltlen) ;
          ap_fputstrs(F, BB, "=\"", a[1], "\"", NULL) ;
          break ;
      }
    }
  }
  ap_fputc(F, BB, '>') ;
}
static void xendElement(void* ctx, const XML_Char* name) {
  parsedname name3 ;
  xmlns_active* ns ;
  mod_xmlns_parsename(name, &name3) ;

  ns = lookup_ns((xmlns_ctx*)ctx, &name3) ;
  if ( ns && ns->handler ) {
    if (CTX->tagpath != NULL)
      apr_array_pop(CTX->tagpath) ;
    if ( ns->handler->EndElement ) {
      if ( ns->handler->EndElement(CTX->public, &name3) != DECLINED )
        return  ;
    }
  }
  ap_fputs(F, BB, "</") ;
  if ( name3.nparts == 3 ) {
    ap_fwrite(F, BB, name3.prefix, name3.prefixlen) ;
    ap_fputc(F, BB, ':') ;
  }
  ap_fwrite(F, BB, name3.elt, name3.eltlen) ;

  ap_fputc(F, BB, '>') ;
}

static void xstartNamespaceDecl(void* ctx,
        const XML_Char *prefix, const XML_Char *uri) {
  xmlns_active* ns ;

  ns = lookup_prefix((xmlns_ctx*)ctx, prefix, uri) ;
  if ( ns ) {
    if ( ns->handler && ns->handler->StartNamespace )
      ns->handler->StartNamespace((xmlns_public*)CTX->public, prefix, uri) ;
    ns->newns = 1 ;
  }
}
static void xendNamespaceDecl(void* ctx, const XML_Char *prefix) {
  xmlns_active* ns ;

  ns = lookup_prefix((xmlns_ctx*)ctx, prefix, NULL) ;
  if ( ns && ns->handler->EndNamespace )
    ns->handler->EndNamespace((xmlns_public*)CTX->public, prefix) ;
}

static void xComment(void* ctx, const XML_Char* buf) {
  xmlns_cfg* cfg = ap_get_module_config(F->r->per_dir_config, &xmlns_module) ;
  xmlns_active* p = CTX->activens ;
  if ( cfg->comments & COMMENT_HANDLERS )
    for ( p = CTX->activens ; p ; p = p->next )
      if ( p->handler && p->handler->comment_prefix ) {
        if ( !strncmp(p->handler->comment_prefix, buf,
                strlen(p->handler->comment_prefix) ) ) {
          /* this is the comment handler we wanted */
          if ( p->handler->CommentHandler ) {
            if ( p->handler->CommentHandler((xmlns_public*)CTX->public, buf) !=
 DECLINED )
              return ;
          }
        }
      }
  if ( cfg->comments & COMMENT_PRESERVE )
    if ( ! (CTX->suppressed & SUPPRESS_COMMENTS) )
      ap_fputstrs(F, BB, "<!--", buf, "-->", NULL) ;
}
static void xXmlDecl(void* ctx,
        const XML_Char *version, const XML_Char *encoding, int standalone) {
  if ( ! version )
    return ;
  ap_fputstrs(F, BB, "<?xml version=\"", version,
        "\" encoding=\"utf-8\"", NULL) ;
  if ( standalone != -1 ) {
    ap_fputstrs(F, BB, " standalone=\"", (standalone ? "yes" : "no"),
        "\"", NULL) ;
  }
  ap_fputs(F, BB, "?>") ;
}

/**
 *Character Handler to be called from the SAX parser
 * @param ctx The context that holds the list of active namespaces
 * @param buf The character data
 * @param len Its length
 */
#define FLUSH ap_fwrite(F, BB, (buf+begin), (i-begin)) ; begin = i+1
static void xCharacters(void* ctx, const XML_Char* buf, int len) {
  const apr_array_header_t *tp = CTX->tagpath ;
  const xmlns** ns = (const xmlns**)tp->elts;
  const xmlns *h	       = NULL;
  int i, begin ;

  /* Walk down the tagpath */
  for (i=tp->nelts-1; i>=0; i--) {
    h = ns[i] ;
    if (h->CharactersHandler) {
      if (h->CharactersHandler((xmlns_public*)CTX->public,
			      buf, len) != DECLINED ) {
	return ;
      }
    }
  }

  /* All active handlers DECLINED, so default is copy through.
   * But this needs escaping.
   */ 
  for ( begin=i=0; i<len; i++ ) {
    switch (buf[i]) {
      case '&' : FLUSH ; ap_fputs(F, BB, "&amp;") ; break ;
      case '<' : FLUSH ; ap_fputs(F, BB, "&lt;") ; break ;
      case '>' : FLUSH ; ap_fputs(F, BB, "&gt;") ; break ;
      case '"' : FLUSH ; ap_fputs(F, BB, "&quot;") ; break ;
      default : break ;
    }
  }
  FLUSH ;
}
#define CDATABUFSIZE 8000
static void xreserve(xmlns_ctx* ctx, int len) {
  char* newbuf ;
  if ( len <= ( ctx->CdataAlloc - ctx->CdataSize ) )
    return ;
  else while ( len > ( ctx->CdataAlloc - ctx->CdataSize ) )
    ctx->CdataAlloc += CDATABUFSIZE;

  newbuf = realloc(ctx->CdataBuf, ctx->CdataAlloc) ;
  if ( newbuf != ctx->CdataBuf ) {
    if ( ctx->CdataBuf )
      apr_pool_cleanup_kill(ctx->public->f->r->pool, ctx->CdataBuf, (void*)free) ;
    apr_pool_cleanup_register(ctx->public->f->r->pool, newbuf,
		 (void*)free, apr_pool_cleanup_null);
    ctx->CdataBuf = newbuf ;
  }
}
static void xStartCdata(void* ctx) {
  const apr_array_header_t *tp = CTX->tagpath ;
  const xmlns** ns = (const xmlns**)tp->elts;
  const xmlns *h	       = NULL;
  int i ;
  /* Walk down the tagpath - if there are no Cdata handlers then
   * leave everything for the default handler
   */
  for (i=tp->nelts-1; i>=0; i--) {
    h = ns[i] ;
    if (h->CdataHandler) {
      CTX->CdataSize = 0 ; /* flags default handler we're in CDATA */
      break ;
    }
  }
}
static void xEndCdata(void* ctx) {
  const apr_array_header_t *tp = CTX->tagpath ;
  const xmlns** ns = (const xmlns**)tp->elts;
  const xmlns *h	       = NULL;
  int i ;
  int handled = 0 ;

  /* Walk down the tagpath */
  for (i=tp->nelts-1; i>=0; i--) {
    h = ns[i];
    if (h->CdataHandler) {
      if (h->CdataHandler((xmlns_public*)CTX->public,
		      CTX->CdataBuf, CTX->CdataSize) != DECLINED ) {
	handled = 1 ;
	break ;
      }
    }
  }
  if ( !handled ) {
    /* All active handlers DECLINED, so default is copy through */ 
    ap_fwrite(F, BB, CTX->CdataBuf, CTX->CdataSize) ;	
  }
  CTX->CdataSize = -1;
}
static void xdefault(void* ctx, const XML_Char* buf, int len) {
  if (CTX->CdataSize == -1) {	/* not in CData */
    ap_fwrite(F, BB, buf, len) ;  /*      escape not required     */
  } else {
    xreserve(CTX, len);
    memcpy(CTX->CdataBuf + CTX->CdataSize, buf, len);
    CTX->CdataSize += len;
  }
}

static char* ctype2encoding(apr_pool_t* pool, const char* in) {
  char* x ;
  char* ptr ;
  char* ctype ;
  if ( ! in )
    return 0 ;
  ctype = apr_pstrdup(pool, in) ;
  for ( ptr = ctype ; *ptr; ++ptr)
    if ( isupper(*ptr) )
      *ptr = tolower(*ptr) ;

  if ( ptr = strstr(ctype, "charset=") , ptr > 0 ) {
    ptr += 8 ;  /* jump over "charset=" and chop anything that follows charset*/
    if ( x = strpbrk(ptr, " ;") , x != NULL )
      *x = 0 ;
  }
  x =  ptr ? apr_pstrdup(pool, ptr) : 0 ;
  return x ;
}
static apr_status_t onoff_filter(ap_filter_t* f, apr_bucket_brigade* bb) {
  return ( ((xmlns_ctx*)f->ctx)->suppressed & SUPPRESS_ALL )
        ? APR_SUCCESS
        : ap_pass_brigade(f->next, bb) ;
}
static ap_filter_rec_t fonoff ;
static void xmlns_child_init(apr_pool_t* p, server_rec* s) {
  /** use child_init with memset and sizeof to work with both 2.0
   *  and 2.1+ versions of ap_filter_rec_t without #ifdefs
   */
  memset(&fonoff, 0, sizeof(ap_filter_rec_t)) ;
  fonoff.name = "fonoff" ;
  fonoff.filter_func.out_func = onoff_filter ;
  fonoff.ftype = AP_FTYPE_RESOURCE ;
}
#define CALLBACK(fn,handler) fn ( fctx->parser , handler )
static int xmlns_filter_init(ap_filter_t* f) {
  xmlns_ctx* fctx ;
  void* ptr ;
  xmlns_cfg* cfg = ap_get_module_config(f->r->per_dir_config, &xmlns_module) ;

  char* enc = ctype2encoding(f->r->pool, f->r->content_type) ;

  ap_filter_t* fnext = apr_palloc(f->c->pool, sizeof(ap_filter_t) ) ;

/* I don't think this is necesessary anymore, and ElDiablo wants it out */
#if 0
/* remove content-length filter */
  ap_filter_rec_t* clf = ap_get_output_filter_handle("CONTENT_LENGTH") ;
  ap_filter_t* ff = f->next ;
  do {
    ap_filter_t* fnext = ff->next ;
    if ( ff->frec == clf )
      ap_remove_output_filter(ff) ;
    ff = fnext ;
  } while ( ff ) ;
#endif
  apr_table_unset(f->r->headers_out, "Content-Length") ;

  fctx = f->ctx = apr_pcalloc(f->r->pool, sizeof(xmlns_ctx)) ;
  fctx->CdataSize = -1;
  fctx->public = apr_palloc(f->r->pool, sizeof(xmlns_public)) ;
  fctx->public->f = f ;
  fctx->public->bb
        = apr_brigade_create(f->r->pool, f->r->connection->bucket_alloc) ;
  fctx->defaultns = cfg->defaultns ;

  fnext->frec = &fonoff ;
  fnext->ctx = fctx ;
  fnext->next = f->next ;
  fnext->r = f->r ;
  fnext->c = f->c ;
  f->next = fnext ;

#if 0
/* chunked encoding enables HTTP keepalive */
  if ( f->r->proto_num >= 1001 ) {
    if ( ! f->r->main && ! f->r->prev )
      f->r->chunked = 1 ;
  }
#endif

/* set up the parser */
  fctx->parser = XML_ParserCreateNS(enc, ' ') ;
  apr_pool_cleanup_register(f->r->pool, fctx->parser,
        (void*)XML_ParserFree, apr_pool_cleanup_null) ;

/* Option for default to complain ?  */
  CALLBACK(XML_SetDefaultHandler, xdefault) ;

/* Comment */
  CALLBACK(XML_SetCommentHandler, xComment) ;

/* LoadNamespace mech for replacing these */
  CALLBACK(XML_SetStartElementHandler, xstartElement) ;
  CALLBACK(XML_SetEndElementHandler, xendElement) ;
  CALLBACK(XML_SetStartNamespaceDeclHandler, xstartNamespaceDecl) ;
  CALLBACK(XML_SetEndNamespaceDeclHandler, xendNamespaceDecl) ;

  CALLBACK(XML_SetXmlDeclHandler, xXmlDecl) ;

  if (cfg->use_tagpath == 1) {
    fctx->tagpath = apr_array_make(f->r->pool, 12, sizeof(xmlns*)) ;
    CALLBACK(XML_SetCharacterDataHandler, xCharacters);
    CALLBACK(XML_SetStartCdataSectionHandler, xStartCdata);
    CALLBACK(XML_SetEndCdataSectionHandler, xEndCdata);
  }

  XML_SetReturnNSTriplet(fctx->parser, 1) ;
  XML_SetUserData(fctx->parser, fctx) ;

/* set active namespace handlers */
  for ( ptr = apr_hash_first(f->r->pool, cfg->namespaces ) ;
        ptr ; ptr = apr_hash_next(ptr) ) {
    xmlns_rec* rec ;
    const void* url ;
    apr_ssize_t len ;
    apr_hash_this(ptr, &url, &len, (void**)&rec) ;
    if ( rec->onoff ) {
      xmlns_active* ns = apr_pcalloc(f->r->pool, sizeof(xmlns_active)) ;
      ns->next = fctx->activens ;
      fctx->activens = ns ;
      ns->url = url ;
      ns->handler = rec->handler ;
    }
  }
  return OK ;
}
static int xmlns_filter(ap_filter_t* f, apr_bucket_brigade* bb) {
  apr_bucket* b ;
  const char* buf = 0 ;
  apr_size_t bytes = 0 ;

  xmlns_ctx* ctxt = (xmlns_ctx*)f->ctx ;
  if ( ! ctxt ) {
    xmlns_filter_init(f) ;
  }
  if ( ctxt = (xmlns_ctx*)f->ctx , ! ctxt )
    return ap_pass_brigade(f->next, bb) ;

  for ( b = APR_BRIGADE_FIRST(bb) ;
        b != APR_BRIGADE_SENTINEL(bb) ;
        b = APR_BUCKET_NEXT(b) ) {
    if ( APR_BUCKET_IS_EOS(b) ) {
      if ( XML_Parse(ctxt->parser, buf, 0, 1) != XML_STATUS_OK ) {
        enum XML_Error err = XML_GetErrorCode(ctxt->parser) ;
        const XML_LChar* msg = XML_ErrorString(err) ;
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r, "Endparse Error %d: %s",
                err, msg) ;
      }
      APR_BRIGADE_INSERT_TAIL(ctxt->public->bb,
        apr_bucket_eos_create(ctxt->public->bb->bucket_alloc) ) ;
      ap_pass_brigade(ctxt->public->f->next, ctxt->public->bb) ;
    } else if ( APR_BUCKET_IS_FLUSH(b) ) {
      APR_BRIGADE_INSERT_TAIL(ctxt->public->bb,
        apr_bucket_flush_create(ctxt->public->bb->bucket_alloc) ) ;
    } else if ( apr_bucket_read(b, &buf, &bytes, APR_BLOCK_READ)
              == APR_SUCCESS ) {
      if ( XML_Parse(ctxt->parser, buf, bytes, 0) != XML_STATUS_OK ) {
        enum XML_Error err = XML_GetErrorCode(ctxt->parser) ;
        const XML_LChar* msg = XML_ErrorString(err) ;
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r, "Parse Error %d: %s",
                err, msg) ;
      }
    } else {
      ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, f->r, "Error in bucket read") ;
    }
  }
/*  apr_brigade_cleanup(bb) ;*/
  return APR_SUCCESS ;
}
static const char* use_namespace(cmd_parms* cmd, void* cfg,
        const char* uri, const char* action, const char* version) {
  xmlns_rec* rec ;
  void* handler ;
  int onoff ;
  if ( ! version )
    version = "default" ;
  handler = ap_lookup_provider("xmlns", uri, version) ;
  if ( ! handler ) {
    /* insist on a version and exit if not found */
    return apr_pstrcat(cmd->pool,
		       "Can't use namespace ",
		       uri,", ",version,
		       ": not loaded or incompatible version",
		       NULL);
  }
  if ( ! action || !strcasecmp(action, "on") ) {
    onoff = 1 ;
  } else if ( !strcasecmp(action, "force") ) {
    onoff = 2 ;
  } else if ( !strcasecmp(action, "off") ) {
    onoff = 0 ;
  } else {
    return "Action must be On, Off or Force" ;
  }
  if (((xmlns*)handler)->version != XMLNS_VERSION) {
    switch (onoff) {
    case 0:
      break;
    case 1:
      ap_log_perror(APLOG_MARK, APLOG_NOTICE|APLOG_STARTUP, 0, cmd->pool,
	"Namespace handler %s (version %s) is compiled to a different "
	"API version (%d) to mod_xmlns (%d) - ignoring.  Use \"force\" "
	"to use it anyway, if you're satisfied it's compatible.",
	uri, version, ((xmlns*)handler)->version, XMLNS_VERSION) ;
      break;
    case 2:
      ap_log_perror(APLOG_MARK, APLOG_NOTICE|APLOG_STARTUP, 0, cmd->pool,
	"Namespace handler %s (version %s) is compiled to a different "
	"API version (%d) to mod_xmlns (%d).  This may cause apache to "
	"crash.", uri, version, ((xmlns*)handler)->version, XMLNS_VERSION) ;
      break;
    }
  }
  rec = apr_hash_get(((xmlns_cfg*)cfg)->namespaces,uri,APR_HASH_KEY_STRING) ;
  if ( ! rec ) {
    rec = apr_palloc(cmd->pool, sizeof(xmlns_rec) ) ;
    rec->handler = handler ;
    apr_hash_set(((xmlns_cfg*)cfg)->namespaces,uri,APR_HASH_KEY_STRING, rec) ;
  }
  rec->onoff = onoff ;
  return NULL ;
}
static const char* comments(cmd_parms* cmd, void* cfg, const char* arg) {
  int onoff ;
  unsigned int which = (int) cmd->info ;
  if ( !strcasecmp(arg, "on") )
    onoff = 1 ;
  else if ( !strcasecmp(arg, "off") )
    onoff = 0 ;
  else
    return "Syntax error: values are On or Off" ;

  ((xmlns_cfg*)cfg)->comments |= which ;

  switch ( which ) {
    case COMMENT_SET_HANDLERS:
      if ( onoff == 0 )
        ((xmlns_cfg*)cfg)->comments ^= COMMENT_HANDLERS ;
      else
        ((xmlns_cfg*)cfg)->comments |= COMMENT_HANDLERS ;
      break ;
    case COMMENT_SET_PRESERVE:
      if ( onoff == 1 )
        ((xmlns_cfg*)cfg)->comments ^= COMMENT_PRESERVE ;
      else
        ((xmlns_cfg*)cfg)->comments |= COMMENT_PRESERVE ;
      break ;
  }
  return NULL ;
}

static const command_rec xmlns_cmds[] = {
  AP_INIT_TAKE123("XMLNSUseNamespace", use_namespace, NULL, OR_ALL, NULL) ,
  AP_INIT_TAKE1("XMLNSDefaultNamespace", ap_set_string_slot,
        (void*)APR_OFFSETOF(xmlns_cfg, defaultns), OR_ALL, NULL) ,
  AP_INIT_TAKE1("XMLNSCommentHandlers", comments,
        (void*)COMMENT_SET_HANDLERS, OR_ALL, NULL) ,
  AP_INIT_TAKE1("XMLNSCommentRemove", comments,
        (void*)COMMENT_SET_PRESERVE, OR_ALL, NULL) ,
  AP_INIT_FLAG("XMLNSEnableDataHandlers", ap_set_flag_slot,
        (void*)APR_OFFSETOF(xmlns_cfg, use_tagpath), OR_ALL, NULL) ,
  { NULL }
} ;

static void* cr_xmlns_cfg(apr_pool_t* pool, char* x) {
  xmlns_cfg* cfg = apr_pcalloc(pool, sizeof(xmlns_cfg) ) ;
  cfg->namespaces = apr_hash_make(pool) ;
  cfg->comments = COMMENT_PRESERVE | COMMENT_HANDLERS ;
  cfg->use_tagpath = -1;
  return cfg ;
}
static void* merge_xmlns_cfg(apr_pool_t* pool, void* BASE, void* ADD) {
  xmlns_cfg* base = BASE ;
  xmlns_cfg* add = ADD ;
  xmlns_cfg* cfg = apr_palloc(pool, sizeof(xmlns_cfg) ) ;
  cfg->namespaces = apr_hash_overlay(pool, add->namespaces, base->namespaces) ;

  cfg->comments = 0 ;
  if ( add->comments & COMMENT_SET_PRESERVE )
    cfg->comments |=
        ( add->comments & ( COMMENT_SET_PRESERVE | COMMENT_PRESERVE ) ) ;
  else if ( base->comments & COMMENT_SET_PRESERVE )
    cfg->comments |=
        ( base->comments & ( COMMENT_SET_PRESERVE | COMMENT_PRESERVE ) ) ;
  else
    cfg->comments |= COMMENT_PRESERVE ; /* default */

  if ( add->comments & COMMENT_SET_HANDLERS )
    cfg->comments |=
        ( add->comments & ( COMMENT_SET_HANDLERS | COMMENT_HANDLERS ) ) ;
  else if ( base->comments & COMMENT_SET_HANDLERS )
    cfg->comments |=
        ( base->comments & ( COMMENT_SET_HANDLERS | COMMENT_HANDLERS ) ) ;
  else
    cfg->comments |= COMMENT_HANDLERS ; /* default */

  cfg->defaultns = add->defaultns ? add->defaultns : base->defaultns ;

  cfg->use_tagpath = (add->use_tagpath == -1) ? base->use_tagpath : add->use_tagpath ;

  return cfg ;
}


#if 0
static void xmlns_enable_comment(xmlns_public* ctx, int onoff) {
  xmlns_ctx* xctx = ctx->f->ctx ;
  if ( onoff )
    xctx->suppressed ^= SUPPRESS_COMMENTS ;     /* default */
  else
    xctx->suppressed |= SUPPRESS_COMMENTS ;
}
#endif
static void mod_xmlns_suppress_output(xmlns_public* ctx, int onoff) {
  xmlns_ctx* xctx = ctx->f->ctx ;
  ap_pass_brigade(ctx->f->next, ctx->bb) ;
  apr_brigade_cleanup(ctx->bb) ;
  if ( onoff )
    xctx->suppressed |= SUPPRESS_ALL ;  /* default */
  else
    xctx->suppressed ^= SUPPRESS_ALL ;
}
static const xml_char_t* mod_xmlns_get_attr_name(const xmlns_attr_t* atts, int n) {
  return ((XML_Char**)atts)[2*n] ;
}
static const xml_char_t* mod_xmlns_get_attr_val(const xmlns_attr_t* atts, int n) {
  return ((XML_Char**)atts)[2*n + 1] ;
}
static int mod_xmlns_get_attr_parsed(const xmlns_attr_t* atts, int n, parsedname* ret) {
  const xml_char_t* name = mod_xmlns_get_attr_name(atts, n) ;
  if ( ! name )
    return 0 ;
  mod_xmlns_parsename(name, ret) ;
  return 1 ;
}

static void xmlns_hooks(apr_pool_t* p) {
  ap_hook_child_init(xmlns_child_init, NULL, NULL, APR_HOOK_MIDDLE) ;
  ap_register_output_filter("xmlns", xmlns_filter,
        NULL, AP_FTYPE_RESOURCE) ;
  APR_REGISTER_OPTIONAL_FN(mod_xmlns_parsename);
  APR_REGISTER_OPTIONAL_FN(mod_xmlns_suppress_output);
  APR_REGISTER_OPTIONAL_FN(mod_xmlns_get_attr_name);
  APR_REGISTER_OPTIONAL_FN(mod_xmlns_get_attr_val);
  APR_REGISTER_OPTIONAL_FN(mod_xmlns_get_attr_parsed);
}

module AP_MODULE_DECLARE_DATA xmlns_module = {
        STANDARD20_MODULE_STUFF,
        cr_xmlns_cfg,
        merge_xmlns_cfg,
        NULL,
        NULL,
        xmlns_cmds,
        xmlns_hooks
} ;
