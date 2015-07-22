/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2015 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

/*
 * Output Formatter prototypes
 *
 * Joerg Steffens, April 2015
 */

#ifndef __OUTPUT_FORMATTER_H_
#define __OUTPUT_FORMATTER_H_

#if HAVE_JANSSON
#define UA_JSON_FLAGS JSON_INDENT(2)

/*
 * See if the source file needs the full JANSSON namespace or that we can
 * get away with using a forward declaration of the json_t struct.
 */
#ifndef NEED_JANSSON_NAMESPACE
typedef struct json_t json_t;
#else
#include <jansson.h>
#endif
#endif

class OUTPUT_FORMATTER : public SMARTALLOC {
public:
   typedef void (SEND_HANDLER)(void *, const char *);

   OUTPUT_FORMATTER(SEND_HANDLER *send_func, void *send_ctx, int api_mode = API_MODE_OFF);
   ~OUTPUT_FORMATTER();

   void set_mode(int mode) { api = mode; };
   int  get_mode() { return api; };

   void object_start(const char *name = NULL);
   void object_end(const char *name = NULL);
   void decoration(const char *fmt, ...);
   void object_key_value(const char *key, uint64_t value);
   void object_key_value(const char *key, uint64_t value, const char *value_fmt);
   void object_key_value(const char *key, const char *key_fmt, uint64_t value, const char *value_fmt);
   void object_key_value(const char *key, const char *value, int wrap = -1);
   void object_key_value(const char *key, const char *value, const char *value_fmt, int wrap = -1);
   void object_key_value(const char *key, const char *key_fmt, const char *value, const char *value_fmt, int wrap = -1);

   void finalize_result(bool result);

#if HAVE_JANSSON
   void json_add_result(json_t *json);
   void json_key_value_add(const char *key, uint64_t value);
   void json_key_value_add(const char *key, const char *value);
   void json_finalize_result(bool result);
#endif

private:
   int api;
   SEND_HANDLER *send_func;
   void *send_ctx;
   POOL_MEM *result_message_plain;

   /*
    * reformat string.
    * remove newlines and replace tabs with a single space.
    * wrap < 0: no modification
    * wrap = 0: reformat to single line
    * wrap > 0: if api==0: wrap after x characters, else no modifications
    */
   void rewrap(POOL_MEM &string, int wrap);

   void process_text_buffer();

#if HAVE_JANSSON
   json_t *result_array_json;
   alist *result_stack_json;
#endif
};

#endif
