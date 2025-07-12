#include <nan.h>

NAN_METHOD(serializeNative)
{
  using namespace v8;
  Local<Context> context = Nan::GetCurrentContext();
  Isolate* isolate = context->GetIsolate();
  Nan::HandleScope scope;

  ValueSerializer serializer(isolate);

  serializer.WriteHeader();

  if (serializer.WriteValue(context, info[0])
          .FromMaybe(false))
  {
    auto data = serializer.Release();

    auto buffer = Nan::NewBuffer((char *)data.first, data.second,
                                 [](char *data, void *hint) {
                                   free(data);
                                 },
                                 nullptr);
    info.GetReturnValue().Set(buffer.ToLocalChecked());
  }
  else
  {
    Nan::ThrowError("could not serialize value");
  }
}

NAN_METHOD(deserializeNative)
{
  using namespace v8;
  Local<Context> context = Nan::GetCurrentContext();
  Isolate* isolate = context->GetIsolate();
  Nan::HandleScope scope;

  if (!node::Buffer::HasInstance(info[0]))
  {
    Nan::ThrowError("argument must be a Buffer instance");
    return;
  }

  ValueDeserializer deserializer(
      isolate,
      (uint8_t *)node::Buffer::Data(info[0]),
      node::Buffer::Length(info[0]));

  if (!deserializer.ReadHeader(context).FromMaybe(false))
  {
    Nan::ThrowError("invalid buffer");
    return;
  }

  Local<Value> value =
      deserializer.ReadValue(context).ToLocalChecked();

  info.GetReturnValue().Set(value);
}

NAN_MODULE_INIT(init)
{
  NAN_EXPORT(target, serializeNative);
  NAN_EXPORT(target, deserializeNative);
}

NODE_MODULE(serialism, init)
