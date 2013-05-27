#include "stdafx.h"
#include "WapacheApplication.h"
#include "WapacheProtocol.h"

static const char *set_server_root(cmd_parms *cmd, void *dummy,
                                   const char *arg)
{
    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);

    if (err != NULL) {
        return err;
    }
	
	if ((apr_filepath_merge((char**)&ap_server_root, Application.Process->bin_root, arg,
                            APR_FILEPATH_TRUENAME, cmd->pool) != APR_SUCCESS)
        || !ap_is_directory(cmd->pool, ap_server_root)) {
        return "ServerRoot must be a valid directory";
    }

	chdir(ap_server_root);

    return NULL;
}

static const char *set_document_root(cmd_parms *cmd, void *dummy,
                                     const char *arg)
{
    void *sconf = cmd->server->module_config;
    core_server_config *conf = (core_server_config *) ap_get_module_config(sconf, &core_module);

    const char *err = ap_check_cmd_context(cmd,
                                           NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

	// merge against the configuration root
	if (apr_filepath_merge((char**)&conf->ap_document_root, Application.Process->conf_root, arg,
                           APR_FILEPATH_TRUENAME, cmd->pool) != APR_SUCCESS
        || !ap_is_directory(cmd->pool, conf->ap_document_root)) {
        if (cmd->server->is_virtual) {
            ap_log_perror(APLOG_MARK, APLOG_STARTUP, 0,
                          cmd->pool,
                          "Warning: DocumentRoot [%s] does not exist",
                          arg);
            conf->ap_document_root = arg;
        }
        else {
            return "DocumentRoot must be a directory";
        }
    }
    return NULL;
}

static void *create_wa_core_server_config(apr_pool_t *a, server_rec *s)
{
    wa_core_server_config *conf;
    int is_virtual = s->is_virtual;

    conf = (wa_core_server_config *) apr_pcalloc(a, sizeof(wa_core_server_config));

#ifdef GPROF
    conf->gprof_dir = NULL;
#endif

    conf->access_name = is_virtual ? NULL : DEFAULT_ACCESS_FNAME;
	conf->ap_document_root = is_virtual ? NULL : Application.Process->conf_root;
    conf->sec_dir = apr_array_make(a, 40, sizeof(ap_conf_vector_t *));
    conf->sec_url = apr_array_make(a, 40, sizeof(ap_conf_vector_t *));

	conf->def_env = apr_table_make(a, sizeof(const char *));
	conf->sav_env = apr_array_make(a, 10, sizeof(const char *));
	conf->sess_env = apr_array_make(a, 10, sizeof(const char *));
	conf->monitor_ext = apr_array_make(a, 10, sizeof(const char *));

    /* recursion stopper */
    conf->redirect_limit = 0; /* 0 == unset */
    conf->subreq_limit = 0;

    return (void *)conf;
}

static void *merge_wa_core_server_configs(apr_pool_t *p, void *basev, void *virtv)
{
    wa_core_server_config *base = (wa_core_server_config *) basev;
    wa_core_server_config *virt = (wa_core_server_config *) virtv;
    wa_core_server_config *conf;

    conf = (wa_core_server_config *) apr_palloc(p, sizeof(wa_core_server_config));
    memcpy(conf, virt, sizeof(wa_core_server_config));

    if (!conf->access_name) {
        conf->access_name = base->access_name;
    }

    if (!conf->ap_document_root) {
        conf->ap_document_root = base->ap_document_root;
    }

    conf->sec_dir = apr_array_append(p, base->sec_dir, virt->sec_dir);
    conf->sec_url = apr_array_append(p, base->sec_url, virt->sec_url);

    conf->redirect_limit = virt->redirect_limit
                           ? virt->redirect_limit
                           : base->redirect_limit;

    conf->subreq_limit = virt->subreq_limit
                         ? virt->subreq_limit
                         : base->subreq_limit;

    return conf;
}

static conn_rec *wa_core_create_conn(apr_pool_t *ptrans, server_rec *server,
                                     apr_socket_t *csd, long id, void *sbh,
                                     apr_bucket_alloc_t *alloc)
{
	// there is no TCP/IP connection, so there's not much to do here
	// just create a structure and set the fields to reasonable values
    wa_conn_rec *c = (wa_conn_rec *) apr_pcalloc(ptrans, sizeof(wa_conn_rec));
	apr_sockaddr_t *addr = (apr_sockaddr_t *) apr_pcalloc(ptrans, sizeof(apr_sockaddr_t));

	addr->pool = ptrans;
	addr->family = APR_INET;	// IP4 address
	addr->addr_str_len = sizeof(addr->sa);	
	addr->salen = addr->ipaddr_len = sizeof(addr->sa.sin.sin_addr);
	addr->sa.sin.sin_family = APR_INET;
	addr->sa.sin.sin_addr.s_addr = 0x0100007F;	// 127.0.0.1 (little endian)
	addr->ipaddr_ptr = &addr->sa.sin.sin_addr;

    c->sbh = sbh;
	c->local_addr = addr;
	c->remote_addr = addr; 
    c->conn_config = ap_create_conn_config(ptrans);
    c->notes = apr_table_make(ptrans, 5);	
    c->pool = ptrans;
    apr_sockaddr_ip_get(&c->local_ip, c->local_addr);
    c->base_server = server;
	c->local_host = "Someplace";

    c->id = id;
    c->bucket_alloc = alloc;

	// the protocol object is stored in the socket as userdata
	c->object = (LPVOID) csd->userdata;

    return c;
}

static int wa_core_pre_connection(conn_rec *c, void *csd)
{
    core_net_rec *net = (core_net_rec *) apr_palloc(c->pool, sizeof(*net));

    net->c = c;
    net->in_ctx = NULL;
    net->out_ctx = NULL;
    net->client_socket = (apr_socket_t *) csd;

    ap_set_module_config(net->c->conn_config, &core_module, csd);
    ap_add_input_filter_handle(ap_core_input_filter_handle, net, NULL, net->c);
    ap_add_output_filter_handle(ap_core_output_filter_handle, net, NULL, net->c);
    return DONE;
}

/*
 * Handle a request to include the server's OS platform in the Server
 * response header field (the ServerTokens directive).  Unfortunately
 * this requires a new global in order to communicate the setting back to
 * http_main so it can insert the information in the right place in the
 * string.
 */

static char *server_version = NULL;
static int version_locked = 0;

enum server_token_type {
    SrvTk_MAJOR,        /* eg: Apache/2 */
    SrvTk_MINOR,        /* eg. Apache/2.0 */
    SrvTk_MINIMAL,      /* eg: Apache/2.0.41 */
    SrvTk_OS,           /* eg: Apache/2.0.41 (UNIX) */
    SrvTk_FULL,         /* eg: Apache/2.0.41 (UNIX) PHP/4.2.2 FooBar/1.2b */
    SrvTk_PRODUCT_ONLY  /* eg: Apache */
};
static enum server_token_type ap_server_tokens = SrvTk_FULL;

/*
 * This routine adds the real server base identity to the version string,
 * and then locks out changes until the next reconfig.
 */
static void ap_set_version(apr_pool_t *pconf)
{
    if (ap_server_tokens == SrvTk_PRODUCT_ONLY) {
        ap_add_version_component(pconf, AP_SERVER_BASEPRODUCT);
    }
    else if (ap_server_tokens == SrvTk_MINIMAL) {
        ap_add_version_component(pconf, AP_SERVER_BASEVERSION);
    }
    else if (ap_server_tokens == SrvTk_MINOR) {
        ap_add_version_component(pconf, AP_SERVER_BASEPRODUCT "/" AP_SERVER_MINORREVISION);
    }
    else if (ap_server_tokens == SrvTk_MAJOR) {
        ap_add_version_component(pconf, AP_SERVER_BASEPRODUCT "/" AP_SERVER_MAJORVERSION);
    }
    else {
        ap_add_version_component(pconf, AP_SERVER_BASEVERSION " (" PLATFORM ")");
    }

    /*
     * Lock the server_version string if we're not displaying
     * the full set of tokens
     */
    if (ap_server_tokens != SrvTk_FULL) {
        version_locked++;
    }
}

static APR_OPTIONAL_FN_TYPE(ap_logio_add_bytes_out) *logio_add_bytes_out;

static int wa_core_post_config(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
    logio_add_bytes_out = APR_RETRIEVE_OPTIONAL_FN(ap_logio_add_bytes_out);

    ap_set_version(pconf);
    ap_setup_make_content_type(pconf);
    return OK;
}

static int wa_process_request(request_rec *r)
{
	wa_conn_rec *wc = (wa_conn_rec *) r->connection;
	WapacheProtocol *proto = (WapacheProtocol *) wc->object;
    int access_status;

    access_status = ap_process_request_internal(r);
    if (access_status == OK) {
		access_status = ap_invoke_handler(r);
    }

    if (access_status == DONE) {
        /* e.g., something not in storage like TRACE */
        access_status = OK;
    }

    if (access_status == OK) {
        ap_finalize_request_protocol(r);
    }
    else {
        ap_die(access_status, r);
    }

	proto->ReportApacheResponseEnd(r);

    ap_run_log_transaction(r);

	return OK;
}

static int wa_core_process_connection(conn_rec *c)
{
    request_rec *r;
	wa_conn_rec *wc = (wa_conn_rec *) c;
	apr_pool_t *p;
	apr_status_t rv;
	WapacheProtocol *proto = (WapacheProtocol *) wc->object;

	// each connection is associated with one request
	// create the request object here
	// need to create a separate pool for the request
	// since it's used inside a worker thread, while the 
	// connection pool is destroyed in the main thread

    apr_pool_create(&p, c->pool);
    r = (request_rec *) apr_pcalloc(p, sizeof(request_rec));
    r->pool            = p;
    r->connection      = c;
    r->server          = c->base_server;

    r->user            = NULL;
    r->ap_auth_type    = NULL;

    r->allowed_methods = ap_make_method_list(r->pool, 2);

    r->headers_in      = apr_table_make(r->pool, 25);
    r->subprocess_env  = apr_table_make(r->pool, 25);
    r->headers_out     = apr_table_make(r->pool, 12);
    r->err_headers_out = apr_table_make(r->pool, 5);
    r->notes           = apr_table_make(r->pool, 5);

    r->request_config  = ap_create_request_config(r->pool);
    /* Must be set before we run create request hook */

    r->proto_output_filters = c->output_filters;
    r->output_filters  = r->proto_output_filters;
    r->proto_input_filters = c->input_filters;
    r->input_filters   = r->proto_input_filters;
    ap_run_create_request(r);
    r->per_dir_config  = r->server->lookup_defaults;

    r->sent_bodyct     = 0;                      /* bytect isn't for body */

    r->read_length     = 0;
    r->read_body       = REQUEST_NO_BODY;

    r->status          = HTTP_OK;
    r->the_request     = NULL;

	proto->BindApacheRequest(r);

    rv = ap_run_post_read_request(r);

	if(!rv) {

		ap_update_vhost_from_headers(r);

		rv = wa_process_request(r);
	}

	apr_pool_destroy(p);

    return rv;
}

static apr_size_t num_request_notes = AP_NUM_STD_NOTES;

static int wa_core_create_req(request_rec *r)
{
    /* Alloc the config struct and the array of request notes in
     * a single block for efficiency
     */
    core_request_config *req_cfg;

    req_cfg = (core_request_config *) apr_pcalloc(r->pool, sizeof(core_request_config) +
												  sizeof(void *) * num_request_notes);
    req_cfg->notes = (void **)((char *)req_cfg + sizeof(core_request_config));

    /* ### temporarily enable script delivery as the default */
    req_cfg->deliver_script = 1;

    if (r->main) {
        core_request_config *main_req_cfg = (core_request_config *)
            ap_get_module_config(r->main->request_config, &core_module);
        req_cfg->bb = main_req_cfg->bb;
    }
    else {
        req_cfg->bb = apr_brigade_create(r->pool, r->connection->bucket_alloc);
		/*
        if (!r->prev) {
            ap_add_input_filter_handle(ap_net_time_filter_handle,
                                       NULL, r, r->connection);
        }
		*/
    }

    ap_set_module_config(r->request_config, &core_module, req_cfg);

    /* Begin by presuming any module can make its own path_info assumptions,
     * until some module interjects and changes the value.
     */
    r->used_path_info = AP_REQ_DEFAULT_PATH_INFO;
    ap_add_output_filter_handle(ap_http_header_filter_handle, NULL, r, r->connection);

    return OK;
}

static int wa_core_translate(request_rec *r)
{
    void *sconf = r->server->module_config;
    core_server_config *conf = (core_server_config *) ap_get_module_config(sconf, &core_module);

    /* XXX this seems too specific, this should probably become
     * some general-case test
     */
    if (r->proxyreq) {
        return HTTP_FORBIDDEN;
    }
    if (!r->uri || ((r->uri[0] != '/') && strcmp(r->uri, "*"))) {
        ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                     "Invalid URI in request %s", r->the_request);
        return HTTP_BAD_REQUEST;
    }

    if (r->server->path
        && !strncmp(r->uri, r->server->path, r->server->pathlen)
        && (r->server->path[r->server->pathlen - 1] == '/'
            || r->uri[r->server->pathlen] == '/'
            || r->uri[r->server->pathlen] == '\0')) 
    {
        /* skip all leading /'s (e.g. http://localhost///foo) 
         * so we are looking at only the relative path.
         */
        char *path = r->uri + r->server->pathlen;
        while (*path == '/') {
            ++path;
        }
        if (apr_filepath_merge(&r->filename, conf->ap_document_root, path,
                               APR_FILEPATH_TRUENAME
                             | APR_FILEPATH_SECUREROOT, r->pool)
                    != APR_SUCCESS) {
            return HTTP_FORBIDDEN;
        }
        r->canonical_filename = r->filename;
    }
    else {
        /*
         * Make sure that we do not mess up the translation by adding two
         * /'s in a row.  This happens under windows when the document
         * root ends with a /
         */
        /* skip all leading /'s (e.g. http://localhost///foo) 
         * so we are looking at only the relative path.
         */
        char *path = r->uri;
        while (*path == '/') {
            ++path;
        }
        if (apr_filepath_merge(&r->filename, conf->ap_document_root, path,
                               APR_FILEPATH_TRUENAME
                             | APR_FILEPATH_SECUREROOT, r->pool)
                    != APR_SUCCESS) {
            return HTTP_FORBIDDEN;
        }
        r->canonical_filename = r->filename;
    }

    return OK;
}

static int wa_core_map_to_storage(request_rec *r)
{
    int access_status;

    if ((access_status = ap_directory_walk(r))) {
        return access_status;
    }

    if ((access_status = ap_file_walk(r))) {
        return access_status;
    }

    return OK;
}

static int wa_default_handler(request_rec *r)
{
    conn_rec *c = r->connection;
    apr_bucket_brigade *bb;
    apr_bucket *e;
    core_dir_config *d;
    int errstatus;
    apr_file_t *fd = NULL;
    apr_status_t status;
    /* XXX if/when somebody writes a content-md5 filter we either need to
     *     remove this support or coordinate when to use the filter vs.
     *     when to use this code
     *     The current choice of when to compute the md5 here matches the 1.3
     *     support fairly closely (unlike 1.3, we don't handle computing md5
     *     when the charset is translated).
     */
    int bld_content_md5;

    d = (core_dir_config *)ap_get_module_config(r->per_dir_config,
                                                &core_module);
    bld_content_md5 = (d->content_md5 & 1)
                      && r->output_filters->frec->ftype != AP_FTYPE_RESOURCE;

    ap_allow_standard_methods(r, MERGE_ALLOW, M_GET, M_OPTIONS, M_POST, -1);

    /* If filters intend to consume the request body, they must
     * register an InputFilter to slurp the contents of the POST
     * data from the POST input stream.  It no longer exists when
     * the output filters are invoked by the default handler.
     */
    if ((errstatus = ap_discard_request_body(r)) != OK) {
        return errstatus;
    }

    if (r->method_number == M_GET || r->method_number == M_POST) {
        if (r->finfo.filetype == 0) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                          "File does not exist: %s", r->filename);
            return HTTP_NOT_FOUND;
        }

        /* Don't try to serve a dir.  Some OSs do weird things with
         * raw I/O on a dir.
         */
        if (r->finfo.filetype == APR_DIR) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                          "Attempt to serve directory: %s", r->filename);
            return HTTP_NOT_FOUND;
        }

        if ((r->used_path_info != AP_REQ_ACCEPT_PATH_INFO) &&
            r->path_info && *r->path_info)
        {
            /* default to reject */
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                          "File does not exist: %s",
                          apr_pstrcat(r->pool, r->filename, r->path_info, NULL));
            return HTTP_NOT_FOUND;
        }

        /* We understood the (non-GET) method, but it might not be legal for
           this particular resource. Check to see if the 'deliver_script'
           flag is set. If so, then we go ahead and deliver the file since
           it isn't really content (only GET normally returns content).

           Note: based on logic further above, the only possible non-GET
           method at this point is POST. In the future, we should enable
           script delivery for all methods.  */
        if (r->method_number != M_GET) {
            core_request_config *req_cfg;

            req_cfg = (core_request_config *) ap_get_module_config(r->request_config, &core_module);
            if (!req_cfg->deliver_script) {
                /* The flag hasn't been set for this request. Punt. */
                ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                              "This resource does not accept the %s method.",
                              r->method);
                return HTTP_METHOD_NOT_ALLOWED;
            }
        }


        if ((status = apr_file_open(&fd, r->filename, APR_READ | APR_BINARY
#if APR_HAS_SENDFILE
                            | ((d->enable_sendfile == ENABLE_SENDFILE_OFF) 
                                                ? 0 : APR_SENDFILE_ENABLED)
#endif
                                    , 0, r->pool)) != APR_SUCCESS) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, status, r,
                          "file permissions deny server access: %s", r->filename);
            return HTTP_FORBIDDEN;
        }

        ap_update_mtime(r, r->finfo.mtime);
        ap_set_last_modified(r);
        ap_set_etag(r);
        apr_table_setn(r->headers_out, "Accept-Ranges", "bytes");
        ap_set_content_length(r, r->finfo.size);
        if ((errstatus = ap_meets_conditions(r)) != OK) {
            apr_file_close(fd);
            return errstatus;
        }

		/*
        if (bld_content_md5) {
            apr_table_setn(r->headers_out, "Content-MD5",
                           ap_md5digest(r->pool, fd));
        }
		*/

        bb = apr_brigade_create(r->pool, c->bucket_alloc);
#if APR_HAS_LARGE_FILES
#if APR_HAS_SENDFILE
        if ((d->enable_sendfile != ENABLE_SENDFILE_OFF) &&
#else
        if (
#endif
            (r->finfo.size > AP_MAX_SENDFILE)) {
            /* APR_HAS_LARGE_FILES issue; must split into mutiple buckets,
             * no greater than MAX(apr_size_t), and more granular than that
             * in case the brigade code/filters attempt to read it directly.
             */
            apr_off_t fsize = r->finfo.size;
            e = apr_bucket_file_create(fd, 0, AP_MAX_SENDFILE, r->pool,
                                       c->bucket_alloc);
            while (fsize > AP_MAX_SENDFILE) {
                apr_bucket *ce;
                apr_bucket_copy(e, &ce);
                APR_BRIGADE_INSERT_TAIL(bb, ce);
                e->start += AP_MAX_SENDFILE;
                fsize -= AP_MAX_SENDFILE;
            }
            e->length = (apr_size_t)fsize; /* Resize just the last bucket */
        }
        else
#endif
            e = apr_bucket_file_create(fd, 0, (apr_size_t)r->finfo.size,
                                       r->pool, c->bucket_alloc);

#if APR_HAS_MMAP
        if (d->enable_mmap == ENABLE_MMAP_OFF) {
            (void)apr_bucket_file_enable_mmap(e, 0);
        }
#endif
        APR_BRIGADE_INSERT_TAIL(bb, e);
        e = apr_bucket_eos_create(c->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL(bb, e);

        return ap_pass_brigade(r->output_filters, bb);
    }
    else {              /* unusual method (not GET or POST) */
        if (r->method_number == M_INVALID) {
            ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
                          "Invalid method in request %s", r->the_request);
            return HTTP_NOT_IMPLEMENTED;
        }

        if (r->method_number == M_OPTIONS) {
            return ap_send_http_options(r);
        }
        return HTTP_METHOD_NOT_ALLOWED;
    }
}

static int do_nothing(request_rec *r) { return OK; }

/**
 * Remove all zero length buckets from the brigade.
 */
#define BRIGADE_NORMALIZE(b) \
do { \
    apr_bucket *e = APR_BRIGADE_FIRST(b); \
    do {  \
        if (e->length == 0 && !APR_BUCKET_IS_METADATA(e)) { \
            apr_bucket *d; \
            d = APR_BUCKET_NEXT(e); \
            apr_bucket_delete(e); \
            e = d; \
        } \
        e = APR_BUCKET_NEXT(e); \
    } while (!APR_BRIGADE_EMPTY(b) && (e != APR_BRIGADE_SENTINEL(b))); \
} while (0)

static int wa_core_input_filter(ap_filter_t *f, apr_bucket_brigade *b,
                                ap_input_mode_t mode, apr_read_type_e block,
                                apr_off_t readbytes)
{
    apr_bucket *e;
    apr_status_t rv;
    core_net_rec *net = (core_net_rec *) f->ctx;
    core_ctx_t *ctx = net->in_ctx;
    const char *str;
    apr_size_t len;
	wa_conn_rec *wc = (wa_conn_rec *) net->c;
	WapacheProtocol *proto = (WapacheProtocol *) wc->object;

    if (mode == AP_MODE_INIT) {
        /*
         * this mode is for filters that might need to 'initialize'
         * a connection before reading request data from a client.
         * NNTP over SSL for example needs to handshake before the
         * server sends the welcome message.
         * such filters would have changed the mode before this point
         * is reached.  however, protocol modules such as NNTP should
         * not need to know anything about SSL.  given the example, if
         * SSL is not in the filter chain, AP_MODE_INIT is a noop.
         */
        return APR_SUCCESS;
    }

    if (!ctx)
    {
        ctx = (core_ctx_t *) apr_pcalloc(f->c->pool, sizeof(*ctx));
        ctx->b = apr_brigade_create(f->c->pool, f->c->bucket_alloc);

        /* seed the brigade with input data from IInternetBindInfo */
        e = proto->CreateInputBucket(f->c->bucket_alloc);
		APR_BRIGADE_INSERT_TAIL(ctx->b, e);
        net->in_ctx = ctx;
    }
    else if (APR_BRIGADE_EMPTY(ctx->b)) {
        return APR_EOF;
    }

    /* ### This is bad. */
    BRIGADE_NORMALIZE(ctx->b);

    /* check for empty brigade again *AFTER* BRIGADE_NORMALIZE()
     * If we have lost our socket bucket (see above), we are EOF.
     *
     * Ideally, this should be returning SUCCESS with EOS bucket, but
     * some higher-up APIs (spec. read_request_line via ap_rgetline)
     * want an error code. */
    if (APR_BRIGADE_EMPTY(ctx->b)) {
        return APR_EOF;
    }

    if (mode == AP_MODE_GETLINE) {
        /* we are reading a single LF line, e.g. the HTTP headers */
        rv = apr_brigade_split_line(b, ctx->b, block, HUGE_STRING_LEN);
        /* We should treat EAGAIN here the same as we do for EOF (brigade is
         * empty).  We do this by returning whatever we have read.  This may
         * or may not be bogus, but is consistent (for now) with EOF logic.
         */
        if (APR_STATUS_IS_EAGAIN(rv)) {
            rv = APR_SUCCESS;
        }
        return rv;
    }

    /* ### AP_MODE_PEEK is a horrific name for this mode because we also
     * eat any CRLFs that we see.  That's not the obvious intention of
     * this mode.  Determine whether anyone actually uses this or not. */
    if (mode == AP_MODE_EATCRLF) {
        apr_bucket *e;
        const char *c;

        /* The purpose of this loop is to ignore any CRLF (or LF) at the end
         * of a request.  Many browsers send extra lines at the end of POST
         * requests.  We use the PEEK method to determine if there is more
         * data on the socket, so that we know if we should delay sending the
         * end of one request until we have served the second request in a
         * pipelined situation.  We don't want to actually delay sending a
         * response if the server finds a CRLF (or LF), becuause that doesn't
         * mean that there is another request, just a blank line.
         */
        while (1) {
            if (APR_BRIGADE_EMPTY(ctx->b))
                return APR_EOF;

            e = APR_BRIGADE_FIRST(ctx->b);

            rv = apr_bucket_read(e, &str, &len, APR_NONBLOCK_READ);

            if (rv != APR_SUCCESS)
                return rv;

            c = str;
            while (c < str + len) {
                if (*c == APR_ASCII_LF)
                    c++;
                else if (*c == APR_ASCII_CR && *(c + 1) == APR_ASCII_LF)
                    c += 2;
                else
                    return APR_SUCCESS;
            }

            /* If we reach here, we were a bucket just full of CRLFs, so
             * just toss the bucket. */
            /* FIXME: Is this the right thing to do in the core? */
            apr_bucket_delete(e);
        }
        return APR_SUCCESS;
    }

    /* If mode is EXHAUSTIVE, we want to just read everything until the end
     * of the brigade, which in this case means the end of the socket.
     * To do this, we attach the brigade that has currently been setaside to
     * the brigade that was passed down, and send that brigade back.
     *
     * NOTE:  This is VERY dangerous to use, and should only be done with
     * extreme caution.  However, the Perchild MPM needs this feature
     * if it is ever going to work correctly again.  With this, the Perchild
     * MPM can easily request the socket and all data that has been read,
     * which means that it can pass it to the correct child process.
     */
    if (mode == AP_MODE_EXHAUSTIVE) {
        apr_bucket *e;

        /* Tack on any buckets that were set aside. */
        APR_BRIGADE_CONCAT(b, ctx->b);

        /* Since we've just added all potential buckets (which will most
         * likely simply be the socket bucket) we know this is the end,
         * so tack on an EOS too. */
        /* We have read until the brigade was empty, so we know that we
         * must be EOS. */
        e = apr_bucket_eos_create(f->c->bucket_alloc);
        APR_BRIGADE_INSERT_TAIL(b, e);
        return APR_SUCCESS;
    }

    /* read up to the amount they specified. */
    if (mode == AP_MODE_READBYTES || mode == AP_MODE_SPECULATIVE) {
        apr_bucket *e;
        apr_bucket_brigade *newbb;

        AP_DEBUG_ASSERT(readbytes > 0);

        e = APR_BRIGADE_FIRST(ctx->b);
        rv = apr_bucket_read(e, &str, &len, block);

        if (APR_STATUS_IS_EAGAIN(rv)) {
            return APR_SUCCESS;
        }
        else if (rv != APR_SUCCESS) {
            return rv;
        }
        else if (block == APR_BLOCK_READ && len == 0) {
            /* We wanted to read some bytes in blocking mode.  We read
             * 0 bytes.  Hence, we now assume we are EOS.
             *
             * When we are in normal mode, return an EOS bucket to the
             * caller.
             * When we are in speculative mode, leave ctx->b empty, so
             * that the next call returns an EOS bucket.
             */
            apr_bucket_delete(e);

            if (mode == AP_MODE_READBYTES) {
                e = apr_bucket_eos_create(f->c->bucket_alloc);
                APR_BRIGADE_INSERT_TAIL(b, e);
            }
            return APR_SUCCESS;
        }

        /* We can only return at most what we read. */
        if (len < readbytes) {
            readbytes = len;
        }

        rv = apr_brigade_partition(ctx->b, readbytes, &e);
        if (rv != APR_SUCCESS) {
            return rv;
        }

        /* Must do split before CONCAT */
        newbb = apr_brigade_split(ctx->b, e);

        if (mode == AP_MODE_READBYTES) {
            APR_BRIGADE_CONCAT(b, ctx->b);
        }
        else if (mode == AP_MODE_SPECULATIVE) {
            apr_bucket *copy_bucket;
            APR_BRIGADE_FOREACH(e, ctx->b) {
                rv = apr_bucket_copy(e, &copy_bucket);
                if (rv != APR_SUCCESS) {
                    return rv;
                }
                APR_BRIGADE_INSERT_TAIL(b, copy_bucket);
            }
        }

        /* Take what was originally there and place it back on ctx->b */
        APR_BRIGADE_CONCAT(ctx->b, newbb);
    }
    return APR_SUCCESS;
}

static apr_status_t wa_http_header_filter(ap_filter_t *f, apr_bucket_brigade *b)
{
    request_rec *r = f->r;
    conn_rec *c = r->connection;
    const char *clheader;
    apr_bucket *e;
	wa_conn_rec *wc = (wa_conn_rec *) c;
	WapacheProtocol *proto = (WapacheProtocol *) wc->object;

    APR_BRIGADE_FOREACH(e, b) {
        if (e->type == &ap_bucket_type_error) {
            ap_bucket_error *eb = (ap_bucket_error *) e->data;

            ap_die(eb->status, r);
            return AP_FILTER_ERROR;
        }
    }

    /*
     * Now remove any ETag response header field if earlier processing
     * says so (such as a 'FileETag None' directive).
     */
    if (apr_table_get(r->notes, "no-etag") != NULL) {
        apr_table_unset(r->headers_out, "ETag");
    }

    apr_table_setn(r->headers_out, "Content-Type", 
                   ap_make_content_type(r, r->content_type));

    if (r->content_encoding) {
        apr_table_setn(r->headers_out, "Content-Encoding",
                       r->content_encoding);
    }

    if (!apr_is_empty_array(r->content_languages)) {
        int i;
        char **languages = (char **)(r->content_languages->elts);
        for (i = 0; i < r->content_languages->nelts; ++i) {
            apr_table_mergen(r->headers_out, "Content-Language", languages[i]);
        }
    }

     /* This is a hack, but I can't find anyway around it.  The idea is that
     * we don't want to send out 0 Content-Lengths if it is a head request.
     * This happens when modules try to outsmart the server, and return
     * if they see a HEAD request.  Apache 1.3 handlers were supposed to
     * just return in that situation, and the core handled the HEAD.  In
     * 2.0, if a handler returns, then the core sends an EOS bucket down
     * the filter stack, and the content-length filter computes a C-L of
     * zero and that gets put in the headers, and we end up sending a
     * zero C-L to the client.  We can't just remove the C-L filter,
     * because well behaved 2.0 handlers will send their data down the stack,
     * and we will compute a real C-L for the head request. RBB
     */
    if (r->header_only
        && (clheader = apr_table_get(r->headers_out, "Content-Length"))
        && !strcmp(clheader, "0")) {
        apr_table_unset(r->headers_out, "Content-Length");
    }

    if (!r->status_line) {
        r->status_line = status_lines[ap_index_of_response(r->status)];
    }

	// pass the response headers to APP
	proto->ReportApacheResponseStart(r);

    ap_remove_output_filter(f);

    if (r->header_only) {
        apr_brigade_destroy(b);
        return OK;
    }

    r->sent_bodyct = 1;         /* Whatever follows is real body stuff... */
    return ap_pass_brigade(f->next, b);
}

static int core_override_type(request_rec *r)
{
    core_dir_config *conf =
        (core_dir_config *)ap_get_module_config(r->per_dir_config,
                                                &core_module);

    /* Check for overrides with ForceType / SetHandler
     */
    if (conf->mime_type && strcmp(conf->mime_type, "none"))
        ap_set_content_type(r, (char*) conf->mime_type);

    if (conf->handler && strcmp(conf->handler, "none"))
        r->handler = conf->handler;

    /* Deal with the poor soul who is trying to force path_info to be
     * accepted within the core_handler, where they will let the subreq
     * address its contents.  This is toggled by the user in the very
     * beginning of the fixup phase, so modules should override the user's
     * discretion in their own module fixup phase.  It is tristate, if
     * the user doesn't specify, the result is 2 (which the module may
     * interpret to its own customary behavior.)  It won't be touched
     * if the value is no longer undefined (2), so any module changing
     * the value prior to the fixup phase OVERRIDES the user's choice.
     */
    if ((r->used_path_info == AP_REQ_DEFAULT_PATH_INFO)
        && (conf->accept_path_info != 3)) {
        r->used_path_info = conf->accept_path_info;
    }

    return OK;
}

static apr_status_t wa_core_output_filter(ap_filter_t *f, apr_bucket_brigade *b)
{
    conn_rec *c = f->c;
    core_net_rec *net = (core_net_rec *) f->ctx;
    core_output_filter_ctx_t *ctx = net->out_ctx;
	wa_conn_rec *wc = (wa_conn_rec *) c;
	WapacheProtocol *proto = (WapacheProtocol *) wc->object;

    if (ctx == NULL) {
        ctx = (core_output_filter_ctx_t *) apr_pcalloc(c->pool, sizeof(*ctx));
        net->out_ctx = ctx;
    }

	// hand the brigade to APP for processing by MSHTML
	proto->TakeOutputBuckets(b);

    apr_brigade_destroy(b);
    return APR_SUCCESS;
}

static const char *wa_core_http_method(const request_rec *r)
    { return "http"; }

static apr_port_t wa_core_default_port(const request_rec *r)
    { return DEFAULT_HTTP_PORT; }

static void register_hooks(apr_pool_t *p)
{
    ap_hook_create_connection(wa_core_create_conn, NULL, NULL,
                              APR_HOOK_REALLY_LAST);
    ap_hook_pre_connection(wa_core_pre_connection, NULL, NULL,
                           APR_HOOK_REALLY_LAST);
    ap_hook_process_connection(wa_core_process_connection,NULL,NULL,
							   APR_HOOK_REALLY_LAST);
    ap_hook_post_config(wa_core_post_config,NULL,NULL,APR_HOOK_REALLY_FIRST);
    ap_hook_translate_name(wa_core_translate,NULL,NULL,APR_HOOK_REALLY_LAST);
    ap_hook_map_to_storage(wa_core_map_to_storage,NULL,NULL,APR_HOOK_REALLY_LAST);
	ap_hook_http_method(wa_core_http_method,NULL,NULL,APR_HOOK_REALLY_LAST);
    ap_hook_default_port(wa_core_default_port,NULL,NULL,APR_HOOK_REALLY_LAST);
    ap_hook_handler(wa_default_handler,NULL,NULL,APR_HOOK_REALLY_LAST);
    ap_hook_type_checker(do_nothing,NULL,NULL,APR_HOOK_REALLY_LAST);
    ap_hook_fixups(core_override_type,NULL,NULL,APR_HOOK_REALLY_FIRST);
    ap_hook_access_checker(do_nothing,NULL,NULL,APR_HOOK_REALLY_LAST);
    ap_hook_create_request(wa_core_create_req, NULL, NULL, APR_HOOK_MIDDLE);

    ap_core_input_filter_handle =
        ap_register_input_filter("CORE_IN", wa_core_input_filter,
                                 NULL, AP_FTYPE_NETWORK);

    ap_http_header_filter_handle =
        ap_register_output_filter("HTTP_HEADER", wa_http_header_filter, 
                                  NULL, AP_FTYPE_PROTOCOL);

    ap_core_output_filter_handle =
        ap_register_output_filter("CORE", wa_core_output_filter,
                                  NULL, AP_FTYPE_NETWORK);
    //ap_subreq_core_filter_handle =
    //   ap_register_output_filter("SUBREQ_CORE", wa_sub_req_output_filter,
    //                             NULL, AP_FTYPE_CONTENT_SET);
    ap_old_write_func =
        ap_register_output_filter("OLD_WRITE", ap_old_write_filter,
                                  NULL, (ap_filter_type) 0);
}

static char *unclosed_directive(cmd_parms *cmd)
{
    return apr_pstrcat(cmd->pool, cmd->cmd->name,
                       "> directive missing closing '>'", NULL);
}

static const char *wa_virtualhost_section(cmd_parms *cmd, void *dummy,
										  const char *arg)
{
    server_rec *main_server = cmd->server, *s;
    const char *errmsg;
    const char *endp = ap_strrchr_c(arg, '>');
    apr_pool_t *p = cmd->pool;

    const char *err = ap_check_cmd_context(cmd, GLOBAL_ONLY);
    if (err != NULL) {
        return err;
    }

    if (endp == NULL) {
        return unclosed_directive(cmd);
    }

    arg = apr_pstrndup(cmd->pool, arg, endp - arg);

    /* FIXME: There's another feature waiting to happen here -- since you
        can now put multiple addresses/names on a single <VirtualHost>
        you might want to use it to group common definitions and then
        define other "subhosts" with their individual differences.  But
        personally I'd rather just do it with a macro preprocessor. -djg */
    if (main_server->is_virtual) {
        return "<VirtualHost> doesn't nest!";
    }

    errmsg = wa_init_virtual_host(p, arg, main_server, &s);
    if (errmsg) {
        return errmsg;
    }

    s->next = main_server->next;
    main_server->next = s;

    s->defn_name = cmd->directive->filename;
    s->defn_line_number = cmd->directive->line_num;

    cmd->server = s;

    errmsg = ap_walk_config(cmd->directive->first_child, cmd,
                            s->lookup_defaults);

    cmd->server = main_server;

    return errmsg;
}

static const char *wa_add_sess_env(cmd_parms *cmd, void *mconfig, const char *arg)
{
    const command_rec *thiscmd = cmd->cmd;
    void *sconf = cmd->server->module_config;
    wa_core_server_config *conf = 
			(wa_core_server_config *) ap_get_module_config(sconf, &core_module);

    const char *err = ap_check_cmd_context(cmd,
                                           NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

	char **slot = (char **) apr_array_push(thiscmd->cmd_data ? conf->sav_env : conf->sess_env);
	char *name = ap_getword_conf(cmd->pool, &arg);
	*slot = name;

	return NULL;
}

static const char *wa_add_def_env(cmd_parms *cmd, void *mconfig, const char *arg1, 
								  const char *arg2)
{
    const command_rec *thiscmd = cmd->cmd;
    void *sconf = cmd->server->module_config;
    wa_core_server_config *conf = 
			(wa_core_server_config *) ap_get_module_config(sconf, &core_module);

    const char *err = ap_check_cmd_context(cmd,
                                           NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

	char *name = ap_getword_conf(cmd->pool, &arg1);
	char *value = ap_getword_conf(cmd->pool, &arg2);

	apr_table_set(conf->def_env, name, value);

	return NULL;
}

static const char *wa_add_mon_ext(cmd_parms *cmd, void *mconfig, const char *arg)
{
    const command_rec *thiscmd = cmd->cmd;
    void *sconf = cmd->server->module_config;
    wa_core_server_config *conf = 
			(wa_core_server_config *) ap_get_module_config(sconf, &core_module);

    const char *err = ap_check_cmd_context(cmd,
                                           NOT_IN_DIR_LOC_FILE|NOT_IN_LIMIT);
    if (err != NULL) {
        return err;
    }

	
	char *ext = ap_getword_conf(cmd->pool, &arg);

	if(stricmp(ext, "Off") != 0) {
		char **slot = (char **) apr_array_push(conf->monitor_ext);
		*slot = strlwr(ext);
	}

	return NULL;
}


static const command_rec wa_core_cmds[] = {
AP_INIT_TAKE1("DocumentRoot", (cmd_func) set_document_root, NULL, RSRC_CONF,
  "Root directory of the document tree"),
AP_INIT_RAW_ARGS("<VirtualHost", (cmd_func) wa_virtualhost_section, NULL, RSRC_CONF,
  "Container to map directives to a particular virtual host, takes one or "
  "more host addresses"),
AP_INIT_TAKE1("SessionEnv", (cmd_func) wa_add_sess_env, NULL, RSRC_CONF,
  "name of the variable"),
AP_INIT_TAKE1("PersistentEnv", (cmd_func) wa_add_sess_env, (void *) 1, RSRC_CONF,
  "name of the variable"),
AP_INIT_TAKE2("DefaultEnv", (cmd_func) wa_add_def_env, NULL, RSRC_CONF,
  "name of the variable and its value"),
AP_INIT_ITERATE("MonitorFiles", (cmd_func) wa_add_mon_ext, NULL, RSRC_CONF,
  "extensions of files to monitor"),
    { NULL }  
};

int __count_cmds(const command_rec *t) {
	int size = 0;
	while(t[size].name) {
		size++;
	}	
	return size;
}

int __find_cmd(const command_rec *t, const char *name) {
	unsigned int index = 0;
	while(t[index].name) {
		if(stricmp(t[index].name, name) == 0) {
			return index;
		}
		index++;
	}	
	return -1;
}

extern "C" {
	extern module wa_core_module;
	extern module wa_client_module;
	extern __declspec( dllimport ) module core_module;
	extern __declspec( dllimport ) module so_module;
	extern __declspec( dllimport ) module win32_module;
}

void wa_hack_core(void) {

	// Apache loads these core modules by default:
	//
	//	core_module
	//	win32_module
	//	mpm_winnt_module
	//	http_module
	//	so_module
	//
	// we'll replace core_module with our own "core"
	// and remove the ones that we don't need
  
	ap_prelinked_modules[0] = ap_preloaded_modules[0] = &core_module;
	ap_prelinked_modules[1] = ap_preloaded_modules[1] = &win32_module;
	ap_prelinked_modules[2] = ap_preloaded_modules[2] = &so_module;
	ap_prelinked_modules[3] = ap_preloaded_modules[3] = &wa_client_module;
	ap_prelinked_modules[4] = ap_preloaded_modules[4] = NULL;
	ap_prelinked_modules[5] = ap_preloaded_modules[5] = NULL;

	// now, we'll merge core_module's config command table with our own

	// first, find out how large the table could be
	int num_core_entries = __count_cmds(core_module.cmds);
	int num_wa_core_entries = __count_cmds(wa_core_module.cmds);
	command_rec *new_wa_core_cmds = new command_rec[num_core_entries + num_wa_core_entries + 1];
	// copy the entries from the original
	memcpy(new_wa_core_cmds, core_module.cmds, (num_core_entries + 1) * sizeof(command_rec));
	int last = num_core_entries;
	// add our entries, overriding those from the original
	for(const command_rec *p = wa_core_module.cmds; p->name != NULL; p++) {
		int index = __find_cmd(core_module.cmds, p->name);
		if(index != -1) {
			new_wa_core_cmds[index] = *p;
		}
		else {
			new_wa_core_cmds[last++] = *p;
		}
	}
	new_wa_core_cmds[last] = wa_core_module.cmds[num_wa_core_entries]; // terminator
	wa_core_module.cmds = new_wa_core_cmds;

	// copy functions for dealing with per-dir config
	wa_core_module.create_dir_config = core_module.create_dir_config;
	wa_core_module.merge_dir_config = core_module.merge_dir_config;

	// ok, now we'll copy wa_core_module into core_module
	// we need to do this because many of the core functions
	// use core_module's memory addresses

	memcpy(&core_module, &wa_core_module, sizeof(module));
}

extern "C" {

module wa_core_module = {
    STANDARD20_MODULE_STUFF,
    NULL,						     /* create per-directory config structure */
    NULL,							 /* merge per-directory config structures */
    create_wa_core_server_config,    /* create per-server config structure */
    merge_wa_core_server_configs,    /* merge per-server config structures */
    wa_core_cmds,                    /* command apr_table_t */
    register_hooks                   /* register hooks */
};

}