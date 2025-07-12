import bindings from 'bindings';

declare class Serialism {
  /**
   * Serialize a JavaScript value.
   * @param value The value to serialize.
   * @returns A buffer containing the serialized data.
   * @throws Throws an error if a non-serializable is encountered. (Symbol, Native object, etc)
   */
  public serialize(value: unknown): Buffer;

  /**
   * Deserialize a NodeJS.Buffer to a JavaScript value.
   * @param buffer The buffer to deserialize.
   * @returns The deserialized object.
   * @throws Throws an error if the buffer is incompatible or malformed.
   */
  public deserialize<T = any>(buffer: Buffer): T;

  /**
   * Register class constructors for serialization/deserialization.
   * @param classes Class constructors
   */
  public register(...classes: Array<new (...args: any[]) => any>): this;
}

const SerialismInstance = bindings('serialism').Serialism as typeof Serialism;

export {SerialismInstance as Serialism};

export default SerialismInstance;
