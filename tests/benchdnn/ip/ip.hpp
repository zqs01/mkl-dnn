/*******************************************************************************
* Copyright 2017-2020 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef IP_HPP
#define IP_HPP

#include <iostream>

#include "dnnl.h"

#include "common.hpp"
#include "dnnl_common.hpp"
#include "dnnl_memory.hpp"
#include "perf_report.hpp"

namespace ip {

struct desc_t {
    int64_t mb, oc, ic, id, ih, iw;
    const char *name;
    int ndims;
};
int str2desc(desc_t *desc, const char *str);
std::ostream &operator<<(std::ostream &s, const desc_t &d);

typedef struct dt_conf_t {
    dnnl_data_type_t dt;
    double min, max; /* representative */
    double f_min, f_max; /* fill range */
    int f_base; /* fill base, use 0 */
    double f_sparsity; /* amount of non-zeros, default 0.25 */
    double f_scale; /* fill scale, scaling factor for integer generated data */
    double eps; /* acceptable error */
} _dt_conf_t[DAT_TOTAL];

extern const _dt_conf_t conf_f32;

struct settings_t {
    settings_t() = default;

    // ctor to save certain fields from resetting
    settings_t(const char *perf_template) : settings_t() {
        this->perf_template = perf_template;
    }

    desc_t desc;

    std::vector<dir_t> dir {FWD_B};
    std::vector<const dt_conf_t *> cfg {conf_f32};
    std::vector<std::string> stag {tag::any}, wtag {tag::any}, dtag {tag::any};
    std::vector<int64_t> mb {0};
    attr_t attr = {};
    bool allow_unimpl = false;

    const char *perf_template_csv
            = "perf,%engine%,%name%,%dir%,%cfg%,%attr%,%DESC%,"
              "%Gops%,%Gfreq%,%-time%,%-Gflops%,%0time%,%0Gflops%";
    const char *perf_template_def
            = "perf,%engine%,%name%,%prb%,"
              "%Gops%,%Gfreq%,%-time%,%-Gflops%,%0time%,%0Gflops%";
    const char *perf_template = perf_template_def;

    void reset() { *this = settings_t(perf_template); }
};

struct prb_t : public desc_t {
    prb_t(const desc_t &desc, int64_t mb, dir_t dir, const dt_conf_t *cfg,
            const std::string &stag, const std::string &wtag,
            const std::string &dtag, const attr_t &attr)
        : desc_t(desc)
        , dir(dir)
        , cfg(cfg)
        , stag(stag)
        , wtag(wtag)
        , dtag(dtag)
        , attr(attr)
        , ops(0)
        , scales(NULL) {
        if (mb) this->mb = mb;
        count_ops();
        generate_oscales();
    }
    ~prb_t() {
        if (scales) zfree(scales);
    }

    dir_t dir;
    const dt_conf_t *cfg;
    std::string stag, wtag, dtag;
    attr_t attr;

    double ops;
    float *scales;

    void count_ops() {
        if (ops > 0) return;
        ops = 2. * mb * ic * oc * id * ih * iw;
    };

    void generate_oscales();

    BENCHDNN_DISALLOW_COPY_AND_ASSIGN(prb_t);
};
std::ostream &operator<<(std::ostream &s, const prb_t &p);

const dt_conf_t *str2cfg(const char *str);
std::ostream &operator<<(std::ostream &s, const dt_conf_t *cfg);

struct perf_report_t : public base_perf_report_t {
    using base_perf_report_t::base_perf_report_t;

    void report(const prb_t *p, const res_t *r, const char *prb_str) {
        p_ = p;
        base_report(r, prb_str);
    }

    virtual void dump_cfg(std::ostream &s) const override { s << p_->cfg; }

    virtual void dump_desc(std::ostream &s) const override {
        s << static_cast<const desc_t &>(*p_);
    }

    virtual void dump_desc_csv(std::ostream &s) const override {
        s << p_->mb << ',' << p_->oc << ',' << p_->ic << ',' << p_->id << ','
          << p_->ih << ',' << p_->iw;
    }

    virtual double ops() const override { return p_->ops; }
    virtual const attr_t *attr() const override { return &p_->attr; }
    virtual const char *name() const override { return p_->name; }
    virtual const dir_t *dir() const override { return &p_->dir; }

private:
    const prb_t *p_ = NULL;
};

inline size_t src_off_f(const prb_t *p, int64_t mb, int64_t ic, int64_t id,
        int64_t ih, int64_t iw) {
    return (((mb * p->ic + ic) * p->id + id) * p->ih + ih) * p->iw + iw;
}

inline size_t wei_off_f(const prb_t *p, int64_t oc, int64_t ic, int64_t id,
        int64_t ih, int64_t iw) {
    return (((oc * p->ic + ic) * p->id + id) * p->ih + ih) * p->iw + iw;
}

inline size_t bia_off_f(const prb_t *p, int64_t oc) {
    return oc;
}

inline size_t dst_off_f(const prb_t *p, int64_t mb, int64_t oc) {
    return mb * p->oc + oc;
}

void compute_ref_fwd(const prb_t *p, dnn_mem_t &src_m, dnn_mem_t &wei_m,
        dnn_mem_t &bia_m, dnn_mem_t &dst_m);
void compute_ref_bwd_d(const prb_t *p, dnn_mem_t &diff_src_m, dnn_mem_t &wei_m,
        dnn_mem_t &diff_dst_m);
void compute_ref_bwd_w(const prb_t *p, dnn_mem_t &src_m, dnn_mem_t &diff_wei_m,
        dnn_mem_t &diff_bia_m, dnn_mem_t &diff_dst_m);

int doit(const prb_t *p, res_t *res);

int bench(int argc, char **argv);
} // namespace ip

#endif
