#include "wgdb.h"

Persistent<Function> Record::constructor;
Persistent<Function> Cursor::constructor;

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
  wg_int lock = wg_start_write(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire write lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }
  data->record = wg_create_record(baton->wgdb->db_ptr, data->length);

  if(!wg_end_write(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not release write lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  if(data->record == NULL){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not create record of length %d", baton->wgdb->db_name, data->length);
    baton->error = buffer;
    return;
  }

  baton->data = data;
}

void do_create_index(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);
  IndexData* data = static_cast<IndexData*>(baton->data);

  wg_int lock = wg_start_write(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire write lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  if(wg_column_to_index_id(baton->wgdb->db_ptr, data->field, WG_INDEX_TYPE_TTREE, data->matchrec, data->reclen) == -1){
    if(wg_create_index(baton->wgdb->db_ptr, data->field, WG_INDEX_TYPE_TTREE, data->matchrec, data->reclen) != 0){
      char buffer[1024];
      sprintf(buffer, "creation of index on field %d failed for wgdb database %s", data->field, baton->wgdb->db_name);
      baton->error = buffer;
    }
  } else{
    char buffer[1024];
    sprintf(buffer, "index on field %d already exists for wgdb database %s", data->field, baton->wgdb->db_name);
    baton->error = buffer;
  }

  if(!wg_end_write(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not release write lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }
}

void do_drop_index(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);
  IndexData* data = static_cast<IndexData*>(baton->data);

  wg_int lock = wg_start_write(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire write lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  wg_int index_id = wg_column_to_index_id(baton->wgdb->db_ptr, data->field, 0, data->matchrec, data->reclen);

  if(index_id == -1){
    char buffer[1024];
    sprintf(buffer, "index on field %d does not exist for wgdb database %s", data->field, baton->wgdb->db_name);
    baton->error = buffer;

  } else{
    if(wg_drop_index(baton->wgdb->db_ptr, index_id) != 0){
      char buffer[1024];
      sprintf(buffer, "failed to drop index on field %d for wgdb database %s", data->field, baton->wgdb->db_name);
      baton->error = buffer;

    }
  }

  if(!wg_end_write(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not release write lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }
}

void do_first_record(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  RecordData* data = static_cast<RecordData*>(baton->data);
  wg_int lock = wg_start_read(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }
  data->record = wg_get_first_record(baton->wgdb->db_ptr);

  if(!wg_end_read(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not release read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  baton->data = data;
}

void do_dump(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);
  char* filename = static_cast<char*>(baton->data);

  wg_int lock = wg_start_read(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  if(wg_dump(baton->wgdb->db_ptr, filename) != 0){
    char buffer[1024];
    sprintf(buffer, "error dumping database %s to %s", baton->wgdb->db_name, filename);
    baton->error = buffer;
  }

  if(!wg_end_read(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not release read lock", baton->wgdb->db_name);
    baton->error = buffer;
  }
}

void do_import(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);
  char* filename = static_cast<char*>(baton->data);

  wg_int lock = wg_start_write(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire write lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  if(wg_import_dump(baton->wgdb->db_ptr, filename) != 0){
    char buffer[1024];
    sprintf(buffer, "error importing %s to database %s", filename, baton->wgdb->db_name);
    baton->error = buffer;
  }

  if(!wg_end_write(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not release write lock", baton->wgdb->db_name);
    baton->error = buffer;
  }
}

void do_find_record(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);
  FindData* data = static_cast<FindData*>(baton->data);

  wg_int lock = wg_start_read(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  RecordData* new_data = new RecordData();
  new_data->record = wg_find_record(baton->wgdb->db_ptr, data->field, data->cond, data->enc, data->rec);
  baton->data = new_data;
  wg_free_encoded(baton->wgdb->db_ptr, data->enc);
  delete data;

  if(!wg_end_read(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not release read lock", baton->wgdb->db_name);
    baton->error = buffer;
  }

}

void do_cursor_next(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  Cursor* cursor = static_cast<Cursor*>(baton->data);

  wg_int lock = wg_start_read(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  RecordData* new_data = new RecordData();
  new_data->record = wg_fetch(cursor->wgdb->db_ptr, cursor->query);
  baton->data = new_data;

  if(!wg_end_read(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not releae read lock", baton->wgdb->db_name);
    baton->error = buffer;
  }
}

void do_query(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  CursorData* data = static_cast<CursorData*>(baton->data);

  wg_int lock = wg_start_read(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  data->query = wg_make_query(baton->wgdb->db_ptr, NULL, 0, data->arglist, data->arglen);
  baton->data = data;

  if(!wg_end_read(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not releae read lock", baton->wgdb->db_name);
    baton->error = buffer;
  }
}

void do_record_next(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  Record* record = static_cast<Record*>(baton->data);

  wg_int lock = wg_start_read(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  RecordData* new_data = new RecordData();
  new_data->record = wg_get_next_record(record->wgdb->db_ptr, record->rec_ptr);
  baton->data = new_data;

  if(!wg_end_read(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not releae read lock", baton->wgdb->db_name);
    baton->error = buffer;
  }
}

void do_record_delete(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  Record* record = static_cast<Record*>(baton->data);

  wg_int lock = wg_start_write(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire write lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  if(wg_delete_record(record->wgdb->db_ptr, record->rec_ptr) != 0){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not delete record", baton->wgdb->db_name);
    baton->error = buffer;
  }

  if(!wg_end_write(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not releae write lock", baton->wgdb->db_name);
    baton->error = buffer;
  }
}

void do_record_set(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  FieldData* set_field = static_cast<FieldData*>(baton->data);

  wg_int lock = wg_start_write(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire write lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  wg_int result = wg_set_field(set_field->record->wgdb->db_ptr, set_field->record->rec_ptr, set_field->field, set_field->enc);
  if(result < 0){
    char buffer[1024];
    sprintf(buffer, "error setting field %d on record for database %s", set_field->field, set_field->record->wgdb->db_name);
    baton->error = buffer;
  }

  if(!wg_end_write(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not release write lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }
}

void do_record_get(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  FieldData* get_field = static_cast<FieldData*>(baton->data);

  wg_int lock = wg_start_read(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  wg_int result = wg_get_field(get_field->record->wgdb->db_ptr, get_field->record->rec_ptr, get_field->field);

  if(!wg_end_read(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not release read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  if(result < 0){
    char buffer[1024];
    sprintf(buffer, "error getting field %d on record for database %s", get_field->field, get_field->record->wgdb->db_name);
    baton->error = buffer;
  }
  get_field->enc = result;
}

void do_record_fields(uv_work_t* req){
  Baton* baton = static_cast<Baton*>(req->data);

  Record* record = static_cast<Record*>(baton->data);

  wg_int lock = wg_start_read(baton->wgdb->db_ptr);
  if(!lock){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not acquire read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

  wg_int length = wg_get_record_len(record->wgdb->db_ptr, record->rec_ptr);

  if(!wg_end_read(baton->wgdb->db_ptr, lock)){
    char buffer[1024];
    sprintf(buffer, "wgdb database %s could not release read lock", baton->wgdb->db_name);
    baton->error = buffer;
    return;
  }

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

void do_after_enc_result(uv_work_t* req, int status){
  Baton* baton = static_cast<Baton*>(req->data);
  FieldData* data = static_cast<FieldData*>(baton->data);
  Local<Value> result = encoded_to_v8(baton->wgdb->db_ptr, data->enc);
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
  if(!baton->error && data->record != NULL){
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
  tpl->PrototypeTemplate()->Set(String::NewSymbol("firstRecord"),
                                FunctionTemplate::New(WgDB::FirstRecord)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("dump"),
                                FunctionTemplate::New(WgDB::Dump)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("import"),
                                FunctionTemplate::New(WgDB::Import)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("findRecord"),
                                FunctionTemplate::New(WgDB::FindRecord)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("query"),
                                FunctionTemplate::New(WgDB::Query)->GetFunction());
  // tpl->PrototypeTemplate()->Set(String::NewSymbol("createIndex"),
  //                               FunctionTemplate::New(WgDB::CreateIndex)->GetFunction());
  // tpl->PrototypeTemplate()->Set(String::NewSymbol("dropIndex"),
  //                               FunctionTemplate::New(WgDB::DropIndex)->GetFunction());

  tpl->Set(String::NewSymbol("EQUAL"), Int32::New(int(WG_COND_EQUAL)));
  tpl->Set(String::NewSymbol("NOT_EQUAL"), Int32::New(int(WG_COND_NOT_EQUAL)));
  tpl->Set(String::NewSymbol("LESSTHAN"), Int32::New(int(WG_COND_LESSTHAN)));
  tpl->Set(String::NewSymbol("GREATER"), Int32::New(int(WG_COND_GREATER)));
  tpl->Set(String::NewSymbol("LTEQUAL"), Int32::New(int(WG_COND_LTEQUAL)));
  tpl->Set(String::NewSymbol("GTEQUAL"), Int32::New(int(WG_COND_GTEQUAL)));

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

Handle<Value> WgDB::FirstRecord(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  Baton* baton = new Baton();
  RecordData* data = new RecordData();
  baton->data = data;

  if(argc > 0 && args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
    }

  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());
  baton->wgdb = db;
  db->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_first_record, Record::do_after_create_record);

  return scope.Close(Undefined());
}

Handle<Value> WgDB::Dump(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  Baton* baton = new Baton();
  if(argc > 1 && args[0]->IsString()){
    baton->data = get_str(args[0]->ToString());

    if(args[argc - 1]->IsFunction()){
      baton->has_cb = true;
      baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
    }
  } else{
    return ThrowException(Exception::TypeError(String::New("dump argument 1 must be a string")));
  }

  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());
  baton->wgdb = db;
  db->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_dump, do_after_no_result);

  return scope.Close(Undefined());
}

Handle<Value> WgDB::Import(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  Baton* baton = new Baton();
  if(argc > 1 && args[0]->IsString()){
    baton->data = get_str(args[0]->ToString());

    if(args[argc - 1]->IsFunction()){
      baton->has_cb = true;
      baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
    }
  } else{
    return ThrowException(Exception::TypeError(String::New("import argument 1 must be a string")));
  }

  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());
  baton->wgdb = db;
  db->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_import, do_after_no_result);

  return scope.Close(Undefined());
}

Handle<Value> WgDB::FindRecord(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  if(argc < 3){
    return ThrowException(Exception::Error(String::New("findRecord requires 3 parameters")));
  }

  if(!args[0]->IsInt32()){
    return ThrowException(Exception::TypeError(String::New("findRecord argument 1 must be an integer")));
  }

  if(!args[1]->IsInt32()){
    return ThrowException(Exception::TypeError(String::New("findRecord argument 2 must be an integer")));
  }

  void* rec = NULL;
  if(argc > 3 && args[3]->IsObject() && !args[3]->IsFunction()){
    Record* record = ObjectWrap::Unwrap<Record>(args[3]->ToObject());
    rec = record->rec_ptr;
  }

  Baton* baton = new Baton();
  if(args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }

  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());
  baton->wgdb = db;

  FindData* data = new FindData();
  data->field = (int)args[0]->Int32Value();
  data->cond = (int)args[1]->Int32Value();
  data->enc = v8_to_encoded(db->db_ptr, args[2]);
  data->rec = rec;

  baton->data = data;
  db->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_find_record, Record::do_after_create_record);

  return scope.Close(Undefined());
}

Handle<Value> WgDB::Query(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  if(argc < 1){
    return ThrowException(Exception::Error(String::New("query requires 1 parameter")));
  }

  if(!args[0]->IsArray()){
    return ThrowException(Exception::TypeError(String::New("query argument 1 must be an array")));
  }

  Baton* baton = new Baton();
  if(args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }

  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());
  baton->wgdb = db;
  db->Ref();

  CursorData* data = new CursorData();

  Local<Array> arg_array = Local<Array>::Cast(args[0]->ToObject());
  data->arglen = arg_array->Length();
  wg_query_arg arglist[data->arglen];
  data->arglist = arglist;
  int i;
  for(i = 0; i < data->arglen; ++i){
    Local<Value> next_val = arg_array->Get(i);
    if(!next_val->IsArray()){
      return ThrowException(Exception::TypeError(String::New("query argument 1 must be an array of arrays: [ [<field>, <cond>, <vaue>], ... ]")));
    }
    Local<Array> next = Local<Array>::Cast(next_val->ToObject());
    if(next->Length() != 3){
      return ThrowException(Exception::TypeError(String::New("query argument 1 must be an array of arrays: [ [<field>, <cond>, <vaue>], ... ]")));
    }

    Local<Value> field = next->Get(0);
    if(!field->IsInt32()){
      return ThrowException(Exception::TypeError(String::New("query argument 1 must be an array of arrays: [ [<field>, <cond>, <vaue>], ... ]")));
    }

    Local<Value> cond = next->Get(1);
    if(!cond->IsInt32()){
      return ThrowException(Exception::TypeError(String::New("query argument 1 must be an array of arrays: [ [<field>, <cond>, <vaue>], ... ]")));
    }

    data->arglist[i].column = field->Int32Value();
    data->arglist[i].cond = cond->Int32Value();
    data->arglist[i].value = v8_to_encoded_param(baton->wgdb->db_ptr, next->Get(2));
  }

  baton->data = data;
  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_query, Cursor::do_after_create_cursor);

  return scope.Close(Undefined());
}

Handle<Value> WgDB::CreateIndex(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  if(argc < 1){
    return ThrowException(Exception::Error(String::New("query requires at least 1 parameter")));
  }

  if(!args[0]->IsInt32()){
    return ThrowException(Exception::TypeError(String::New("query argument 1 must be an integer")));
  }


  IndexData* data = new IndexData();
  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());
  data->field = args[0]->Int32Value();

  if(argc > 1 && args[1]->IsArray()){
    Local<Array> arg_array = Local<Array>::Cast(args[0]->ToObject());
    data->reclen = arg_array->Length();
    wg_int tmp[data->reclen];

    for(int i = 0; i < data->reclen; ++i){
      Local<Value> next = arg_array->Get(i);
      if(next->IsUndefined()){
        tmp[i] = 0;
      } else{
        tmp[i] = v8_to_encoded(db->db_ptr, next);
      }
    }

    data->matchrec = tmp;
  } else if(argc > 2 && !args[1]->IsArray()){
    return ThrowException(Exception::TypeError(String::New("query argument 2 must be an array")));
  }

  Baton* baton = new Baton();
  if(args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }
  baton->wgdb = db;
  baton->data = data;
  db->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_create_index, do_after_no_result);

  return scope.Close(Undefined());
}

Handle<Value> WgDB::DropIndex(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  if(argc < 1){
    return ThrowException(Exception::Error(String::New("query requires at least 1 parameter")));
  }

  if(!args[0]->IsInt32()){
    return ThrowException(Exception::TypeError(String::New("query argument 1 must be an integer")));
  }


  IndexData* data = new IndexData();
  WgDB* db = ObjectWrap::Unwrap<WgDB>(args.This());
  data->field = args[0]->Int32Value();

  if(argc > 1 && args[1]->IsArray()){
    Local<Array> arg_array = Local<Array>::Cast(args[0]->ToObject());
    data->reclen = arg_array->Length();
    wg_int tmp[data->reclen];

    for(int i = 0; i < data->reclen; ++i){
      Local<Value> next = arg_array->Get(i);
      if(next->IsUndefined()){
        tmp[i] = 0;
      } else{
        tmp[i] = v8_to_encoded(db->db_ptr, next);
      }
    }

    data->matchrec = tmp;
  } else if(argc > 2 && !args[1]->IsArray()){
    return ThrowException(Exception::TypeError(String::New("query argument 2 must be an array")));
  }

  Baton* baton = new Baton();
  if(args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }
  baton->wgdb = db;
  baton->data = data;
  db->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_drop_index, do_after_no_result);

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
  tpl->PrototypeTemplate()->Set(String::NewSymbol("getField"),
                                FunctionTemplate::New(Record::GetField)->GetFunction());
  tpl->PrototypeTemplate()->Set(String::NewSymbol("delete"),
                                FunctionTemplate::New(Record::Delete)->GetFunction());
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

Handle<Value> Record::Delete(const Arguments& args){
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
  uv_queue_work(uv_default_loop(), req, do_record_delete, do_after_no_result);

  return scope.Close(Undefined());
}

Handle<Value> Record::SetField(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  if(argc < 2){
    return ThrowException(Exception::Error(String::New("setField requires 2 parameters")));
  }

  Baton* baton = new Baton();
  if(argc > 2 && args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }

  if(!args[0]->IsInt32()){
    return ThrowException(Exception::Error(String::New("setField argument 1 must be an integer")));
  }

  Record* record = ObjectWrap::Unwrap<Record>(args.This());
  baton->wgdb = record->wgdb;

  FieldData* set_field = new FieldData();
  set_field->field = (int)args[0]->Int32Value();
  set_field->enc = v8_to_encoded(record->wgdb->db_ptr, args[1]);
  set_field->record = record;

  baton->data = set_field;
  record->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_record_set, do_after_no_result);

  return scope.Close(Undefined());
}

Handle<Value> Record::GetField(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  if(argc < 1){
    return ThrowException(Exception::Error(String::New("setField requires 1 parameters")));
  }

  Baton* baton = new Baton();
  if(argc > 1 && args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }

  if(!args[0]->IsInt32()){
    return ThrowException(Exception::Error(String::New("getField argument 1 must be an integer")));
  }

  Record* record = ObjectWrap::Unwrap<Record>(args.This());
  baton->wgdb = record->wgdb;

  FieldData* get_field = new FieldData();
  get_field->field = (int)args[0]->Int32Value();
  get_field->record = record;

  baton->data = get_field;
  record->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_record_get, do_after_enc_result);

  return scope.Close(Undefined());
}

/*
 *
 * Cursor Class Definitions
 *
*/
Cursor::~Cursor(){
  if(this->wgdb && this->wgdb->db_ptr){
    wg_free_query(this->wgdb->db_ptr, this->query);
    int i;
    for(i = 0; i < this->arglen; ++i){
      wg_free_query_param(this->wgdb->db_ptr, this->arglist[i].value);
    }
  }
}

void Cursor::Init(){
  HandleScope scope;
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("cursor"));
  tpl->InstanceTemplate()->SetInternalFieldCount(2);
  tpl->PrototypeTemplate()->Set(String::NewSymbol("next"),
                                FunctionTemplate::New(Cursor::Next)->GetFunction());
  constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<Value> Cursor::New(const Arguments& args){
  HandleScope scope;

  Cursor* cursor = new Cursor();
  cursor->Wrap(args.This());

  return scope.Close(args.This());
}

Handle<Value> Cursor::Next(const Arguments& args){
  HandleScope scope;
  int argc = args.Length();

  Baton* baton = new Baton();
  if(argc > 0 && args[argc - 1]->IsFunction()){
    baton->has_cb = true;
    baton->callback = Persistent<Function>::New(Local<Function>::Cast(args[argc - 1]));
  }

  Cursor* cursor = ObjectWrap::Unwrap<Cursor>(args.This());
  baton->wgdb = cursor->wgdb;
  baton->data = cursor;
  cursor->Ref();

  uv_work_t *req = new uv_work_t;
  req->data = baton;
  uv_queue_work(uv_default_loop(), req, do_cursor_next, Record::do_after_create_record);

  return scope.Close(Undefined());
}

void Cursor::do_after_create_cursor(uv_work_t* req, int status){
  Baton* baton = static_cast<Baton*>(req->data);
  CursorData* data = static_cast<CursorData*>(baton->data);
  Local<Value> result;
  if(!baton->error){
    Local<Object> cursor_obj = Cursor::constructor->NewInstance(0, NULL);
    Cursor* cursor = ObjectWrap::Unwrap<Cursor>(cursor_obj);
    cursor->arglen = data->arglen;
    cursor->arglist = data->arglist;
    cursor->query = data->query;
    cursor->wgdb = baton->wgdb;
    result = Local<Value>::New(cursor_obj);
  } else{
    result = Local<Value>::New(Undefined());
  }
  end_call(baton->has_cb, baton->callback, baton->error, result);
  baton->callback.Dispose();
  delete baton;
  delete req;
}
