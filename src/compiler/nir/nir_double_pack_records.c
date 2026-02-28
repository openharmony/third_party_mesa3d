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

#include "nir_double_pack_records.h"
const unsigned int DOUBLE_BITS = 64;
#define HASH(hash, data) XXH32(&(data), sizeof(data), hash)

uint32_t hash_nir_lower_double_def(const void *p)
{
   const struct nir_lower_double_def* def = p;
   uint32_t hash = HASH(0, def->index);
   return HASH(hash, def->src);
}

bool nir_lower_double_def_equal(const void *void_a, const void *void_b)
{
   const struct nir_lower_double_def* src_a = void_a;
   const struct nir_lower_double_def* src_b = void_b;
   return src_a->src == src_b->src && src_a->index == src_b->index;
}

void destroy_lower_double_def(struct set_entry *entry)
{
   ralloc_free((void *)(entry->key));
}

static struct nir_lower_double_def *get_lower_double_def(struct set *defs, struct nir_lower_double_def *key)
{
   if (defs == NULL) {
      return NULL;
   }
   struct set_entry *entry = _mesa_set_search(defs, key);
   if (entry == NULL) {
      return NULL;
   }
   return (struct nir_lower_double_def *)(entry->key);
}

void add_lower_double_def(struct set* defs, struct nir_lower_double_def* key)
{
   if (defs == NULL) {
      return;
   }
   struct set_entry *entry = _mesa_set_search(defs, key);
   if (entry) {
      struct nir_lower_double_def *def = (struct nir_lower_double_def *)(entry->key);
      def->dest = key->dest;
   } else {
      _mesa_set_add(defs, key);
   }
}

nir_def *convert_double_before_inline(nir_builder *b, nir_alu_src src, const struct lower_doubles_data *data,
   bool unpack)
{
   if (src.src.ssa->bit_size != DOUBLE_BITS) {
      return NULL;
   }

   struct nir_lower_double_def *key = ralloc(b->shader, struct nir_lower_double_def);
   key->index = src.swizzle[0];
   key->src = src.src.ssa;
   key->dest = NULL;
   key->packed = unpack;
   struct nir_lower_double_def *dest_def = get_lower_double_def(data->defs, key);
   if ((dest_def && dest_def->packed == !unpack) || (dest_def == NULL && !unpack)) {
      ralloc_free(key); // no use override variable.
      return NULL;
   }
   if (dest_def && dest_def->dest) {
      ralloc_free(key); // use already generated variable.
      return dest_def->dest;
   }

   const char *name = unpack ? "__unpackFp64ToFp32" : "__packFp32ToFp64";
   const nir_shader *softfp64 = data->softfp64;
   nir_function *func = nir_shader_get_function_for_name(softfp64, name);
   if (!func || !func->impl) {
      ralloc_free(key);
      return NULL;
   }

   bool adj_cursor =  (key->src->parent_instr
      && key->src->parent_instr->type != nir_instr_type_phi);
   nir_cursor curr_cursor = b->cursor;
   if (adj_cursor) {
      b->cursor = nir_after_instr(key->src->parent_instr);
   }

   nir_def *params[2] = {NULL, NULL}; // 2: two defs
   const struct glsl_type *param_type = glsl_uint64_t_type();
   nir_variable *ret_tmp =
      nir_local_variable_create(b->impl, param_type, unpack ? "return_unpack" : "return_pack");
   nir_deref_instr *ret_deref = nir_build_deref_var(b, ret_tmp);
   params[0] = &ret_deref->def;

   nir_variable *param =
      nir_local_variable_create(b->impl, param_type, unpack ? "param_unpack" : "param_pack");
   nir_deref_instr *param_deref = nir_build_deref_var(b, param);
   nir_store_deref(b, param_deref, nir_mov_alu(b, src, 1), ~0);
   params[1] = &param_deref->def;

   nir_inline_function_impl(b, func->impl, params, NULL);

   key->dest = nir_load_deref(b, ret_deref);
   add_lower_double_def(data->defs, key);

   if (adj_cursor) {
      b->cursor = curr_cursor;
   }
   return key->dest;
}

nir_def *pack_double_before_lower_phi(nir_builder *b, nir_phi_instr *instr, const struct lower_doubles_data *data)
{
   nir_foreach_phi_src(src, instr) {
      nir_alu_src alu_src = {NIR_SRC_INIT};
      alu_src.src = src->src;
      alu_src.swizzle[0] = 0;
      nir_def* new_def = convert_double_before_inline(b, alu_src, data, false);
      if (new_def) {
         nir_src_rewrite(&src->src, new_def);
      }
   }
   return NULL;
}

nir_def *pack_double_before_lower_alu(nir_builder *b, nir_alu_instr *instr, const struct lower_doubles_data *data)
{
   const uint8_t num_inputs = nir_op_infos[instr->op].num_inputs;
   for (unsigned i = 0; i < num_inputs; i++) {
      nir_def* new_def = convert_double_before_inline(b, instr->src[i], data, false);
      if (new_def != NULL) {
         nir_src_rewrite(&(instr->src[i].src), new_def);
         for (int j = 0; j < NIR_MAX_VEC_COMPONENTS; ++j) {
            instr->src[i].swizzle[j] = 0;
         }
      }
   }
   return NULL;
}

bool should_lower_double_phi_instr(const nir_instr *instr, const nir_lower_doubles_options options)
{
   if (instr->type != nir_instr_type_phi)
      return false;

   const nir_phi_instr *phi = nir_instr_as_phi(instr);
   bool is_64 = phi->def.bit_size == DOUBLE_BITS;

   nir_foreach_phi_src(src, phi) {
      is_64 |= (nir_src_bit_size(src->src) == DOUBLE_BITS);
   }

   return is_64 && (options & nir_lower_fp64_full_software);
}

bool is_quick_softfp64(const nir_shader *softfp64)
{
   if (softfp64 && softfp64->info.label && strcmp(softfp64->info.label, "float64q") == 0) {
      return true;
   }
   return false;
}
