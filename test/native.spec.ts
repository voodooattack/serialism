import { assert } from "chai";
import { deserializeNative, serializeNative } from "..";

describe("TestNative", () => {
  it("native functions are exported", () => {
    assert.isFunction(deserializeNative);
    assert.isFunction(serializeNative);
  });

  it("serializeNative works", () => {
    const result = serializeNative("test");
    assert(result);
    assert.instanceOf(result, Buffer);
  });

  it("deserializeNative works", () => {
    const data = ["hello", "world"];
    const result = serializeNative(data);
    assert(result);
    assert.instanceOf(result, Buffer);
    assert.deepEqual(data, deserializeNative(result));
  });

  it("can (de)serialize primitives and complex nested cyclic references", () => {
    const target: any = {
      undef: undefined,
      nullVal: null,
      negativeZero: -0,
      test: "hello world",
      array: [1, 2, 3, "test", Math.PI, new Date()],
      map: new Map<any, any>([
        ["hi", 3],
        [Infinity, "test"],
        [NaN, null],
      ]),
      set: new Set<any>([1, 2, 3, +0, null, undefined]),
      regexp: /hello-world/g,
      instance: new Dummy(),
      str: new String("string object"), // tslint:disable-line
      num: new Number(100), // tslint:disable-line
      object: {
        "test long key": "value",
        emptyString: "",
      },
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
  });
  it(`check for memory leaks${
    typeof gc === "undefined" ? " (skipping because gc is not exposed)" : ""
  }`, function () {
    if (typeof gc === "undefined") {
      console.warn(
        "Garbage collector is not available, skipping memory leak test"
      );
      return this.skip();
    }
    gc();
    const memBefore = process.memoryUsage();
    for (let i = 0; i < 1024; i++) {
      const buffer = serializeNative(new ArrayBuffer(1024));
      assert.isAbove(buffer.byteLength, 1024);
    }
    gc();
    const memAfter = process.memoryUsage();
    assert.isAtMost(
      memAfter.external - memBefore.external,
      1024 * 1024 * 5,
      "possible memory leak detected"
    );
  });
});
class Dummy {
  public value = "hello";
}

