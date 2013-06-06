/*
 * Copyright (c) 2012, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Intel Corporation nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <murphy/common/debug.h>
#include <murphy/common/mainloop.h>

#include "src/daemon/plugin.h"
#include "src/daemon/recognizer.h"

#define FAKE_NAME        "fake-speech"
#define FAKE_DESCRIPTION "A fake/test SRS speech engine to test the infra."
#define FAKE_AUTHORS     "Krisztian Litkey <krisztian.litkey@intel.com>"
#define FAKE_VERSION     "0.0.1"


typedef struct {
    char     *token;                     /* token */
    double    next;                      /* time (in secs) till next token */
} fake_token_t;


typedef struct {
    srs_plugin_t      *self;             /* fake speech backend plugin */
    srs_srec_notify_t  notify;           /* recognition notification callback */
    void              *notify_data;      /* notification callback data */
    int                active;           /* have been activated */
    fake_token_t      *tokens;           /* fake tokens to push */
    int                tokidx;           /* next token index */
    mrp_timer_t       *toktmr;           /* timer for next token */
} fake_t;


static fake_token_t tokens[] = {
    { "hal"  , 1.0 },
    { "open" , 0.5 },
    { "the"  , 0.3 },
    { "pod"  , 0.2 },
    { "bay"  , 0.5 },
    { "doors", 1.0 },
    { NULL   , 0.0 }
};


static void push_token_cb(mrp_timer_t *t, void *user_data);


static int arm_token_timer(fake_t *fake, double delay)
{
    srs_context_t *srs   = fake->self->srs;
    unsigned int   msecs = (unsigned int)(1000 * delay);

    mrp_del_timer(fake->toktmr);
    fake->toktmr = mrp_add_timer(srs->ml, msecs, push_token_cb, fake);

    if (fake->toktmr != NULL)
        return TRUE;
    else
        return FALSE;
}


static void push_token_cb(mrp_timer_t *t, void *user_data)
{
    fake_t           *fake  = (fake_t *)user_data;
    fake_token_t     *token = fake->tokens + fake->tokidx++;
    srs_srec_token_t  tok;

    mrp_del_timer(t);
    fake->toktmr = NULL;

    if (token->token == NULL) {
        fake->tokidx = 0;
        return;
    }

    arm_token_timer(fake, token->next);

    tok.token = token->token;
    tok.score = 1;
    tok.start = token - fake->tokens;
    tok.end   = tok.start + 1;
    tok.flush = FALSE;

    fake->notify(&tok, 1, fake->notify_data);
}


static int fake_activate(void *user_data)
{
    fake_t *fake = (fake_t *)user_data;

    if (fake->active)
        return TRUE;

    mrp_debug("activating fake backend");

    fake->tokens = tokens;
    fake->tokidx = 0;

    if (arm_token_timer(fake, fake->tokens->next)) {
        fake->active = TRUE;
        return TRUE;
    }
    else
        return FALSE;
}


static void fake_deactivate(void *user_data)
{
    fake_t *fake = (fake_t *)user_data;

    if (fake->active) {
        mrp_debug("deactivating fake backend");

        mrp_del_timer(fake->toktmr);
        fake->toktmr = NULL;
        fake->active = FALSE;
    }
}


static int fake_flush(uint32_t start, uint32_t end, void *user_data)
{
    fake_t *fake = (fake_t *)user_data;

    MRP_UNUSED(fake);

    mrp_debug("flushing fake backend buffer (%u - %u)", start, end);

    return TRUE;
}


static int fake_rescan(uint32_t start, uint32_t end, void *user_data)
{
    fake_t *fake = (fake_t *)user_data;

    MRP_UNUSED(fake);

    mrp_debug("scheduling fake backend buffer rescan (%u - %u)", start, end);

    return TRUE;
}


static void *fake_sampledup(uint32_t start, uint32_t end, void *user_data)
{
    fake_t   *fake = (fake_t *)user_data;
    uint32_t *buf  = mrp_allocz(2 * sizeof(*buf));

    MRP_UNUSED(fake);

    mrp_debug("duplicating fake backend sample (%u - %u)", start, end);

    buf[0] = start;
    buf[1] = end;

    return (void *)buf;
}


static int fake_check_model(const char *model, void *user_data)
{
    fake_t *fake = (fake_t *)user_data;

    MRP_UNUSED(fake);

    mrp_debug("checking model '%s' for fake backend", model);

    return TRUE;
}


static int fake_check_dictionary(const char *dictionary, void *user_data)
{
    fake_t *fake = (fake_t *)user_data;

    MRP_UNUSED(fake);

    mrp_debug("checking dictionary '%s' for fake backend", dictionary);

    return TRUE;
}


static int fake_set_model(const char *model, void *user_data)
{
    fake_t *fake = (fake_t *)user_data;

    MRP_UNUSED(fake);

    mrp_debug("setting model '%s' for fake backend", model);

    return TRUE;
}


static int fake_set_dictionary(const char *dictionary, void *user_data)
{
    fake_t *fake = (fake_t *)user_data;

    MRP_UNUSED(fake);

    mrp_debug("setting dictionary '%s' for fake backend", dictionary);

    return TRUE;
}


static int create_fake(srs_plugin_t *plugin)
{
    srs_srec_api_t  fake_api = {
    activate:         fake_activate,
    deactivate:       fake_deactivate,
    flush:            fake_flush,
    rescan:           fake_rescan,
    sampledup:        fake_sampledup,
    check_model:      fake_check_model,
    check_dictionary: fake_check_dictionary,
    set_model:        fake_set_model,
    set_dictionary:   fake_set_dictionary,
    };

    srs_context_t *srs = plugin->srs;
    fake_t        *fake;


    mrp_debug("creating fake speech recognition backend");

    fake = mrp_allocz(sizeof(*fake));

    if (fake != NULL) {
        fake->self = plugin;

        if (srs_register_srec(srs, FAKE_NAME, &fake_api, fake,
                              &fake->notify, &fake->notify_data) == 0) {
            plugin->plugin_data = fake;
            return TRUE;
        }
        else
            mrp_free(fake);
    }

    return FALSE;
}


static int config_fake(srs_plugin_t *plugin, srs_cfg_t *settings)
{
    srs_cfg_t *cfg;
    int        n, i;

    MRP_UNUSED(plugin);

    mrp_debug("configure fake plugin");

    if ((cfg = settings) != NULL) {
        while (cfg->key != NULL) {
            mrp_debug("got config setting: %s = %s", cfg->key,
                      cfg->value);
            cfg++;
        }
    }

    n = srs_collect_config(settings, "fake.", &cfg);
    mrp_debug("Found %d own configuration keys.", n);
    for (i = 0; i < n; i++)
        mrp_debug("    %s = %s", cfg[i].key, cfg[i].value);
    srs_free_config(cfg);

    return TRUE;
}


static int start_fake(srs_plugin_t *plugin)
{
    MRP_UNUSED(plugin);

    mrp_debug("start fake plugin");

    return TRUE;
}


static void stop_fake(srs_plugin_t *plugin)
{
    MRP_UNUSED(plugin);

    mrp_debug("stop fake plugin");

    return;
}


static void destroy_fake(srs_plugin_t *plugin)
{
    srs_context_t *srs = plugin->srs;
    fake_t        *fake = (fake_t *)plugin->plugin_data;

    mrp_debug("destroy fake plugin");

    if (fake != NULL) {
        srs_unregister_srec(srs, FAKE_NAME);
        mrp_free(fake);
    }
}


SRS_DECLARE_PLUGIN(FAKE_NAME, FAKE_DESCRIPTION, FAKE_AUTHORS, FAKE_VERSION,
                   create_fake, config_fake, start_fake, stop_fake,
                   destroy_fake)
