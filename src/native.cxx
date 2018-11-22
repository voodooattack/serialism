#include <nan.h>
#include <iostream>

NAN_METHOD(serializeNative)
{
  v8::Isolate *isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);

  v8::ValueSerializer serializer(isolate);

  serializer.WriteHeader();

  if (serializer.WriteValue(isolate->GetEnteredContext(), info[0])
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
  v8::Isolate *isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);

  if (!node::Buffer::HasInstance(info[0]))
  {
    Nan::ThrowError("argument must be a Buffer instance");
    return;
  }

  v8::ValueDeserializer deserializer(
      isolate,
      (uint8_t *)node::Buffer::Data(info[0]),
      node::Buffer::Length(info[0]));

  if (!deserializer.ReadHeader(isolate->GetEnteredContext()).FromMaybe(false))
  {
    Nan::ThrowError("invalid buffer");
    return;
  }

  v8::Local<v8::Value> value =
      deserializer.ReadValue(isolate->GetEnteredContext()).ToLocalChecked();

  info.GetReturnValue().Set(value);
}

NAN_MODULE_INIT(init)
{
  NAN_EXPORT(target, serializeNative);
  NAN_EXPORT(target, deserializeNative);
}

NODE_MODULE(serialism, init)
