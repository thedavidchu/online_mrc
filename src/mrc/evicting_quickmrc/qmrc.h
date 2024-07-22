#pragma once

struct qmrc *qmrc_init(int log_hist_buckets, int log_qmrc_buckets,
		       int log_epoch_limit);
int qmrc_lookup(struct qmrc *qmrc, int epoch);
int qmrc_insert(struct qmrc *qmrc);
void qmrc_delete(struct qmrc *qmrc, int epoch);
void qmrc_output(struct qmrc *qmrc);
void qmrc_destroy(struct qmrc *qmrc);
