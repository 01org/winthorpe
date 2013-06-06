#ifndef __SRS_POCKET_SPHINX_OPTIONS_H__
#define __SRS_POCKET_SPHINX_OPTIONS_H__

#include "src/daemon/config.h"

#include "sphinx-plugin.h"

#define SPHINX_PREFIX "sphinx."

struct options_s {
    const char *hmm;
    const char *lm;
    const char *dict;
    const char *fsg;
    const char *srcnam;
    const char *audio;
    const char *logfn;
    uint32_t rate;
    uint32_t topn;
    double silen;
};


int options_create(context_t *ctx, int ncfg, srs_cfg_t *cfgs);
void options_destroy(context_t *ctx);

#endif /* __SRS_POCKET_SPHINX_H__ */

/*
 * Local Variables:
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 *
 */
