import { assert } from 'chai';
import { suite, test } from 'mocha-typescript';
import { deserializeNative, serializeNative } from '../src/serialism';

class Dummy {
  public value = 'hello';
}

@suite
export class TestNative {
  @test
  public 'native functions are exported'() {
    assert.isFunction(deserializeNative);
    assert.isFunction(serializeNative);
  }

  @test
  public 'serializeNative works'() {
    const result = serializeNative('test');
    assert(result);
    assert.instanceOf(result, Buffer);
  }

  @test
  public 'deserializeNative works'() {
    const data = ['hello', 'world'];
    const result = serializeNative(data);
    assert(result);
    assert.instanceOf(result, Buffer);
    assert.deepEqual(data, deserializeNative(result));
  }

  @test
  public 'can (de)serialize primitives and complex nested cyclic references'() {
    const target: any = {
      undef: undefined,
      nullVal: null,
      negativeZero: -0,
      test: 'hello world',
      array: [1, 2, 3, 'test', Math.PI, new Date()],
      map: new Map<any, any>([['hi', 3], [Infinity, 'test'], [NaN, null]]),
      set: new Set<any>([1, 2, 3, +0, null, undefined]),
      regexp: /hello-world/g,
      instance: new Dummy(),
      str: new String('string object'), // tslint:disable-line
      num: new Number(100), // tslint:disable-line
      object: {
        'test long key': 'value',
        emptyString: ''
      }
    };

    target.cyclic = target;

    target.map.set(target, target.set);
    target.set.add(target);
    target.map.set(target.array, target.instance);

    const data = serializeNative(target);

    assert.instanceOf(data, Buffer);

    const result = deserializeNative(data);

    assert.isNotEmpty(result);
    assert.deepStrictEqual(target, result);
  }
}
