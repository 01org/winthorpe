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

#ifndef __SRS_DAEMON_RECOGNIZER_H__
#define __SRS_DAEMON_RECOGNIZER_H__

/** Type for tokens recognized by a speech recognition backend. */
typedef struct srs_srec_utterance_s srs_srec_utterance_t;

/** Type for a backend recognition notification callback. */
typedef int (*srs_srec_notify_t)(srs_srec_utterance_t *utt, void *notify_data);

/** Notification callback return value for flushing the full audio buffer. */
#define SRS_SREC_FLUSH_ALL -1

/*
 * API to a speech recognition backend.
 */
typedef struct {
    /** Activate speech recognition. */
    int (*activate)(void *user_data);
    /** Deactivate speech recognition. */
    void (*deactivate)(void *user_data);
    /** Flush part or whole of the audio buffer. */
    int (*flush)(uint32_t start, uint32_t end, void *user_data);
    /** Schedule a rescan of the given portion of the audio buffer. */
    int (*rescan)(uint32_t start, uint32_t end, void *user_data);
    /** Get a copy of the audio samples in the buffer. */
    void *(*sampledup)(uint32_t start, uint32_t end, void *user_data);
    /** Check if the given language model exists/is usable. */
    int (*check_decoder)(const char *decoder, void *user_data);
    /** Set language model to be used. */
    int (*select_decoder)(const char *decoder, void *user_data);
} srs_srec_api_t;

/*
 * a single speech token
 */
typedef struct {
    char     *token;                     /* recognized tokens */
    double    score;                     /* correctness probability */
    uint32_t  start;                     /* start in audio buffer */
    uint32_t  end;                       /* end in audio buffer */
} srs_srec_token_t;

/*
 * a single candidate (essentially a set of speech tokens)
 */
typedef struct {
    double            score;             /* overall candidate quality score */
    size_t            ntoken;            /* number of tokens in candidate */
    srs_srec_token_t *tokens;            /* actual tokens of this candidate */
} srs_srec_candidate_t;

/*
 * an utterance (candidates for a silence-terminated audio sequence)
 */
struct srs_srec_utterance_s {
    const char            *id;           /* backend ID for this utterance */
    double                 score;        /* overall quality score */
    uint32_t               length;       /* length in the audio buffer */
    size_t                 ncand;        /* number of candidates */
    srs_srec_candidate_t **cands;        /* actual candidates */
};

/** Register a speech recognition backend. */
int srs_register_srec(srs_context_t *srs, const char *name,
                      srs_srec_api_t *api, void *api_data,
                      srs_srec_notify_t *notify, void **notify_data);

/** Unregister a speech recognition backend. */
void srs_unregister_srec(srs_context_t *srs, const char *name);

/** Activate speech recognition using the specified backend. */
int srs_activate_srec(srs_context_t *srs, const char *name);

/** Deactivate the specified speech recognition backend. */
void srs_deactivate_srec(srs_context_t *srs, const char *name);

/** Check if a decoder (model/dictionary combination) exists for a backend. */
int srs_check_decoder(srs_context_t *srs, const char *name,
                      const char *decoder);

/** Select a decoder for a backend. */
int srs_set_decoder(srs_context_t *srs, const char *name, const char *decoder);

#endif /* __SRS_DAEMON_RECOGNIZER_H__ */
