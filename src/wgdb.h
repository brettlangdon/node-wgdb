#ifndef WGDB_DB_H
#define WGDB_DB_H

#include <node.h>
#include <whitedb/dbapi.h>
#include <v8.h>

#include "utils.h"

using namespace node;
using namespace v8;

class WgDB : ObjectWrap{
 public:
  void* db_ptr;
  char* db_name;
  int size;

  static void Init(Handle<Object> target);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Attach(const Arguments& args);
  static Handle<Value> Detach(const Arguments& args);
  static Handle<Value> Delete(const Arguments& args);
  static Handle<Value> CreateRecord(const Arguments& args);
  static Handle<Value> FirstRecord(const Arguments& args);
  static Handle<Value> Dump(const Arguments& args);
  static Handle<Value> Import(const Arguments& args);
  static Handle<Value> Query(const Arguments& args);
  static Handle<Value> FindRecord(const Arguments& args);
};

class Record : ObjectWrap{
 public:
  WgDB* wgdb;
  void* rec_ptr;

  static Persistent<Function> constructor;
  static void Init();
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Next(const Arguments& args);

  static Handle<Value> SetField(const Arguments& args);
  static Handle<Value> GetField(const Arguments& args);
  static Handle<Value> Length(const Arguments& args);
  static Handle<Value> Fields(const Arguments& args);
  static Handle<Value> Delete(const Arguments& args);

  static void do_after_create_record(uv_work_t* req, int status);
};

class Cursor : ObjectWrap{
 public:
  WgDB* wgdb;
  wg_query* query;
  wg_query_arg* arglist;
  int arglen;

  ~Cursor();
  static Persistent<Function> constructor;
  static void Init();
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> Next(const Arguments& args);

  static void do_after_create_cursor(uv_work_t* req, int status);
};


struct Baton {
  bool has_cb;
  Persistent<Function> callback;
  WgDB* wgdb;
  const char* error;
  void* data;
};

struct FieldData {
  int field;
  wg_int enc;
  Record* record;
};

struct Fields {
  int length;
  wg_int* encs;
};

struct RecordData {
  int length;
  void* record;
};

struct FindData {
  int field;
  wg_int enc;
  wg_int cond;
  void* rec;
  void* data;
};

struct CursorData {
  int arglen;
  wg_query_arg* arglist;
  wg_query* query;
};

struct IndexData {
  int reclen;
  wg_int* matchrec;
  int field;
};

#endif
