import { assert } from 'chai';
import { suite, test } from 'mocha-typescript';
import { Serialism } from '../src/serialism';

const mySymbol = Symbol.for('mySymbol');

class TestDummy {
  public [mySymbol] = 'hello world';
  constructor(public data: string) {}
}

class TestDummy2 extends TestDummy {
  public readonly instance: TestDummy = this;

  constructor(public data2: string) {
    super(data2);
  }

  get data3() {
    return this.data;
  }

  set data3(v) {
    this.data = v;
  }
}

@suite
export class TestClasses {
  @test
  public 'class is exported'() {
    assert.isFunction(Serialism);
  }

  @test
  public 'default constructor works'() {
    const serializer = new Serialism();
    assert.instanceOf(serializer, Serialism);
  }

  @test
  public 'can (de)serialize basic values'() {
    const serializer = new Serialism();
    const data = serializer.serialize('hello');
    assert.equal(serializer.deserialize(data), 'hello');
  }

  @test
  public 'custom classes work'() {
    const serializer = new Serialism([TestDummy, TestDummy2]);
    const sharedInstance = new TestDummy2('value');
    const target = {
      test: new TestDummy('hello'),
      sharedInstance,
      map: new Map([['shared', sharedInstance]]),
      set: new Set([sharedInstance]),
      array: [sharedInstance, 1, 2, 3, 'test', Math.PI, new Date()],
      regexp: /hello-world/g
    };
    const data = serializer.serialize(target);
    const result = serializer.deserialize(data);
    assert.deepEqual(target, result);
    assert.notStrictEqual(target, result);
    assert.instanceOf(result.test, TestDummy);
    assert.instanceOf(result.sharedInstance, TestDummy2);
    assert.strictEqual(
      target.sharedInstance.data3,
      result.sharedInstance.data3
    );
    target.sharedInstance.data3 = 'different';
    assert.notEqual(target.sharedInstance.data3, result.sharedInstance.data3);
  }

  @test
  public 'serialize class instance'() {
    const serializer = new Serialism([TestDummy, TestDummy2]);
    const target = new TestDummy2('value');
    const data = serializer.serialize(target);
    const result = serializer.deserialize<TestDummy2>(data);
    assert.instanceOf(result, TestDummy);
    assert.instanceOf(result, TestDummy2);
    assert.deepStrictEqual(target, result);
    assert.strictEqual(result.instance, result);
  }

  @test
  public 'global symbols work'() {
    const serializer = new Serialism();
    const sym = Symbol.for('test');
    const target = {
      [sym]: 'hello symbols',
      sym
    };
    const data = serializer.serialize(target);
    const result = serializer.deserialize(data);
    assert.deepStrictEqual(target, result);
    assert.strictEqual(target[sym], result[sym]);
  }

  @test
  public 'local symbols do not work'() {
    const serializer = new Serialism();
    const sym = Symbol();
    const target = { [sym]: 'hello symbols' };
    const data = serializer.serialize(target);
    const result = serializer.deserialize(data);
    assert.isUndefined(result[sym]);
  }
}
