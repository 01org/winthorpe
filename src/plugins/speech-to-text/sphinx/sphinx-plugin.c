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

#include <pulse/pulseaudio.h>
#include <pulse/mainloop.h>

#include "options.h"
#include "decoder-set.h"
#include "utterance.h"
#include "filter-buffer.h"
#include "input-buffer.h"
#include "pulse-interface.h"


#define SPHINX_NAME        "sphinx-speech"
#define SPHINX_DESCRIPTION "A CMU Sphinx-based speech engine backend plugin."
#define SPHINX_AUTHORS     "Janos Kovacs <janos.kovacs@intel.com>"
#define SPHINX_VERSION     "0.0.1"

struct plugin_s {
    srs_plugin_t *self;               /* us, the backend plugin */
    struct {
        srs_srec_notify_t callback;   /* recognition notification callback */
        void *data;                   /* notifiation callback data */
    } notify;
};


mrp_mainloop_t *plugin_get_mainloop(plugin_t *plugin)
{
    return plugin->self->srs->ml;
}


int32_t plugin_utterance_handler(context_t *ctx, srs_srec_utterance_t *utt)
{
    plugin_t *pl;
    srs_srec_notify_t notify;
    int32_t length;

    if (!(pl = ctx->plugin) || !(notify = pl->notify.callback))
        length = -1;
    else {
        length = notify(utt, pl->notify.data);
        mrp_log_info("buffer processed till %d", length);
    }

    return length;
}

static int activate(void *user_data)
{
    context_t *ctx = (context_t *)user_data;

    MRP_UNUSED(ctx);

    mrp_log_info("Activating CMU Sphinx backend.");

    pulse_interface_cork_input_stream(ctx, false);

    return TRUE;
}


static void deactivate(void *user_data)
{
    context_t *ctx = (context_t *)user_data;

    MRP_UNUSED(ctx);

    mrp_log_info("Deactivating CMU Sphinx backend.");

    pulse_interface_cork_input_stream(ctx, true);
    filter_buffer_purge(ctx, -1);
    input_buffer_purge(ctx);
}


static int flush(uint32_t start, uint32_t end, void *user_data)
{
    context_t *ctx = (context_t *)user_data;

    MRP_UNUSED(ctx);

    mrp_log_info("flushing CMU Sphinx backend buffer (%u - %u)", start, end);

    return TRUE;
}


static int rescan(uint32_t start, uint32_t end, void *user_data)
{
    context_t *ctx = (context_t *)user_data;

    MRP_UNUSED(ctx);

    mrp_log_info("scheduling CMU Sphinx backend buffer rescan (%u - %u)",
              start, end);

    return TRUE;
}


static srs_audiobuf_t *sampledup(uint32_t start, uint32_t end, void *user_data)
{
    context_t *ctx  = (context_t *)user_data;
    options_t *opts;
    srs_audioformat_t format;
    uint32_t rate;
    uint8_t channels;
    size_t  samples;
    int16_t *buf;

    if (!ctx || !(opts = ctx->opts))
        return NULL;

    mrp_debug("duplicating CMU Sphinx backend sample (%u - %u)", start, end);

    format = SRS_AUDIO_S16LE;
    rate = opts->rate;
    channels = 1;
    buf = filter_buffer_dup(ctx, start, end, &samples);

    return srs_create_audiobuf(format, rate, channels, samples, buf);
}


static int check_decoder(const char *decoder, void *user_data)
{
    context_t *ctx = (context_t *)user_data;
    int available;

    mrp_log_info("checking availability of decoder '%s' for CMU Sphinx backend",
              decoder);

    available = decoder_set_contains(ctx, decoder);

    mrp_debug("decoder %s %savailable", decoder, available ? "" : "un");

    return available;
}


static int select_decoder(const char *decoder, void *user_data)
{
    context_t *ctx = (context_t *)user_data;

    mrp_log_info("selecting decoder '%s' for CMU Sphinx backend", decoder);

    if (decoder_set_use(ctx, decoder) < 0)
        return FALSE;

    return TRUE;
}


static const char *active_decoder(void *user_data)
{
    context_t *ctx = (context_t *)user_data;
    const char *decoder;

    mrp_log_info("querying active CMU Sphinx backend decoder");

    decoder = decoder_set_name(ctx);

    mrp_debug("active decoder is '%s'", decoder);

    return decoder;
}


static int create_sphinx(srs_plugin_t *plugin)
{
    srs_srec_api_t api = {
        activate:         activate,
        deactivate:       deactivate,
        flush:            flush,
        rescan:           rescan,
        sampledup:        sampledup,
        check_decoder:    check_decoder,
        select_decoder:   select_decoder,
        active_decoder:   active_decoder,
    };

    srs_context_t *srs = plugin->srs;
    context_t     *ctx = NULL;
    plugin_t      *pl = NULL;
    int            sts;

    mrp_debug("creating CMU Sphinx speech recognition backend plugin");

    if ((ctx = mrp_allocz(sizeof(context_t))) &&
        (pl  = mrp_allocz(sizeof(plugin_t)))   )
    {
        ctx->plugin = pl;

        pl->self = plugin;

        sts = srs_register_srec(srs, SPHINX_NAME, &api, ctx,
                                &pl->notify.callback,
                                &pl->notify.data);
        if (sts == 0) {
            plugin->plugin_data = ctx;
            return TRUE;
        }
    }

    mrp_free(pl);
    mrp_free(ctx);

    mrp_log_error("Failed to create CMU Sphinx plugin.");

    return FALSE;
}


static int config_sphinx(srs_plugin_t *plugin, srs_cfg_t *settings)
{
    context_t *ctx = (context_t *)plugin->plugin_data;
    srs_cfg_t *cfg;
    int        n;

    mrp_debug("configuring CMU Sphinx speech recognition backend plugin");

    n = srs_config_collect(settings, SPHINX_PREFIX, &cfg);

    mrp_log_info("Found %d CMU Sphinx plugin configuration keys.", n);

    if (options_create(ctx, n, cfg) < 0 ||
        decoder_set_create(ctx)     < 0 ||
        filter_buffer_create(ctx)   < 0 ||
        input_buffer_create(ctx)    < 0  )
    {
        mrp_log_error("Failed to configure CMU Sphinx plugin.");
        return FALSE;
    }

    srs_config_free(cfg);

    return TRUE;
}


static int start_sphinx(srs_plugin_t *plugin)
{
    srs_context_t *srs = plugin->srs;
    context_t *ctx = (context_t *)plugin->plugin_data;

    mrp_debug("start CMU Sphinx speech recognition backend plugin");

    if (pulse_interface_create(ctx, srs->pa) < 0) {
        mrp_log_error("Failed to start CMU Sphinx plugin: can't create "
                      "pulseaudio interface");
    }

    return TRUE;
}


static void stop_sphinx(srs_plugin_t *plugin)
{
    context_t *ctx = (context_t *)plugin->plugin_data;

    mrp_debug("stop CMU Sphinx speech recognition backend plugin");

    pulse_interface_destroy(ctx);
}


static void destroy_sphinx(srs_plugin_t *plugin)
{
    srs_context_t *srs = plugin->srs;
    context_t     *ctx = (context_t *)plugin->plugin_data;

    mrp_debug("destroy CMU Sphinx speech recognition backend plugin");

    if (ctx != NULL) {
        srs_unregister_srec(srs, SPHINX_NAME);
        mrp_free(ctx->plugin);

        input_buffer_destroy(ctx);
        filter_buffer_destroy(ctx);
        decoder_set_destroy(ctx);
        options_destroy(ctx);

        mrp_free(ctx);
    }
}


SRS_DECLARE_PLUGIN(SPHINX_NAME, SPHINX_DESCRIPTION, SPHINX_AUTHORS,
                   SPHINX_VERSION, create_sphinx, config_sphinx,
                   start_sphinx, stop_sphinx, destroy_sphinx)


/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
