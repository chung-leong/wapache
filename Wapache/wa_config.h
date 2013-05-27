#ifndef __WA_CONFIG_H
#define __WA_CONFOG_H

extern "C" {

const char * wa_init_virtual_host(apr_pool_t *p,
								  const char *hostname,
								  server_rec *main_server,
								  server_rec **ps);

const char *wa_walk_client_config(ap_directive_t *current,
								  cmd_parms *parms,
								  void *section_conf);

int wa_process_config_tree(server_rec *s,
                           ap_directive_t *conftree,
                           apr_pool_t *p, apr_pool_t *ptemp);

}

#endif;