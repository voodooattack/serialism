import bindings = require('bindings');

/**
 * @ignore
 * @hidden
 * @internal
 */
const native = bindings('serialism');

/**
 * Serialize a JavaScript value.
 * @param value The value to serialize.
 * @returns A buffer containing the serialized data.
 * @throws Throws an error if a non-serializable is encountered. (Symbol, Native object, etc)
 */
export function serializeNative(value: any): Buffer {
  return native.serializeNative(value);
}

/**
 * Deserialize a NodeJS.Buffer to a JavaScript value.
 * @param buffer The buffer to deserialize.
 * @returns The deserialized object.
 * @throws Throws an error if the buffer is incompatible or malformed.
 */
export function deserializeNative<T = any>(buffer: Buffer): T {
  return native.deserializeNative(buffer);
}
