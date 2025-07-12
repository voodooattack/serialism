import { assert, expect } from "chai";
import { Serialism } from "..";

const mySymbol = Symbol.for("mySymbol");

class TestDummy {
  public [mySymbol] = "hello world";
  constructor(public data: string) {}
}

class TestDummy2 extends TestDummy {
  public instance: TestDummy = this;
  public data2: string;

  constructor(data: string) {
    super(data);
    this.data2 = data;
  }

  get data3() {
    return this.data;
  }

  set data3(v) {
    this.data = v;
  }
}

class TestDummy3 extends TestDummy2 {
  public data4: string;

  constructor(data: string) {
    super(data);
    this.data4 = data;
  }
}

describe("Basic functionality", function () {
  it("class is exported", function () {
    assert.isFunction(Serialism);
  });

  it("default constructor works", function () {
    const serializer = new Serialism();
    assert.instanceOf(serializer, Serialism);
  });

  it('doesn\'t allow SharedArrayBuffer', function () {
    const serializer = new Serialism();
    expect(() => serializer.serialize(new SharedArrayBuffer())).to.throw(
      "<SharedArrayBuffer> could not be cloned."
    );
  });

  it('can serialize and deserialize a native error object', function () {
    const serializer = new Serialism().register(TypeError);
    const error = new TypeError("Test error");
    const data = serializer.serialize(error);
    const result = serializer.deserialize(data);
    assert.instanceOf(result, Error);
    assert.instanceOf(result, TypeError);
    assert.strictEqual(result.message, "Test error");
    assert.strictEqual(result.name, "TypeError");
    assert.strictEqual(result.stack, error.stack);
  });

  it('cannot serialize bare symbols', function () {
    const serializer = new Serialism();
    const sym = Symbol.for("test");
    expect(() => serializer.serialize(sym)).to.throw(
      "Data clone error"
    );
  });

  it("can (de)serialize basic values", function () {
    const serializer = new Serialism();
    const data = serializer.serialize("hello");
    assert.equal(serializer.deserialize(data), "hello");
  });

  it("can handle a complex graph w/custom classes", function () {
    const serializer = new Serialism().register(TestDummy, TestDummy2, TestDummy3);
    const sharedInstance = new TestDummy2("value");
    const dummy3 = new TestDummy3("test");
    dummy3.data4 = "xxxx";
    dummy3.instance = sharedInstance;
    const target = {
      test: new TestDummy("hello"),
      dummy3,
      sharedInstance,
      map: new Map([["shared", sharedInstance]]),
      set: new Set([sharedInstance]),
      buffer: new Uint8Array([1, 2, 3]),
      arrayBuffer: new ArrayBuffer(8),
      arrayBufferView: new Uint8Array([1, 2, 3]),   
      array: [sharedInstance, 1, 2, 3, "test", Math.PI, new Date()] as [
        TestDummy2,
        number,
        number,
        number,
        string,
        number,
        Date
      ],
      regexp: /hello-world/g,
    };
    const data = serializer.serialize(target);
    const result = serializer.deserialize<typeof target>(data);
    assert.deepEqual(target, result);
    assert.notStrictEqual(target, result);
    assert.instanceOf(result.test, TestDummy);
    assert.instanceOf(result.sharedInstance, TestDummy2);
    assert.strictEqual(
      target.sharedInstance.data3,
      result.sharedInstance.data3
    );
    assert.strictEqual(result.sharedInstance.instance, result.sharedInstance);
    assert.strictEqual(result.sharedInstance, result.array[0].instance);
    target.sharedInstance.data3 = "different";
    assert.notEqual(target.sharedInstance.data3, result.sharedInstance.data3);
  });

  it("serialize class instance", function () {
    const serializer = new Serialism().register(TestDummy, TestDummy2);
    const target = new TestDummy2("value");
    const data = serializer.serialize(target);
    const result = serializer.deserialize<TestDummy2>(data);
    assert.instanceOf(result, TestDummy);
    assert.instanceOf(result, TestDummy2);
    assert.deepStrictEqual(target, result);
    assert.strictEqual(result.instance, result);
  });

  it("global symbols work", function () {
    const serializer = new Serialism();
    const sym = Symbol.for("test");
    const target = {
      [sym]: "hello symbols",
      sym,
    };
    const data = serializer.serialize(target);
    const result = serializer.deserialize(data);
    assert.deepStrictEqual(target, result);
    assert.strictEqual(target[sym], result[sym]);
  });

  it("private symbols do not work but global ones do", function () {
    const serializer = new Serialism();
    const localSym = Symbol();
    expect(() => serializer.serialize({ [localSym]: "private symbol" })).to.throw(
      "Failed to serialize"
    );
    expect(() => serializer.serialize({ sym: localSym })).to.throw(
      "Failed to serialize"
    );
    const targetGlobal = { [mySymbol]: "global symbol", sym: mySymbol };
    expect(() => serializer.serialize(targetGlobal)).to.not.throw();
  });

  it('does not serialize unknown classes', function () {
    const serializer = new Serialism();
    const target = new class A {v = "test";};
    expect(() => serializer.serialize(target)).to.throw(
      "No registered class found for " + target.constructor.name
    );
  });

  it('does not deserialize unknown classes', function () {
    const serializer = new Serialism().register(TestDummy, TestDummy2);
    const result = serializer.serialize(new TestDummy2("test"));
    const deserializer = new Serialism();    
    expect(() => deserializer.deserialize<TestDummy2>(result)).to.throw(
      "No registered class found for: TestDummy2"
    );
  });

  it("can register classes", function () {
    const serializer = new Serialism();
    expect(() => serializer.register(TestDummy, TestDummy2)).to.not.throw();
    expect(() => serializer.register(TestDummy)).to.not.throw();
    expect(() => serializer.register(TestDummy2)).to.not.throw();
  });

  it("can register multiple classes at once", function () {
    const serializer = new Serialism();
    expect(() => serializer.register(TestDummy, TestDummy2, class A {})).to.not.throw();
  });

  it('fails if two classes with the same name are registered', function () {
    const serializer = new Serialism();
    expect(() => serializer.register(TestDummy, TestDummy2, class TestDummy {})).to.throw(
      "A different class with the name 'TestDummy' is already registered"
    );
  });
});
