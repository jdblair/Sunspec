/* -*- tab-width: 4; indent-tabs-mode: nil -*- */

/*
 * suns_model.c
 * $Id: $
 *
 * Functions for constructing and interacting with sunspec models.
 * This includes all the data structures used for every component of the
 * abstract internal representation.
 *
 * Copyright (c) 2011, John D. Blair <jdb@moship.net>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of John D. Blair nor his lackeys may be used
 *       to endorse or promote products derived from this software
 *       without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * JOHN D. BLAIR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "suns_model.h"
#include "trx/debug.h"
#include "trx/macros.h"


suns_model_t *suns_model_new()
{
    suns_model_t *m = malloc(sizeof(suns_model_t));
    if (m == NULL) {
	debug("malloc() failed");
	return NULL;
    }

    m->defines = list_new();
    m->did_list = list_new();
    m->dp_blocks = list_new();
    m->len = 0;
    m->base_len = 0;

    return m;
}


char * suns_type_string(suns_type_t type)
{
    /* these must be in the same order as the enum definition
       in suns_model.h!! */
    char *type_table[] = {
	"null",
	"int16",
	"uint16",
	"acc16",
	"int32",
	"uint32",
	"float32",
	"acc32",
	"int64",
	"uint64",
	"float64",
	"enum16",
	"bitfield16",
	"bitfield32",
	"sunssf",
	"string",
	"undef",
	NULL
    };

    if ((type > SUNS_UNDEF) ||
	(type < SUNS_NULL)) {
	return NULL;
    }

    return type_table[type];
}


char * suns_value_meta_string(suns_value_meta_t meta)
{
    char *meta_table[] = {
	"null",
	"ok",
	"not implemented",
	"error",
	"undef",
	NULL,
    };

    if ((meta > SUNS_VALUE_UNDEF) ||
	(meta < SUNS_VALUE_NULL)) {
	return NULL;
    }

    return meta_table[meta];
}


suns_type_t suns_type_from_name(char *name)
{
    int i;

    typedef struct suns_type_name_map {
	char *name;
	suns_type_t type;
    } suns_type_name_map_t;

    suns_type_name_map_t name_map[] = {
	{ "null",       SUNS_NULL },
	{ "int16",      SUNS_INT16 },
	{ "uint16",     SUNS_UINT16 },
	{ "acc16",      SUNS_ACC16 },
	{ "int32",      SUNS_INT32 },
	{ "uint32",     SUNS_UINT32 },
	{ "float32",    SUNS_FLOAT32 },
	{ "acc32",      SUNS_ACC32 },
	{ "int64",      SUNS_INT64 },
	{ "uint64",     SUNS_UINT64 },
	{ "float64",    SUNS_FLOAT64 },
	{ "enum16",     SUNS_ENUM16 },
	{ "bitfield16", SUNS_BITFIELD16 },
	{ "bitfield32", SUNS_BITFIELD32 },
	{ "sunssf",     SUNS_SF },
	{ "string",     SUNS_STRING },
	{ "undef",      SUNS_UNDEF },
	{ NULL,         -1 },
    };

    for (i = 0; name_map[i].name != NULL; i++) {
	if (strcmp(name_map[i].name, name) == 0) {
	    return name_map[i].type;
	}
    }

    /* if we're here the type was not found */
    return SUNS_UNDEF;
}


suns_type_pair_t *suns_type_pair_new(suns_type_t type)
{
    suns_type_pair_t *type_pair;

    type_pair = malloc(sizeof(suns_type_pair_t));

    if (type_pair == NULL)  /* just in case */
	return NULL;

    return type_pair;
}


void suns_model_free(suns_model_t *model)
{
    list_free(model->dp_blocks, (list_free_data_f)suns_model_dp_block_free);
    free(model);
}


void suns_model_dp_block_free(suns_dp_block_t *dp_block)
{
    list_free(dp_block->dp_list, free);
    free(dp_block);
}


suns_data_t *suns_data_new(void)
{
    suns_data_t *block = malloc(sizeof(suns_data_t));
    if (block == NULL) {
	error("malloc() returned NULL");
	return NULL;
    }

    block->data = buffer_new(BUFFER_SIZE);
    
    return block;
}


void suns_data_free(suns_data_t *block)
{
    if (block->data)
	buffer_free(block->data);

    free(block);
}


suns_data_block_t * suns_data_block_new(void)
{
    suns_data_block_t *new;

    new = malloc(sizeof(suns_data_block_t));
    if (new == NULL)
	return NULL;

    new->data = buffer_new(BIG_BUFFER_SIZE);

    return new;
}


void suns_data_block_free(suns_data_block_t *block)
{
    buffer_free(block->data);
    free(block);
}
    

/* return the size in bytes of a specified suns_type_t */
int suns_type_size(suns_type_t type)
{
    /* must be in the same order as the enum in suns_model.h!! */
    static int sizes[] = {
	0, /* SUNS_NULL */
	2, /* SUNS_INT16 */
	2, /* SUNS_UINT16 */
	2, /* SUNS_ACC16 */
	4, /* SUNS_INT32 */
	4, /* SUNS_UINT32 */
	4, /* SUNS_FLOAT32 */
	4, /* SUNS_ACC32 */
	8, /* SUNS_INT64 */
	8, /* SUNS_UINT64 */
	8, /* SUNS_FLOAT64 */
	2, /* SUNS_ENUM16 */
	2, /* SUNS_BITFIELD16 */
	4, /* SUNS_BITFIELD32 */
	2, /* SUNS_SF */
	0, /* SUNS_STRING */
	0, /* SUNS_UNDEF */
    };

    if ((type > SUNS_UNDEF) ||
	(type < SUNS_NULL)) {
	debug("invalid type %d passed to suns_type_size()", type);
	return -1;
    }

    return sizes[type];
}

/* like suns_type_size(), but acts on a suns_type_pair_t *
   this means can return the length of strings */
int suns_type_pair_size(suns_type_pair_t *tp)
{
    if (tp->type == SUNS_STRING) {
	return tp->sub.len;
    }

    return suns_type_size(tp->type);
}


/* now to write functions which convert between suns_value_t and
   binary buffers */

int suns_value_to_buf(suns_value_t *v, unsigned char *buf, size_t len)
{
    /* uint16_t tmp_u16;
       uint32_t tmp_u32; */

    switch (v->tp.type) {

	/* 16 bit datatypes */
    case SUNS_INT16:
    case SUNS_UINT16:
    case SUNS_ACC16:
    case SUNS_ENUM16:
    case SUNS_BITFIELD16:
    case SUNS_SF:
	if (len < 2) {
	    debug("not enough space for 16 bit conversion "
		  "(type = %s,  len = %d)", suns_type_string(v->tp.type), len);
	}
	/* we can safely treat all these values as uint for this purpose */
	*((uint16_t *) buf) = htobe16(v->value.u16);
	break;
		/* 32 bit datatypes */
    case SUNS_INT32:
    case SUNS_UINT32:
    case SUNS_FLOAT32:
    case SUNS_ACC32:
    case SUNS_BITFIELD32:
	if (len < 4) {
	    debug("not enough space for 32 bit conversion "
		  "(type = %s,  len = %d)", suns_type_string(v->tp.type), len);
	}
	/* we can safely treat all these values as uint for this purpose */
	*((uint32_t *) buf) = htobe32(v->value.u32);
	break;

	/* strings */
    case SUNS_STRING:
	if (len < v->tp.sub.len) {
	    debug("not enough space for string(%d) (type = %s,  len = %d)",
		  v->tp.sub.len, suns_type_string(v->tp.type), len);
	}
	strncpy((char *) buf, v->value.s, v->tp.sub.len);  /* don't allow string to
						 overrun its defined size */
	/* buf[v->tp.sub.len - 1] = '\0';  */          /* make sure the string is
						 always NULL terminated */
	break;
    
    default:
	/* this means we hit an unsupported datatype or SUNS_UNDEF */
	debug("unsupported datatype %s", suns_type_string(v->tp.type));
	return -2;
    }

    return 0;
}


/* writes to a buffer_t, not an ordinary binary buffer
   returns the number of bytes added to the buffer */
int suns_value_to_buffer(buffer_t *buf, suns_value_t *v)
{
    int rc;
    int size;

    if (v->tp.type == SUNS_STRING) {
	size = v->tp.sub.len;
    } else {
	size = suns_type_size(v->tp.type);
	if (size == 0) {
	    debug("unsupported type %s(%d)",
		  suns_type_string(v->tp.type), v->tp.type);
	}
    }

    /* enough space? */
    if (buffer_space(buf) < size) {
	/* should we resize instead of just bailing out? */
	return 0;
    }

    /* write the data to the buffer */
    rc =  suns_value_to_buf(v, (unsigned char *) buf->in,
			    buffer_space(buf));

    debug("buf->in = %p, size = %d", buf->in, size);
    buf->in += size;

    return rc;
}



int suns_buf_to_value(unsigned char *buf,
		      suns_type_pair_t *tp,
		      suns_value_t *v)
{
    switch (tp->type) {
	
	/* 16 bit datatypes */
    case SUNS_INT16:
    case SUNS_UINT16:
    case SUNS_ACC16:
    case SUNS_ENUM16:
    case SUNS_BITFIELD16:
    case SUNS_SF:
	v->value.u16 = be16toh(*((uint16_t *)buf));
	break;
	
	/* 32 bit datatypes */
    case SUNS_INT32:
    case SUNS_UINT32:
    case SUNS_FLOAT32:
    case SUNS_ACC32:
    case SUNS_BITFIELD32:
	v->value.u32 = be32toh(*((uint32_t *)buf));
	break;
	
	/* strings */
    case SUNS_STRING:
	/* same as malloc() if tp->s == NULL */
	v->value.s = realloc(v->value.s, tp->sub.len + 1);
	if (v->value.s == NULL) {
	    /* uh oh */
	    debug("malloc() returned NULL!");
	    error("memory allocation failure");
	    return -1;
	}

	/* don't assume the string will be NULL terminated */
	v->value.s[tp->sub.len] = '\0';

	/* copy out of the buffer */
	strncpy(v->value.s, (char *) buf, tp->sub.len);

	break;
    
    default:
	/* this means we hit an unsupported datatype or SUNS_UNDEF */
	debug("unsupported datatype %s(%d)",
	      suns_type_string(tp->type), tp->type);
	return -2;
    }    

    /* now check for "not implemented" value */
    if ((tp->type == SUNS_INT16   && v->value.i16 == (int16_t)  0x8000)     ||
	(tp->type == SUNS_UINT16  && v->value.u16 == (uint16_t) 0xFFFF)     ||
	(tp->type == SUNS_SF      && v->value.i16 == (int16_t)  0x8000)     ||
	(tp->type == SUNS_INT32   && v->value.i32 == (int32_t)  0x80000000) ||
	(tp->type == SUNS_UINT32  && v->value.i32 == (uint32_t) 0xFFFFFFFF) ||
	(tp->type == SUNS_FLOAT32 && isnan(v->value.f32))) {
	v->meta = SUNS_VALUE_NOT_IMPLEMENTED;
    } else {
	/* mark the value as valid */
	v->meta = SUNS_VALUE_OK;
    }

    /* set the value to the specified type */
    v->tp = *tp;

    memcpy(v->raw, buf, min(16, suns_type_size(tp->type)));
    
    return 0;
}



suns_value_t *suns_value_new(void)
{
    suns_value_t *v = malloc(sizeof(suns_value_t));

    if (v == NULL) {
	/* uh oh */
	debug("malloc() returned NULL!");
	error("memory allocation failure");
	return NULL;
    }

    suns_value_init(v);

    return v;
}


/* use void pointer to have same function signature as free() */
void suns_value_free(void *v)
{
    assert(v != NULL);

    if (((suns_value_t *)v)->tp.type == SUNS_STRING) {
	if (((suns_value_t *)v)->value.s) {
	    free(((suns_value_t *)v)->value.s);
	    ((suns_value_t *)v)->value.s = NULL;
	}
    }

    free(v);
}


int suns_snprintf_value(char *str, size_t size, suns_value_t *v)
{
    int len = 0;

    debug_dump_buffer(v->raw, min(16, suns_type_size(v->tp.type)));

    debug("v->meta = %s", suns_value_meta_string(v->meta));

    /* check for meta condition */
    if (v->meta != SUNS_VALUE_OK) {
	len += snprintf(str + len, size - len,
			"%s", suns_value_meta_string(v->meta));
	return len;
    }

    /* convert value to string */
    switch (v->tp.type) {
    case SUNS_NULL:
	len += snprintf(str + len, size - len, "null type");
	break;
    case SUNS_UNDEF:
	len += snprintf(str + len, size - len, "undef type");
	break;
    case SUNS_INT16:
    case SUNS_SF:
	len += snprintf(str + len, size - len, "%d", v->value.i16);
	break;
    case SUNS_INT32:
	len += snprintf(str + len, size - len, "%d", v->value.i32);
	break;
    case SUNS_ENUM16:
    case SUNS_UINT16:
    case SUNS_ACC16:
	len += snprintf(str + len, size - len, "%u", v->value.u16);
	break;
    case SUNS_UINT32:
    case SUNS_ACC32:
	len += snprintf(str + len, size - len, "%u", v->value.u32);
	break;
    case SUNS_FLOAT32:
	len += snprintf(str + len, size - len, "%f", v->value.f32);
	break;
    case SUNS_BITFIELD16:
	len += snprintf(str + len, size - len, "0x%04x", v->value.u16);
	break;
    case SUNS_BITFIELD32:
	len += snprintf(str + len, size - len, "0x%08x", v->value.u32);
	break;
    case SUNS_STRING:
	len += snprintf(str + len, size - len, "%s", v->value.s);
	break;
    default:
	len += snprintf(str + len, size - len,
			" unknown type %2d", v->tp.type);
    }

    return len;
}


/* int suns_sql_value_snprintf( */


suns_dataset_t *suns_dataset_new(suns_model_did_t *did)
{
    assert(did);

    suns_dataset_t *d;

    d = malloc(sizeof(suns_dataset_t));
    if (d == NULL) {
	debug("malloc() failed");
	return NULL;
    }
	
    d->did = did;
    d->values = list_new();

    return d;
}


void suns_dataset_free(suns_dataset_t *d)
{
    assert(d);

    list_free(d->values, suns_value_free);
    free(d);
}



/* "accessors" for suns_value_t
   
   these are implemented as functions even though macros would be more
   efficient to force the compiler to do type checking on the values
   that are passed back and forth

   other than assert() no checks are made on the actual stored type
*/

void suns_value_init(suns_value_t *v)
{
    assert(v != NULL);
    memset(v, 0, sizeof(suns_value_t));
}


void suns_value_set_null(suns_value_t *v)
{
    assert(v != NULL);
    memset(v, 0, sizeof(suns_value_t));
}

    

void suns_value_set_uint16(suns_value_t *v, uint16_t u16)
{
    assert(v != NULL);

    v->value.u16 = u16;
    v->tp.type = SUNS_UINT16;
    if (u16 == (uint16_t) 0xFFFF) {
	v->meta = SUNS_VALUE_NOT_IMPLEMENTED;
    } else {
	v->meta = SUNS_VALUE_OK;
    }
}

uint16_t suns_value_get_uint16(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_UINT16);

    return v->value.u16;
}


void suns_value_set_int16(suns_value_t *v, int16_t i16)
{
    assert(v != NULL);

    v->value.i16 = i16;
    v->tp.type = SUNS_INT16;
    /* must cast 0x8000 to int16_t, otherwise the compiler
       interprets the value as a plain int (int32_t on i386) */
    if (i16 == (int16_t) 0x8000) {
	v->meta = SUNS_VALUE_NOT_IMPLEMENTED;
    } else {
	v->meta = SUNS_VALUE_OK;
    }
}

int16_t suns_value_get_int16(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_INT16);

    return v->value.i16;
}


void suns_value_set_acc16(suns_value_t *v, uint16_t u16)
{
    assert(v != NULL);

    v->value.u16 = u16;
    v->tp.type = SUNS_ACC16;
    v->meta = SUNS_VALUE_OK;
}

uint16_t suns_value_get_acc16(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_ACC16);

    return v->value.u16;
}


void suns_value_set_uint32(suns_value_t *v, uint32_t u32)
{
    assert(v != NULL);

    v->value.u32 = u32;
    v->tp.type = SUNS_UINT32;
    if (u32 == (uint32_t) 0xFFFFFFFF) {
	v->meta = SUNS_VALUE_NOT_IMPLEMENTED;
    } else {
	v->meta = SUNS_VALUE_OK;
    }
}

uint32_t suns_value_get_uint32(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_UINT32);

    return v->value.u32;
}


void suns_value_set_int32(suns_value_t *v, int32_t i32)
{
    assert(v != NULL);

    v->value.i32 = i32;
    v->tp.type = SUNS_INT32;
    if (i32 == (int32_t) 0x80000000) {
	v->meta = SUNS_VALUE_NOT_IMPLEMENTED;
    } else {
	v->meta = SUNS_VALUE_OK;
    }
}

int32_t suns_value_get_int32(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_UINT32);

    return v->value.i32;
}


void suns_value_set_acc32(suns_value_t *v, uint32_t u32)
{
    assert(v != NULL);

    v->value.u32 = u32;
    v->tp.type = SUNS_ACC32;
    v->meta = SUNS_VALUE_OK;
}

uint32_t suns_value_get_acc32(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_ACC32);

    return v->value.u32;
}


void suns_value_set_float32(suns_value_t *v, float f32)
{
    assert(v != NULL);

    v->value.f32 = f32;
    v->tp.type = SUNS_FLOAT32;
    if (isnan(f32)) {
	v->meta = SUNS_VALUE_NOT_IMPLEMENTED;
    } else {
	v->meta = SUNS_VALUE_OK;
    }
}

float suns_value_get_float32(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_FLOAT32);

    return v->value.f32;
}


void suns_value_set_enum16(suns_value_t *v, uint16_t u16)
{
    assert(v != NULL);

    v->value.u16 = u16;
    v->tp.type = SUNS_ENUM16;
    v->meta = SUNS_VALUE_OK;
}

uint16_t suns_value_get_enum16(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_ENUM16);

    return v->value.u16;
}

void suns_value_set_bitfield16(suns_value_t *v, uint16_t u16)
{
    assert(v != NULL);

    v->value.u16 = u16;
    v->tp.type = SUNS_BITFIELD16;
    v->meta = SUNS_VALUE_OK;
}

uint16_t suns_value_get_bitfield16(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_BITFIELD16);

    return v->value.u16;
}

void suns_value_set_bitfield32(suns_value_t *v, uint32_t u32)
{
    assert(v != NULL);

    v->value.u32 = u32;
    v->tp.type = SUNS_BITFIELD32;
    v->meta = SUNS_VALUE_OK;
}

uint32_t suns_value_get_bitfield32(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_BITFIELD32);

    return v->value.u32;
}

void suns_value_set_sunssf(suns_value_t *v, uint16_t u16)
{
    assert(v != NULL);

    v->value.u16 = u16;
    v->tp.type = SUNS_SF;
    v->meta = SUNS_VALUE_OK;
}

uint16_t suns_value_get_sunssf(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_SUNSSF);

    return v->value.u16;
}


void suns_value_set_string(suns_value_t *v, char *string, size_t len)
{
    assert(v != NULL);
    assert(string);

    /* we may be re-casting this suns_value_t from some
       other value.  if so, we need to ignore v->value since
       it doesn't represent a string pointer */
    if (v->tp.type != SUNS_STRING) {
	v->value.s = NULL;
    }

    /* allocate some space if we need to */
    if (v->tp.sub.len != len ||
	v->value.s == NULL) {
	v->value.s = realloc(v->value.s, len);
	v->tp.sub.len = len;
    }

    memset(v->value.s, 0, len);
    /* suns strings aren't required to be null terminated, so don't leave
       space for a null byte */
    strncpy(v->value.s, string, len);
    v->tp.type = SUNS_STRING;
}

char * suns_value_get_string(suns_value_t *v)
{
    assert(v != NULL);
    assert(v->tp.type == SUNS_STRING);

    return v->value.s;
}


/* search for a specified did
   returns the first suns_model_did_t matching the specified did value
   returns NULL if no matching did value is found */
suns_model_did_t *suns_find_did(list_t *did_list, uint16_t did)
{
    suns_model_did_t *d;

    debug("looking up model for did %d", did);

    list_node_t *c;
    list_for_each(did_list, c) {
	d = c->data;
	if (d->did == did) {
	    return d;
	}
    }

    return NULL;
}


/* take a binary data blob and decode it
   this relies on the global did_list to find the model */
suns_dataset_t *suns_decode_data(list_t *did_list,
				 unsigned char *buf,
				 size_t len)
{
    list_node_t *c;
    suns_model_did_t *did = NULL;
    suns_dataset_t *data;

    /* the first 2 bytes contain the did */
    uint16_t did_value = be16toh(*((uint16_t *)buf));

    /* the next 2 bytes contains the length */
    uint16_t did_len = be16toh(*((uint16_t *)buf + 1));

    did = suns_find_did(did_list, did_value);

    if (did == NULL) {
	warning("unknown did %d", did_value);
	return NULL;
    }

    suns_model_t *m = did->model;

    data = suns_dataset_new(did);
    if (data == NULL) {
	debug("suns_dataset_new() failed");
	return NULL;
    }


    debug("did %d found", did_value);

    debug("did_len = %d, m->len = %d", did_len, m->len);
    
    /* first check the length value

       if model-> len != model->base_len it means there is a variable
       length section at the end of the model
    */
       
    /* sanity check the provided length values */
    if (m->base_len != m->len) {
	/* check for repeating portion of the model */
	if ((did_len - m->base_len) % (m->len - m->base_len) != 0) {
	    warning("repeating portion of the model (%d) is not a multiple"
		    "of the expected repeating portion (%d)",
		    did_len - m->base_len,
		    m->len - m->base_len);
	    /* FIXME: need to mark a flag if we're going
	       to generate a report */
	}
    } else {
	/* check that the length is what we expect */
	if (m->len != did_len) {
	    warning("length value in data block (%d) does "
		    "not match data model spec length (%d).",
		    did_len, m->len);
	    /* FIXME: need to mark a flag if we're going
	       to generate a report */
	}
    }
    if (did == NULL) {
	error("unknown model did %03d", did_value);
	return NULL;
    }

    /* now step through all the dp_blocks decoding data */
    int byte_offset = 0;
    list_for_each(m->dp_blocks, c) {
	byte_offset += suns_decode_dp_block(c->data,
					    buf + byte_offset + 4,
					    (did_len * 2) - byte_offset,
					    data->values);
	if (byte_offset >= (did_len * 2) + 4) {
	    error("buffer overrun in suns_decode_data(): byte offset %d of %d",
		  byte_offset, did_len * 2);
	    return data;
	}

    }
    
    return data;
}



/* fill in any implied offset fields */
void suns_model_fill_offsets(suns_model_t *m)
{
    list_node_t *c, *d;
    int offset = 3;  /* skip the header, did and len fields */
    /* int found_repeating = 0; */

    list_for_each(m->dp_blocks, d) {
	suns_dp_block_t *dp_block = d->data;
	int dp_block_offset = 0;

	list_for_each(dp_block->dp_list, c) {
	    suns_dp_t *dp = c->data;
	    /* suns_dp_t *sf; */

	    /* use a provided offset to set our check offset */
	    if (dp->offset > 0) {
		dp_block_offset = dp->offset - offset;
	    } else {
		/* fill in implied offset */
		dp->offset = offset + dp_block_offset;
	    }

	    /* advance offset by the size of the current data type */
	    if (dp->type_pair->type == SUNS_STRING) {
		/* check if len is an odd number */
		if ( ((dp->type_pair->sub.len / 2.0) -
		      (dp->type_pair->sub.len / 2)) > 0) {
		    warning("datapoint %s is a string of odd length %d;"
			    "rounding up to whole register",
			    dp->name, dp->type_pair->sub.len);
		    dp_block_offset += (dp->type_pair->sub.len / 2) + 1;
		} else {
		    dp_block_offset += dp->type_pair->sub.len / 2;
		}
	    } else {
		dp_block_offset += suns_type_size(dp->type_pair->type) / 2;
	    }
	    
	}
	
	dp_block->len = dp_block_offset;

	/* the repeating block can only be the last block, so throw
	   a warning if we find one someplace else */
	if (dp_block->repeating) {
	    if (dp_block != m->dp_blocks->tail->data) {
		error("repeating marker found on block "
		      "that is not last");
	    }
	} else {
	    /* m->base_len is everything up until the optional
	       repeating block */
	    m->base_len += dp_block_offset;
	}

	m->len += dp_block_offset;
    }

    /* the repeating block can only be the last block */
    /*
    suns_dp_block_t *dp_block = m->dp_blocks->tail->data;
    if (dp_block->repeating) {
	m->base_len = offset;
    }
    */
    
    /* fill in len field */
    if (m->len < 1) {
	m->len = offset - 3;  /* offset starts numbering at 1 */
    }
}


/* decode the provided buffer buf of length len using the provided dp_block
   the results are appended to value_list and the total buffer length decoded
   is returned (which should be the same as the len argument)

   GOTCH: all lengths and offset are in bytes, not modbus registers
   FIXME: at some point i need to go through everything and pick one system */
int suns_decode_dp_block(suns_dp_block_t *dp_block,
			 unsigned char *buf,
			 size_t len,
			 list_t *value_list)
{
    int len_multiple;
    int byte_offset = 0;
    int i;
    list_node_t *c;
    suns_value_t *v;

    if (dp_block->repeating) {
	debug("repeating block");
	len_multiple = len / dp_block->len / 2;
    } else {
	len_multiple = 1;
    }

    debug("len_multiple = %d", len_multiple);

    /* dump_buffer(stdout, buf, len); */
    
    for (i = 0; i < len_multiple; i++) {
	list_for_each(dp_block->dp_list, c) {
	    suns_dp_t *dp = c->data;
	    int size = suns_type_pair_size(dp->type_pair);
	    v = suns_value_new();

	    if ((byte_offset + size) <= len) {
		suns_value_set_null(v);
		v->name = dp->name;
		suns_buf_to_value(buf + byte_offset, dp->type_pair, v);
		debug("v->tp.type = %s", suns_type_string(v->tp.type));
		list_node_add(value_list, list_node_new(v));
		byte_offset += size;
		
	    } else {
		warning("%s offset %d is out-of-bounds",
			dp->name, dp->offset + (i * dp_block->len));
		debug("requested = %d, len = %d",
		      (dp->offset * 2) + suns_type_size(dp->type_pair->type),
		      len);
		return byte_offset;
	    }
	}
    }

    return byte_offset;
}


suns_define_t *suns_search_enum_defines(list_t *list, unsigned int value)
{
    list_node_t *c;

    list_for_each(list, c) {
        suns_define_t *define = c->data;
        if (define->value == value) {
            return define;
        }
    }

    /* not found */
    debug("value %d not found", value);

    return NULL;
}


suns_define_t *suns_search_bitfield_defines(list_t *list, unsigned int value)
{
    list_node_t *c;

    list_for_each(list, c) {
        suns_define_t *define = c->data;
        if (define->value & value) {
            return define;
        }
    }

    /* not found */
    debug("value 0x%x not found", value);

    return NULL;
}
        
