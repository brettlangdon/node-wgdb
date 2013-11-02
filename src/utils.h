#ifndef WGDB_UTILS_H
#define WGDB_UTILS_H

#include <node.h>
#include <whitedb/dbapi.h>
#include <v8.h>

using namespace node;
using namespace v8;

char* get_str(Local<Value> val);

Local<Value> encoded_to_v8(void* db_ptr, wg_int enc);

wg_int v8_to_encoded(void* db_ptr, Local<Value> data);

wg_int v8_to_encoded_param(void* db_ptr, Local<Value> data);
#endif
