#include "wgdb.h"

Persistent<Function> Record::constructor;

/*
 *
 * Structs
 *
*/
struct Baton {
  bool has_cb;
  Persistent<Function> callback;
  WgDB* wgdb;
  const char* error;
  void* data;
};

struct Fields {
  int length;
  wg_int* encs;
};

struct RecordData {
  int length;
  void* record;
};


/*
 *
 * Async Functions
 *
*/
void end_call(bool has_cb, Persistent<Function> callback, const char* error, Local<Value> result){
  HandleScope scope;
  TryCatch try_catch;

  if(has_cb){
    Local<Value> argv[2];
    if(error){
      argv[0] = Exception::Error(String::New(error));
    } else{
      argv[0] = Local<Value>::New(Undefined());
    }
    argv[1] = result;
    callback->Call(Context::GetCurrent()->Global(), 2, argv);
  }

  if(try_catch.HasCaught()){
    FatalException(try_catch);
  }
}

void do_attach(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);
  if(baton->wgdb && baton->wgdb->db_ptr){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s is already attached", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  baton->wgdb->db_ptr = wg_attach_database(baton->wgdb->db_name, baton->wgdb->size);
  if(!baton->wgdb->db_ptr){
    char buffer[1024];
    sprintf(buffer, "wgdb could not attach database %s (%d)", baton->wgdb->db_name, baton->wgdb->size);
    baton->error = buffer;
    return;
  }
}

void do_detach(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);
  if(!baton->wgdb || !baton->wgdb->db_ptr){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s is not attached", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }
  if(wg_detach_database(baton->wgdb->db_ptr) != 0){
    char buffer[1024];
    sprintf(buffer, "wgdb could not detach database %s", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }
}

void do_delete(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);
  if(!baton->wgdb || !baton->wgdb->db_ptr){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s is not attached", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }
  if(wg_delete_database(baton->wgdb->db_name) != 0){
    char buffer[1024];
    sprintf(buffer, "wgdb could not delete database %s", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }
}

void do_create_record(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  RecordData* data = static_cast<RecordData*>(baton->data);
  data->record = wg_create_record(baton->wgdb->db_ptr, data->length);
  if(data->record == NULL){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not create record of length %d", baton->wgdb->db_name, data->length);
    baton->error = buffer;
    delete data;
    return;
  }

  baton->data = data;
}

void do_record_next(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  Record* record = static_cast<Record*>(baton->data);

  RecordData* new_data = new RecordData();
  new_data->record = wg_get_next_record(record->wgdb->db_ptr, record->rec_ptr);

  baton->data = new_data;
}

void do_record_fields(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  Record* record = static_cast<Record*>(baton->data);

  wg_int length = wg_get_record_len(record->wgdb->db_ptr, record->rec_ptr);
  if(length < 0){
    char buffer[1024];
    sprintf(buffer, "could not get record length from database %s", record->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  wg_int enc;
  int i;
  Fields* fields = new Fields();
  fields->length = (int)length;
  fields->encs = new wg_int[length];
  for(i = 0; i < length; ++i){
    enc = wg_get_field(record->wgdb->db_ptr, record->rec_ptr, i);
    fields->encs[i] = enc;
  }

  baton->data = fields;
}

void do_after_no_result(uv_work_t* req, int status){
  Baton* baton = static_cast<Baton*>(req->data);
  Local<Value> result = Local<Value>::New(Undefined());
  end_call(baton->has_cb, baton->callback, baton->error, result);
  baton->callback.Dispose();
  delete baton;
  delete req;
}

void do_after_fields(uv_work_t* req, int status){
  Baton* baton = static_cast<Baton*>(req->data);
  Fields* fields = static_cast<Fields*>(baton->data);
  Local<Array> result = Array::New(fields->length);
  for(int i = 0; i < fields->length; ++i){
    result->Set(i, encoded_to_v8(baton->wgdb->db_ptr, fields->encs[i]));
  }
  end_call(baton->has_cb, baton->callback, baton->error, result);
  delete baton;
  delete req;
}

void Record::do_after_create_record(uv_work_t* req, int status){
  Baton* baton = static_cast<Baton*>(req->data);
  RecordData* data = static_cast<RecordData*>(baton->data);
  Local<Value> result;
  if(!baton->error && data->record){
    Local<Object> record_obj = Record::constructor->NewInstance(0, NULL);
    Record* record = ObjectWrap::Unwrap<Record>(record_obj);
    record->rec_ptr = data->record;
    record->wgdb = baton->wgdb;
    result = Local<Value>::New(record_obj);
  } else{
    result = Local<Value>::New(Undefined());
  }
  end_call(baton->has_cb, baton->callback, baton->error, result);
  baton->callback.Dispose();
  delete baton;
  delete req;
}

/*
 *
 * WgDB Class Definitions
 *
*/

void WgDB::Init(Handle<Object> target){
  Local<FunctionTemplate> tpl = FunctionTemplate::New(WgDB::New);
  tpl->SetClassName(String::NewSymbol("wgdb"));
  tpl->InstanceTemplate()->SetInternalFieldCount(2);

  tpl->PrototypeTemplate()->Set(String::NewSymbol("attach"),
                                FunctionTemplate::New(WgDB::Attach)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("delete"),
                                FunctionTemplate::New(WgDB::Delete)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("detach"),
                                FunctionTemplate::New(WgDB::Detach)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("createRecord"),
                                FunctionTemplate::New(WgDB::CreateRecord)->GetFunction());

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  target->Set(String::NewSymbol("wgdb"), constructor);
}


Handle<Value> WgDB::New(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  if(!args.IsConstructCall()){
    return ThrowException(Exception::SyntaxError(String::New("wgdb requires the 'new' operator to create an instance")));
  }

  char* db_name = NULL;
  int size = 0;
  if(argc > 0 && args[0]->IsNumber()){
    db_name = get_str(args[0]->ToString());
  } else{
    return ThrowException(Exception::TypeError(String::New("wgdb argument 1 must be an integer")));
  }

  if(argc > 1 && args[1]->IsNumber()){
    size = (int)(args[1]->ToInt32()->Value());
  }

  WgDB* db = new WgDB();
  db->db_name = db_name;
  db->size = size;
  db->Wrap(args.This());
  return scope.Close(args.This());
}


Handle<Value> WgDB::Attach(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  Baton* baton = new Baton();

  if(argc > 0 && args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }

  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());

  baton->wgdb = db;
  db->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_attach, do_after_no_result);

  return scope.Close(Undefined());
}


Handle<Value> WgDB::Delete(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  Baton* baton = new Baton();
  if(argc > 0 && args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }

  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());
  baton->wgdb = db;
  db->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_delete, do_after_no_result);

  return scope.Close(Undefined());
}


Handle<Value> WgDB::Detach(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  Baton* baton = new Baton();
  if(argc > 0 && args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }

  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());
  baton->wgdb = db;
  db->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_detach, do_after_no_result);

  return scope.Close(Undefined());
}


Handle<Value> WgDB::CreateRecord(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  Baton* baton = new Baton();
  if(argc > 1 && args[0]->IsNumber()){
    RecordData* data = new RecordData();
    data->length = (int)args[0]->ToInt32()->Value();
    baton->data = data;

    if(args[argc - 1]->IsFunction()){
      baton->has_cb = true;
      baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
    }
  } else{
    return ThrowException(Exception::TypeError(String::New("createRecord argument 1 must be an integer")));
  }

  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());
  baton->wgdb = db;
  db->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_create_record, Record::do_after_create_record);

  return scope.Close(Undefined());
}


/*
 *
 * Record Class Definitions
 *
*/

void Record::Init(){
  HandleScope scope;
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("record"));
  tpl->InstanceTemplate()->SetInternalFieldCount(2);
  tpl->PrototypeTemplate()->Set(String::NewSymbol("next"),
                                FunctionTemplate::New(Record::Next)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("getLength"),
                                FunctionTemplate::New(Record::Length)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("getFields"),
                                FunctionTemplate::New(Record::Fields)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("setField"),
                                FunctionTemplate::New(Record::SetField)->GetFunction());
  constructor = Persistent<Function>::New(tpl->GetFunction());
}


Handle<Value> Record::New(const Arguments& args){
  HandleScope scope;

  Record* record = new Record();
  record->Wrap(args.This());

  return scope.Close(args.This());
}

Handle<Value> Record::Next(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  Baton* baton = new Baton();
  if(argc > 0 && args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }

  Record* record = ObjectWrap::Unwrap<Record>(args.This());
  baton->wgdb = record->wgdb;
  baton->data = record;
  record->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_record_next, Record::do_after_create_record);

  return scope.Close(Undefined());
}

Handle<Value> Record::Length(const Arguments& args){
  HandleScope scope;

  Record* record = ObjectWrap::Unwrap<Record>(args.This());
  wg_int length = wg_get_record_len(record->wgdb->db_ptr, record->rec_ptr);
  if(length < 0){

  }

  return scope.Close(Int32::New(length));
}

Handle<Value> Record::Fields(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  Baton* baton = new Baton();
  if(argc > 0 && args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }

  Record* record = ObjectWrap::Unwrap<Record>(args.This());
  baton->wgdb = record->wgdb;
  baton->data = record;
  record->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_record_fields, do_after_fields);

  return scope.Close(Undefined());
}
