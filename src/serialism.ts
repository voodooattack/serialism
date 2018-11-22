import { deserializeNative, serializeNative } from './native';

export { serializeNative, deserializeNative };

/**
 * Interface representing a constructor.
 */
export interface IConstructor<T = any> {
  new (...args: any[]): T;
}

/**
 * Main library interface.
 */
export class Serialism {
  /**
   * Default constructor.
   * @param  classes An array of classes to support, if you (de)serialize
   * a value containing a class not contained  here, it will be treated as
   * a plain object.
   */
  constructor(public readonly classes: ReadonlyArray<IConstructor> = []) {}

  /**
   * Serialize any JavaScript value.
   * @param value A JavaScript value.
   * @returns A NodeJS.Buffer object containing the serialized
   * binary data.
   */
  public serialize(value: any): Buffer {
    return serializeNative(this.entomb(value));
  }

  /**
   * Deserialize a NodeJS.Buffer into a JavaScript value.
   * @param buffer The buffer containing serialized data.
   * @returns The deserialized data.
   */
  public deserialize<T = any>(buffer: Buffer): T {
    return this.revive(deserializeNative(buffer));
  }

  /**
   * Responsible for preparing all values for serialization.
   * Adds support for custom classes and global symbols.
   * @protected
   * @hidden
   * @param value The value.
   * @param known A map of known values.
   * @returns The entombed value.
   */
  protected entomb(value: any, known: WeakMap<any, any> = new WeakMap()): any {
    if (known.has(value)) {
      return known.get(value);
    }

    if (Array.isArray(value)) {
      const values: any[] = [];
      known.set(value, values);
      for (const val of value) {
        values.push(this.entomb(val, known));
      }
      return values;
    }

    if (typeof value === 'symbol') {
      const key = Symbol.keyFor(value);
      /* istanbul ignore else */
      if (key) {
        const result = ['@serialism:sym', key];
        return result;
      }
      // ignore local symbols
    }

    if (
      typeof value !== 'object' ||
      value instanceof Date ||
      value instanceof RegExp ||
      value instanceof String ||
      value instanceof Number
    ) {
      return value;
    }

    if (value instanceof Map) {
      const result = new Map<any, any>();
      known.set(value, result);
      for (const [key, val] of value) {
        result.set(this.entomb(key, known), this.entomb(val, known));
      }
      return result;
    } else if (value instanceof Set) {
      const result = new Set<any>();
      known.set(value, result);
      for (const entry of value) {
        result.add(this.entomb(entry, known));
      }
      return result;
    }

    let encoded: any = Object.assign({}, value);
    const type = this.classes.find(t => value.constructor === t);
    if (type) {
      encoded = ['@serialism:type', type.name, Object.entries(value)];
    }
    known.set(value, encoded);
    for (const [key, val] of Object.entries(encoded)) {
      encoded[key] = this.entomb(val, known);
    }

    for (const sym of Object.getOwnPropertySymbols(value)) {
      const key = Symbol.keyFor(sym);
      if (key) {
        encoded[`@serialism:sym:${key}`] = this.entomb(value[sym], known);
      }
    }

    return encoded;
  }
  /**
   * Revives symbols and plain objects into known class instances.
   * @protected
   * @hidden
   * @param  value The value to revive.
   * @param  known A map of known values.
   * @returns Revived value.
   */
  protected revive(value: any, known: WeakMap<any, any> = new WeakMap()): any {
    if (known.has(value)) {
      return known.get(value);
    }
    const symMarker = '@serialism:sym:';
    if (
      Array.isArray(value) &&
      typeof value[0] === 'string' &&
      value[0].startsWith('@serialism:')
    ) {
      const directive = (value[0] as string).substr('@serialism:'.length);
      /* istanbul ignore else */
      if (directive === 'sym') {
        const [, key] = value;
        const sym = Symbol.for(key);
        known.set(value, sym);
        return sym;
      } else if (directive === 'type') {
        const [, name, serializedValue] = value;
        const type = this.classes.find(t => t.name === name);
        /* istanbul ignore if */
        if (!type) {
          throw new Error(`Unknown class-name: '${name}'`);
        }
        const instance = Object.create(type.prototype);
        known.set(value, instance);
        for (let [key, val] of serializedValue) {
          if (key.startsWith(symMarker)) {
            const k = key.substr(symMarker.length);
            key = Symbol.for(k);
          }
          instance[key] = this.revive(val, known);
        }
        return instance;
      } else {
        throw new Error(`Unknown directive ${directive}`);
      }
    }

    if (Array.isArray(value)) {
      const values: any[] = [];
      known.set(value, values);
      for (const val of value) {
        values.push(this.revive(val, known));
      }
      return values;
    }

    if (value instanceof Map) {
      const result = new Map<any, any>();
      known.set(value, result);
      for (const [key, val] of value) {
        result.set(this.revive(key, known), this.revive(val, known));
      }
      return result;
    } else if (value instanceof Set) {
      const result = new Set<any>();
      known.set(value, result);
      for (const val of value) {
        result.add(this.revive(val, known));
      }
      return result;
    }

    if (
      typeof value !== 'object' ||
      value instanceof Date ||
      value instanceof RegExp ||
      value instanceof String ||
      value instanceof Number
    ) {
      return value;
    }
    const newObject: any = {};
    known.set(value, newObject);
    for (const entry of Object.entries(value)) {
      let [key, val] = entry as any;
      if (key.startsWith(symMarker)) {
        const k = key.substr(symMarker.length);
        key = Symbol.for(k);
      }
      val = this.revive(val, known);
      newObject[key] = val;
    }
    return newObject;
  }
}
