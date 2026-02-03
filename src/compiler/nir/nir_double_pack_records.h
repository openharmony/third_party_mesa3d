/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
 */

#ifndef NIR_DOUBLE_PACK_RECORDS_H
#define NIR_DOUBLE_PACK_RECORDS_H

#include "util/detect_os.h"
#include "nir.h"
#include "nir_builder.h"

struct nir_lower_double_def {
   nir_def* src;
   nir_def* dest;
   uint32_t index;
   bool packed;
};
struct lower_doubles_data {
   const nir_shader *softfp64;
   nir_lower_doubles_options options;
   struct set *defs;
};
uint32_t hash_nir_lower_double_def(const void *p);
bool nir_lower_double_def_equal(const void *void_a, const void *void_b);
void destroy_lower_double_def(struct set_entry *entry);
void add_lower_double_def(struct set* defs, struct nir_lower_double_def* key);
nir_def *convert_double_before_inline(nir_builder *b, nir_alu_src src, const struct lower_doubles_data *data,
   bool unpack);
nir_def *pack_double_before_lower_phi(nir_builder *b, nir_phi_instr *instr, const struct lower_doubles_data *data);
nir_def *pack_double_before_lower_alu(nir_builder *b, nir_alu_instr *instr, const struct lower_doubles_data *data);
bool should_lower_double_phi_instr(const nir_instr *instr, const nir_lower_doubles_options options);
bool is_quick_softfp64(const nir_shader *softfp64);
#endif // NIR_DOUBLE_PACK_RECORDS_H