#include "utils.h"


char* get_str(Local<Value> val){
  if(!val->IsString()){
    ThrowException(Exception::TypeError(String::New("Argument Must Be A String")));
    return NULL;
  }

  String::Utf8Value val_string(val);
  char * val_char_ptr = (char *) malloc(val_string.length() + 1);
  strcpy(val_char_ptr, *val_string);
  return val_char_ptr;
}


Local<Value> encoded_to_v8(void* db_ptr, wg_int enc){
  wg_int type = wg_get_encoded_type(db_ptr, enc);
  switch(type){
  case WG_INTTYPE:
    return Int32::New(wg_decode_int(db_ptr, enc));
  case WG_NULLTYPE:
    return Local<Value>::New(Null());
  case WG_DOUBLETYPE:
    return Number::New(wg_decode_double(db_ptr, enc));
  case WG_STRTYPE:
    return String::New(wg_decode_str(db_ptr, enc));
  default:
    return Local<Value>::New(Undefined());
  }
}


wg_int v8_to_encoded(void* db_ptr, Local<Value> data){
  if(data->IsInt32()){
    return wg_encode_int(db_ptr, (int)data->Int32Value());
  } else if(data->IsNumber()){
    return wg_encode_double(db_ptr, data->NumberValue());
  } else if(data->IsString()){
    return wg_encode_str(db_ptr, get_str(data->ToString()), NULL);
  } else{
    return wg_encode_null(db_ptr, 0);
  }
}
