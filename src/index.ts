import bindings from 'bindings';

/**
 * Serialism is a library for serializing and deserializing JavaScript values.
 * It supports a wide range of data types, including objects, arrays, and primitive values.
 * It is designed to be fast and efficient, making it suitable for high-performance applications.
 * @example
 * ```typescript
 * import {Serialism} from 'serialism';
 * const serialism = new Serialism();
 * const buffer = serialism.serialize({ foo: 'bar' });
 * const obj = serialism.deserialize(buffer);
 * console.log(obj); // { foo: 'bar' }
 * ```
 */
declare class Serialism {
  /**
   * Serialize a JavaScript value.
   * @param value The value to serialize.
   * @returns A `Buffer` instance containing the serialized data.
   * @throws Throws an error if a non-serializable is encountered.
   *   This includes non-global symbol, native object, unregistered class, etc)
   */
  public serialize(value: unknown): Buffer;

  /**
   * Deserialize a NodeJS.Buffer to a JavaScript value.
   * @param buffer The buffer to deserialize.
   * @returns The deserialized object.
   * @throws Throws an error if the buffer is incompatible or malformed.
   * @throws Throws an error if a non-registered class is encountered.
   */
  public deserialize<T = any>(buffer: Buffer): T;

  /**
   * Register class constructors for serialization/deserialization.
   * @param classes Class constructors
   * @returns The Serialism instance for chaining.
   * @throws Throws an error if a class is already registered.
   * @example
   * ```typescript
   * import {Serialism} from 'serialism';
   * class MyClass {
   *   constructor(public name: string) {}
   * }
   * const serialism = new Serialism();
   * serialism.register(MyClass);
   * const buffer = serialism.serialize(new MyClass('example'));
   * const obj = serialism.deserialize(buffer);
   * console.log(obj); // MyClass { name: 'example' }
   * ```
   */
  public register(...classes: Array<new (...args: any[]) => any>): this;
}

const SerialismInstance = bindings('serialism').Serialism as typeof Serialism;

export { SerialismInstance as Serialism };

export default SerialismInstance;
