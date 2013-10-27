#include <node.h>
#include <v8.h>

#include "wgdb.h"

using namespace node;
using namespace v8;

void init(Handle<Object> target){
  WgDB::Init(target);
  Record::Init();
}

NODE_MODULE(wgdb, init);
