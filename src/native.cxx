#include "v8-container.h"
#include "v8-context.h"
#include "v8-local-handle.h"
#include "v8-template.h"
#include <nan.h>
#include <v8.h>

#ifdef SERIALISM_DEBUG
#include <cstdint>
#include <cstdlib>
#include <iostream>
#endif

using namespace v8;
using namespace node;

/**
 * Internal fields used by the Serialism instance.
 */
enum InternalFields : uint32_t {
  kSerialismInstance = 0, // Instance of Serialism
  kKnownClasses,          // Map for storing registered classes
  kInternalFieldCount     // Count of internal fields
};

namespace delegate {
enum CustomHostKeyKind : uint32_t {
  kString = 0, // String key
  kSymbol,     // Symbol key
  kNumber,     // Number key
};
enum CustomHostValueKind : uint32_t {
  vValue = 0, // Regular value
  vSymbol,    // Symbol value
  vSelf,      // Self-reference
};

class SerializeDelegate : public ValueSerializer::Delegate {
private:
  // A set to keep track of registered classes for serialization
  Local<Map> _registeredClasses;
  ValueSerializer *_serializer = nullptr;

  // Custom delegate implementation
public:
  SerializeDelegate(Isolate *isolate, Local<Map> classes)
      : _registeredClasses(classes) {}

  virtual ~SerializeDelegate() = default;

  void SetSerializer(ValueSerializer *serializer) {
    this->_serializer = serializer;
  }

  virtual void ThrowDataCloneError(Local<String> message) override {
    Isolate *isolate = Isolate::GetCurrent();
    Nan::ThrowError(String::Concat(
        isolate, Nan::New("Data clone error: ").ToLocalChecked(), message));
  }

  virtual bool HasCustomHostObject(Isolate *isolate) override { return true; }

  Local<Array> GetAllPropertyNames(Local<Context> context,
                                   Local<Object> object) {
    return object
        ->GetPropertyNames(context, v8::KeyCollectionMode::kOwnOnly,
                           v8::PropertyFilter::ALL_PROPERTIES,
                           v8::IndexFilter::kIncludeIndices)
        .ToLocalChecked();
  }

  MaybeLocal<Function> MatchHostObjectConstructor(Isolate *isolate,
                                                  Local<Object> value) {
#ifdef SERIALISM_DEBUG
    std::cout << "[Serializer] Getting host object constructor for class: "
              << *Nan::Utf8String(value->GetConstructorName()) << std::endl;
#endif
    auto context = isolate->GetCurrentContext();
    auto maybeProto = value->GetPrototype();
    if (maybeProto.IsEmpty() || !maybeProto->IsObject()) {
      return MaybeLocal<Function>();
    }
    auto maybeValueCtor = maybeProto.As<Object>()->Get(
        context, Nan::New("constructor").ToLocalChecked());
    if (maybeValueCtor.IsEmpty()) {
#ifdef SERIALISM_DEBUG
      std::cout << "[Serializer] No constructor found for object." << std::endl;
#endif
      return MaybeLocal<Function>(); // No constructor found
    }
    auto valueCtor = maybeValueCtor.ToLocalChecked().As<Function>();
    const Local<Array> array = _registeredClasses->AsArray();
    for (unsigned int i = 0; i < _registeredClasses->Size(); ++i) {
      // auto name = array->Get(isolate->GetCurrentContext(), i * 2)
      //                 .ToLocalChecked()
      //                 .As<Function>();
      auto classCtor = array->Get(isolate->GetCurrentContext(), i * 2 + 1)
                           .ToLocalChecked()
                           .As<Function>();
      if (valueCtor->StrictEquals(classCtor)) {
#ifdef SERIALISM_DEBUG
        std::cout << "[Serializer] Found matching constructor: "
                  << *Nan::Utf8String(classCtor->GetName()) << std::endl;
#endif
        return classCtor;
      }
    }
    return MaybeLocal<Function>();
  }

  bool HasSymbols(Local<Context> context, Local<Object> object) {
    auto keys = GetAllPropertyNames(context, object);
    for (unsigned int i = 0; i < keys->Length(); ++i) {
      Local<Value> key = keys->Get(context, i).ToLocalChecked();
      Local<Value> value = object->Get(context, key).ToLocalChecked();
      if (key->IsSymbol() || key->IsSymbolObject() || value->IsSymbol() ||
          value->IsSymbolObject()) {
        return true; // Found a symbol
      }
    }
    return false; // No symbols found
  }

  virtual Maybe<bool> IsHostObject(Isolate *isolate,
                                   Local<Object> value) override {
    Local<Context> context = isolate->GetCurrentContext();
    if (HasSymbols(context,
                   value)) { // Special handling for objects with symbols
      return Just(true);
    }
#ifdef SERIALISM_DEBUG
    std::cout << "[Serializer] Checking if value `"
              << *Nan::Utf8String(value->GetConstructorName())
              << "` is a host object, registery has "
              << _registeredClasses->Size() << " classes" << std::endl;
#endif
    auto globalObject = context->Global()
                            ->Get(context, Nan::New("Object").ToLocalChecked())
                            .ToLocalChecked()
                            .As<Object>();
    if (value->Get(context, Nan::New("constructor").ToLocalChecked())
            .ToLocalChecked()
            ->StrictEquals(globalObject) ||
        value->GetPrototype()->StrictEquals(globalObject->GetPrototype())) {
#ifdef SERIALISM_DEBUG
      std::cout
          << "[Serializer] Value is not a host object, prototype is Object."
          << std::endl;
#endif
      // If the prototype is Object, we check if it has a constructor
      return Just(false); // Not a host object
    }
    auto found = MatchHostObjectConstructor(isolate, value);
    if (found.IsEmpty()) {
// No matching constructor found
#ifdef SERIALISM_DEBUG
      std::cout << "[Serializer] No matching constructor found for object."
                << std::endl;
#endif
      isolate->ThrowError(String::Concat(
          isolate, Nan::New("No registered class found for ").ToLocalChecked(),
          value->GetConstructorName()));
      return Just(false); // Not a host object
    }
#ifdef SERIALISM_DEBUG
    std::cout << "[Serializer] Value is a host object." << std::endl;
#endif
    return Just(true); // It is a host object
  }

  Maybe<bool> WriteKey(Isolate *isolate, Local<Value> key) {
    auto context = isolate->GetCurrentContext();
    CustomHostKeyKind keyKind = CustomHostKeyKind::kString;
    if (key->IsSymbol() || key->IsSymbolObject()) {
      auto symbolDesc =
          key->IsSymbolObject()
              ? key.As<SymbolObject>()->ValueOf()->Description(isolate)
              : key.As<Symbol>()->Description(isolate);
      if (symbolDesc.IsEmpty()) {
        isolate->ThrowError(
            Nan::New("Failed to serialize a non-serializable value: Symbol")
                .ToLocalChecked());
#ifdef SERIALISM_DEBUG
        std::cout << "[Serializer] Failed to get symbol description, skipping."
                  << std::endl;
#endif
        return Nothing<bool>();
      }
      if (symbolDesc->IsNullOrUndefined()) {
        isolate->ThrowError(
            Nan::New("Failed to serialize a non-serializable value: Symbol")
                .ToLocalChecked());
        return Nothing<bool>();
      }
      key = symbolDesc;
      keyKind = CustomHostKeyKind::kSymbol;
    } else if (key->IsNumber()) {
      keyKind = CustomHostKeyKind::kNumber;
    } else if (!key->IsString()) {
      // If the key is not a string or symbol, we throw an error
#ifdef SERIALISM_DEBUG
      std::cout << "[Serializer] Key is not a string or symbol, skipping."
                << std::endl;
#endif
      isolate->ThrowError(
          Nan::New("Failed to serialize a non-serializable value: Key")
              .ToLocalChecked());
      return Nothing<bool>();
    }
    if (key.IsEmpty()) {
#ifdef SERIALISM_DEBUG
      std::cout << "[Serializer] Failed to get key value, skipping."
                << std::endl;
#endif
      isolate->ThrowError(
          Nan::New("Failed to serialize a non-serializable value: Key")
              .ToLocalChecked());
      return Nothing<bool>();
    }
    _serializer->WriteUint32(static_cast<uint32_t>(keyKind));
    return _serializer->WriteValue(context, key);
  }

  Maybe<bool> WriteValue(Isolate *isolate, Local<Object> object,
                         Local<Value> key, Local<Value> value) {
    auto context = isolate->GetCurrentContext();
    if (value.IsEmpty()) {
      return Nothing<bool>();
    }
    if (value->StrictEquals(object)) {
      // If the value is the same as the object, we write a special marker
      _serializer->WriteUint32(static_cast<uint32_t>(vSelf));
      return Just(true); // Skip writing the value, as it's a self-reference
    } else if (value->IsSymbol() || value->IsSymbolObject()) {
      // If the value is a Symbol object, we write its description
      _serializer->WriteUint32(static_cast<uint32_t>(vSymbol));
      auto symbolDesc =
          value->IsSymbolObject()
              ? value.As<SymbolObject>()->ValueOf()->Description(isolate)
              : value.As<Symbol>()->Description(isolate);
      if (symbolDesc.IsEmpty()) {
        isolate->ThrowError(
            Nan::New("Failed to serialize a non-serializable value: Symbol")
                .ToLocalChecked());
        return Nothing<bool>();
      }
      if (symbolDesc->IsNullOrUndefined()) {
        isolate->ThrowError(
            Nan::New("Failed to serialize a non-serializable value: Symbol")
                .ToLocalChecked());
        return Nothing<bool>();
      }
      return _serializer->WriteValue(context, symbolDesc);
    } else if (value->IsString()) {
      // If the value is a string, we write it as a string
      _serializer->WriteUint32(static_cast<uint32_t>(vValue));
    } else {
      // Otherwise, write the value normally
      _serializer->WriteUint32(static_cast<uint32_t>(vValue));
    }
    return _serializer->WriteValue(context, value);
  }

  virtual Maybe<bool> WriteHostObject(Isolate *isolate,
                                      Local<Object> object) override {
    auto context = isolate->GetCurrentContext();
    if (!_serializer) {
#ifdef SERIALISM_DEBUG
      std::cout << "[Serializer] Serializer is not set." << std::endl;
#endif
      isolate->ThrowError(Nan::New("Serializer is not set").ToLocalChecked());
      return Nothing<bool>();
    }
    auto maybeConstructor =
        object->Get(context, Nan::New("constructor").ToLocalChecked());
    if (!maybeConstructor.IsEmpty()) {
      auto constructor = maybeConstructor.ToLocalChecked();
      if (constructor->IsNull() ||
          constructor->StrictEquals( // Check if constructor is Object
              context->Global()
                  ->Get(context, Nan::New("Object").ToLocalChecked())
                  .ToLocalChecked())) {
        // If the constructor is null or undefined, we write a null value
#ifdef SERIALISM_DEBUG
        std::cout << "[Serializer] Null constructor found for host object."
                  << std::endl;
#endif
        _serializer->WriteValue(context, Nan::Undefined())
            .Check(); // Write null if no constructor is found
      } else {
        // Get the constructor for the host object
        auto matchedConstructor = MatchHostObjectConstructor(isolate, object);
        if (matchedConstructor.IsEmpty()) {
#ifdef SERIALISM_DEBUG
          std::cout << "[Serializer] No constructor found for host object."
                    << std::endl;
#endif
          isolate->ThrowError(
              Nan::New("No constructor found for object").ToLocalChecked());
          return Nothing<bool>();
        }
        auto constructor = matchedConstructor.ToLocalChecked();
        auto className = constructor->GetName();
#ifdef SERIALISM_DEBUG
        std::cout << "[Serializer] Found constructor for host object: "
                  << *Nan::Utf8String(className) << std::endl;
#endif
        // Write the constructor's name to the serializer
        if (auto res = _serializer->WriteValue(context, className);
            !res.FromMaybe(false)) {
#ifdef SERIALISM_DEBUG
          std::cout
              << "[Serializer] Failed to write host object constructor data."
              << std::endl;
#endif
          isolate->ThrowError(
              Nan::New("Failed to write host object constructor data")
                  .ToLocalChecked());
          return res;
        }
      }
    } else {
#ifdef SERIALISM_DEBUG
      std::cout << "[Serializer] No constructor found for host object"
                << std::endl;
#endif
      // If no constructor is found, we write an null value
      _serializer->WriteValue(context, Nan::Null()).Check();
    }
#ifdef SERIALISM_DEBUG
    std::cout << "[Serializer] Writing host object." << std::endl;
#endif
    auto keys = GetAllPropertyNames(context, object);

    _serializer->WriteUint32(keys->Length());

    for (unsigned int i = 0; i < keys->Length(); ++i) {
      Local<Value> key = keys->Get(context, i).ToLocalChecked();
      Local<Value> value;
      if (!object->Get(context, key).ToLocal(&value)) {
#ifdef SERIALISM_DEBUG
        std::cout << "[Serializer] Failed to get property value for key: "
                  << *Nan::Utf8String(key) << std::endl;
#endif
        continue;
      }
      if (auto res = WriteKey(isolate, key); !res.FromMaybe(false)) {
#ifdef SERIALISM_DEBUG
        std::cout << "[Serializer] Failed to write host object property key."
                  << std::endl;
#endif
        return res;
      }
      if (auto res = WriteValue(isolate, object, key, value);
          !res.FromMaybe(false)) {
#ifdef SERIALISM_DEBUG
        std::cout
            << "  [Serializer] Failed to write host object property value."
            << std::endl;
#endif
        return res;
      }
    }
    return Just(true);
  }
};

class DeserializeDelegate : public ValueDeserializer::Delegate {
private:
  Local<Map> _registeredClasses;
  ValueDeserializer *_deserializer = nullptr;

public:
  DeserializeDelegate(Local<Map> registeredClasses)
      : _registeredClasses(registeredClasses) {}
  virtual ~DeserializeDelegate() = default;
  void SetDeserializer(ValueDeserializer *deserializer) {
    this->_deserializer = deserializer;
  }

  MaybeLocal<Function> GetHostObjectConstructorByName(Isolate *isolate,
                                                      Local<String> className) {
    const Local<Array> array = _registeredClasses->AsArray();
    for (unsigned int i = 0; i < _registeredClasses->Size(); ++i) {
      Local<String> name = array->Get(isolate->GetCurrentContext(), i * 2)
                               .ToLocalChecked()
                               .As<String>();
      Local<Function> func = array->Get(isolate->GetCurrentContext(), i * 2 + 1)
                                 .ToLocalChecked()
                                 .As<Function>();
      if (name->StrictEquals(className)) {
#ifdef SERIALISM_DEBUG
        std::cout << "[Deserializer] Found matching constructor: "
                  << *Nan::Utf8String(func->GetName()) << std::endl;
#endif
        return func;
      }
    }
    return MaybeLocal<Function>();
  }

  bool ReadKey(Isolate *isolate, Local<Value> *key) {
    auto context = isolate->GetCurrentContext();
    uint32_t keyKind = 0;
    if (!_deserializer->ReadUint32(&keyKind)) {
#ifdef SERIALISM_DEBUG
      std::cerr << "[Deserializer] Failed to read key kind." << std::endl;
#endif
      isolate->ThrowError("Failed to read key kind");
      return false;
    }
    switch (keyKind) {
    case static_cast<uint32_t>(kString):
      if (!_deserializer->ReadValue(context).ToLocal(key)) {
#ifdef SERIALISM_DEBUG
        std::cerr << "[Deserializer] Failed to read string key." << std::endl;
#endif
        isolate->ThrowError("Failed to read string key");
        return false;
      }
      break;
    case static_cast<uint32_t>(kSymbol): {
      auto maybeSymbolDesc = _deserializer->ReadValue(context);
      if (maybeSymbolDesc.IsEmpty() ||
          !maybeSymbolDesc.ToLocalChecked()->IsString()) {
#ifdef SERIALISM_DEBUG
        std::cerr << "[Deserializer] Failed to read symbol description."
                  << std::endl;
#endif
        isolate->ThrowError("Failed to read symbol description");
        return false;
      }
      auto symbolDesc = maybeSymbolDesc.ToLocalChecked();
      *key = Symbol::For(isolate, symbolDesc.As<String>());
      break;
    }
    case static_cast<uint32_t>(kNumber): { // Read a number key
      double number;
      if (!_deserializer->ReadDouble(&number)) {
#ifdef SERIALISM_DEBUG
        std::cerr << "[Deserializer] Failed to read number key." << std::endl;
#endif
        isolate->ThrowError("Failed to read number key");
        return false;
      }
      *key = Nan::New(number);
      break;
    }
    default:
      isolate->ThrowError("Unknown key kind");
      return false;
    }
    return true;
  }

  bool ReadValue(Isolate *isolate, Local<Object> object, Local<Value> key,
                 Local<Value> *value) {
    auto context = isolate->GetCurrentContext();
    uint32_t valueKind = 0;
    if (!_deserializer->ReadUint32(&valueKind)) {
#ifdef SERIALISM_DEBUG
      std::cerr << "[Deserializer] Failed to read value kind." << std::endl;
#endif
      isolate->ThrowError("Failed to read value kind");
      return false;
    }
    switch (valueKind) {
    case static_cast<uint32_t>(vSelf): {
      // If the value is a self-reference, we set the object itself
      *value = object;
#ifdef SERIALISM_DEBUG
      std::cout << "[Deserializer] Set property `" << *Nan::Utf8String(key)
                << "` to self-reference." << std::endl;
#endif
      return true; // Skip reading the value, as it's a self-reference
    }
    case static_cast<uint32_t>(vSymbol): {
      // If the value is a symbol, we read its description
      auto maybeSymbolDesc = _deserializer->ReadValue(context);
      if (maybeSymbolDesc.IsEmpty() ||
          !maybeSymbolDesc.ToLocalChecked()->IsString()) {
#ifdef SERIALISM_DEBUG
        std::cerr << "[Deserializer] Failed to read symbol description."
                  << std::endl;
#endif
        isolate->ThrowError("Failed to read symbol description");
        return false;
      }
      auto symbolDesc = maybeSymbolDesc.ToLocalChecked();
      *value = Symbol::For(isolate, symbolDesc.As<String>());
      break;
    }
    case static_cast<uint32_t>(vValue): {
      // Otherwise, read the value normally
      if (!_deserializer->ReadValue(context).ToLocal(value)) {
#ifdef SERIALISM_DEBUG
        std::cerr << "[Deserializer] Value is empty after reading."
                  << std::endl;
#endif
        isolate->ThrowError("Value is empty after reading");
        return false;
      }
      break;
    }
    default:
#ifdef SERIALISM_DEBUG
      std::cerr << "[Deserializer] Unknown value kind: " << valueKind
                << std::endl;
#endif
      isolate->ThrowError(String::Concat(
          isolate, Nan::New("Unknown value kind: ").ToLocalChecked(),
          Nan::New<Uint32>(valueKind)->ToString(context).ToLocalChecked()));
      return false;
    }
    return true;
  }

  MaybeLocal<Object> ReadHostObject(Isolate *isolate) override {
    if (!_deserializer) {
#ifdef SERIALISM_DEBUG
      std::cerr << "[Deserializer] Deserializer is not set." << std::endl;
#endif
      return MaybeLocal<Object>();
    }

    Local<Context> context = isolate->GetCurrentContext();

    auto maybeClassName = _deserializer->ReadValue(context);

    if (maybeClassName.IsEmpty()) {
#ifdef SERIALISM_DEBUG
      std::cerr << "[Deserializer] Failed to deserialize host object. Could "
                   "not read class name."
                << std::endl;
#endif
      isolate->ThrowError(
          "Failed to deserialize host object: could not read class name");
      return MaybeLocal<Object>();
    }

    auto className = maybeClassName.ToLocalChecked();
    auto object = Object::New(isolate);

    if (className->IsUndefined()) {
#ifdef SERIALISM_DEBUG
      std::cout << "[Deserializer] Class name is undefined, creating empty "
                   "object."
                << std::endl;
#endif
      object
          ->SetPrototype(context,
                         context->Global()
                             ->Get(context, Nan::New("Object").ToLocalChecked())
                             .ToLocalChecked())
          .Check(); // Set the prototype to Object
    } else if (className->IsNull()) {
#ifdef SERIALISM_DEBUG
      std::cout << "[Deserializer] Class name is null, creating empty object."
                << std::endl;
#endif
      object->SetPrototype(context, Nan::Null()).Check();
    } else if (!className->IsNull()) {
      if (!className->IsString()) {
#ifdef SERIALISM_DEBUG
        std::cerr << "[Deserializer] Deserialized class name is not a string."
                  << std::endl;
#endif
        isolate->ThrowError("Deserialized class name is not a string");
        return MaybeLocal<Object>();
      }

      auto maybeConstructor =
          GetHostObjectConstructorByName(isolate, className.As<String>());

      if (maybeConstructor.IsEmpty()) {
#ifdef SERIALISM_DEBUG
        std::cerr
            << "[Deserializer] No constructor found for host object class: "
            << *Nan::Utf8String(maybeClassName.ToLocalChecked()) << std::endl;
#endif
        isolate->ThrowError(String::Concat(
            isolate,
            Nan::New("No registered class found for: ").ToLocalChecked(),
            className.As<String>()));
        return MaybeLocal<Object>();
      }

      auto constructor = maybeConstructor.ToLocalChecked();
#ifdef SERIALISM_DEBUG
      std::cout << "[Deserializer] Host object class found: "
                << *Nan::Utf8String(constructor->GetName()) << std::endl;
#endif

      auto proto =
          constructor->Get(context, Nan::New("prototype").ToLocalChecked());
      if (proto.IsEmpty() || !proto.ToLocalChecked()->IsObject()) {
#ifdef SERIALISM_DEBUG
        std::cerr << "[Deserializer] No prototype found for host object class: "
                  << *Nan::Utf8String(className.As<String>()) << std::endl;
#endif
        proto = constructor->GetPrototype();
      }
      object->SetPrototype(context, proto.ToLocalChecked())
          .Check(); // Set the prototype to the constructor's prototype
    }

    uint32_t propCount = 0;

    if (!_deserializer->ReadUint32(&propCount)) // Read the number of properties
    {
#ifdef SERIALISM_DEBUG
      std::cerr << "[Deserializer] Failed to read number of properties for "
                   "host object."
                << std::endl;
#endif
      isolate->ThrowError("Failed to read number of properties for object");
      return MaybeLocal<Object>();
    }

#ifdef SERIALISM_DEBUG
    std::cout << "[Deserializer] Number of properties to read: " << keyCount
              << std::endl;
#endif

    for (uint32_t i = 0; i < propCount; ++i) {
      Local<Value> key;
      Local<Value> value;

      if (!ReadKey(isolate, &key)) {
        return MaybeLocal<Object>();
      }

      if (!ReadValue(isolate, object, key, &value)) {
        return MaybeLocal<Object>();
      }

      if (!object->Set(context, key, value).ToChecked()) {
#ifdef SERIALISM_DEBUG
        std::cerr << "[Deserializer] Failed to set property " << i
                  << " of host object." << std::endl;
#endif
        isolate->ThrowError(String::Concat(
            isolate, Nan::New("Failed to set property ").ToLocalChecked(),
            Nan::New<Uint32>(i)->ToString(context).ToLocalChecked()));
        return MaybeLocal<Object>();
      }
    }
    return object;
  }
};
} // namespace delegate

bool checkIsSerialism(Local<Context> context, Local<Object> thisObject) {
  auto marker =
      thisObject->GetInternalField(InternalFields::kSerialismInstance);
  if (marker.IsEmpty() || !marker->IsValue() ||
      !marker.As<String>()->StrictEquals(
          Nan::New("SerialismInstance").ToLocalChecked())) {
    // If the marker is not set or does not match, we throw an error.
#ifdef SERIALISM_DEBUG
    std::cerr << "[Serialism] This object is not an instance of Serialism."
              << std::endl;
#endif
    Isolate *isolate = context->GetIsolate();
    isolate->ThrowError(
        "This object is not an instance of Serialism. Please create a new "
        "instance of Serialism before using its methods.");
    return false;
  }
  return true;
}

/**
 * Register a javascript class for serialization/deserialization.
 */
NAN_METHOD(registerClass) {
  Local<Context> ctx = Nan::GetCurrentContext();
  Isolate *isolate = ctx->GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  HandleScope scope(isolate);

  if (!checkIsSerialism(context, info.This())) {
    return; // If the object is not a Serialism instance, we throw an error.
  }

  auto count = info.Length();

  Local<Map> classes =
      info.This()->GetInternalField(InternalFields::kKnownClasses).As<Map>();

  for (int i = 0; i < count; ++i) {
    if (!info[i]->IsFunction()) {
      isolate->ThrowError("All arguments must be constructor functions");
      return;
    }

    auto arg = info[i];

    if (!arg->IsFunction()) {
      isolate->ThrowError("Argument must be a class");
      return;
    }

    Local<Function> constructor = arg.As<Function>();

    if (!constructor->IsConstructor()) {
      Nan::ThrowError("Argument must be a class");
      return;
    }

    // Register the constructor for serialization.

    auto name = constructor->GetName();

    if (name.IsEmpty() || !name->IsString() ||
        name.As<String>()->Length() == 0) {
      isolate->ThrowError("Class must have a name");
      return;
    }

    if (arg->IsProxy()) {
      isolate->ThrowError("Cannot register a proxy as a class");
      return;
    }

    if (classes->Has(ctx, name).FromJust()) {
      if (classes->Get(ctx, name).ToLocalChecked()->StrictEquals(constructor)) {
        // If the class is already registered, we skip it.
        continue;
      } else { // If two different classes share the same name, throw an error.
        isolate->ThrowError(String::Concat(
            isolate,
            String::Concat(
                isolate,
                Nan::New("A different class with the name '").ToLocalChecked(),
                name.As<String>()),
            Nan::New("' is already registered.").ToLocalChecked()));
        return;
      }
    }

    String::Utf8Value utf8Value(isolate, name);

#ifdef SERIALISM_DEBUG
    std::cout << "Registering class: " << *utf8Value << std::endl;
#endif

    classes->Set(isolate->GetCurrentContext(), name, constructor)
        .ToLocalChecked();
  }

  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(serializeNative) {
  Local<Context> context = Nan::GetCurrentContext();
  Isolate *isolate = context->GetIsolate();
  Nan::HandleScope scope;

  if (!checkIsSerialism(context, info.This())) {
    return; // If the object is not a Serialism instance, we throw an error.
  }

  if (info.Length() < 1) {
    isolate->ThrowError("Argument is required");
    return;
  }

  Local<Value> value = info[0];

  if (value->IsFunction()) {
    isolate->ThrowError("Cannot serialize functions");
    return;
  }

  Local<Map> classes =
      info.This()->GetInternalField(InternalFields::kKnownClasses).As<Map>();
  delegate::SerializeDelegate delegate(isolate, classes);
  ValueSerializer serializer(isolate, &delegate);

  delegate.SetSerializer(&serializer);

  serializer.WriteHeader();

  if (auto res = serializer.WriteValue(isolate->GetCurrentContext(), value);
      res.FromMaybe(false)) {
    auto [data, size] = serializer.Release();
    auto buffer =
        Nan::NewBuffer((char *)data, size,
                       [](char *data, void *hint) { free(data); }, nullptr);
    if (buffer.IsEmpty()) {
#ifdef SERIALISM_DEBUG
      std::cerr << "Error creating buffer from serialized data." << std::endl;
#endif
      isolate->ThrowError("Could not create buffer from serialized data");
      return;
    }
    info.GetReturnValue().Set(buffer.ToLocalChecked());
  } else if (!isolate->HasPendingException()) {
    isolate->ThrowError("Could not serialize value");
  }
}

NAN_METHOD(deserializeNative) {
  Isolate *isolate = Nan::GetCurrentContext()->GetIsolate();
  Local<Context> context = Nan::GetCurrentContext();
  Nan::HandleScope scope;

  if (!checkIsSerialism(context, info.This())) {
    return; // If the object is not a Serialism instance, we throw an error.
  }

  if (!node::Buffer::HasInstance(info[0])) {
    isolate->ThrowError("Argument must be a Buffer instance");
    return;
  }

  delegate::DeserializeDelegate delegate(
      info.This()->GetInternalField(InternalFields::kKnownClasses).As<Map>());
  ValueDeserializer deserializer(isolate,
                                 (uint8_t *)node::Buffer::Data(info[0]),
                                 node::Buffer::Length(info[0]), &delegate);

  delegate.SetDeserializer(&deserializer);

  if (!deserializer.ReadHeader(isolate->GetCurrentContext()).FromMaybe(false)) {
    Nan::ThrowError("Invalid data");
    return;
  }

  auto maybeValue = deserializer.ReadValue(isolate->GetCurrentContext());

  if (maybeValue.IsEmpty()) {
    if (!isolate->HasPendingException()) {
      isolate->ThrowError("Could not deserialize value");
    }
    return;
  }

  info.GetReturnValue().Set(maybeValue.ToLocalChecked());
}

NAN_METHOD(constructor) {
  Local<Context> context = Nan::GetCurrentContext();
  Isolate *isolate = context->GetIsolate();
  Nan::HandleScope scope;
  Local<Map> classes = Map::New(isolate);
  info.This()->SetInternalField(InternalFields::kSerialismInstance,
                                Nan::New("SerialismInstance").ToLocalChecked());
  info.This()->SetInternalField(InternalFields::kKnownClasses, classes);
  info.GetReturnValue().Set(info.This());
}

NAN_MODULE_INIT(module) {
  Local<Context> ctx = Nan::GetCurrentContext();
  Nan::HandleScope scope;
  Local<FunctionTemplate> ctor = Nan::New<FunctionTemplate>(&constructor);
  Local<ObjectTemplate> objTemplate = ctor->PrototypeTemplate();
  objTemplate->Set(Nan::New("register").ToLocalChecked(),
                   Nan::New<FunctionTemplate>(&registerClass));
  objTemplate->Set(Nan::New("serialize").ToLocalChecked(),
                   Nan::New<FunctionTemplate>(&serializeNative));
  objTemplate->Set(Nan::New("deserialize").ToLocalChecked(),
                   Nan::New<FunctionTemplate>(&deserializeNative));
  ctor->InstanceTemplate()->SetInternalFieldCount(
      InternalFields::kInternalFieldCount);
  ctor->SetClassName(Nan::New("Serialism").ToLocalChecked());
  Nan::Set(target, Nan::New("Serialism").ToLocalChecked(),
           ctor->GetFunction(ctx).ToLocalChecked());
}

NODE_MODULE(serialism, module)
