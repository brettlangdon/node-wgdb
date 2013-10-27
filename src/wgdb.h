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

#endif
