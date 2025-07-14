# serialism

Serialize complex JavaScript objects and ES classes with circular dependencies natively.

[![npm](https://img.shields.io/npm/v/serialism.svg)](https://www.npmjs.com/package/serialism)
[![GitHub license](https://img.shields.io/github/license/voodooattack/serialism.svg)](https://github.com/voodooattack/serialism/blob/master/LICENSE)
[![GitHub issues](https://img.shields.io/github/issues/voodooattack/serialism.svg)](https://github.com/voodooattack/serialism/issues)
[![Build Status](https://travis-ci.org/voodooattack/serialism.svg?branch=master)](https://travis-ci.org/voodooattack/serialism) [![Coverage Status](https://coveralls.io/repos/github/voodooattack/serialism/badge.svg)](https://coveralls.io/github/voodooattack/serialism)
![npm type definitions](https://img.shields.io/npm/types/serialism.svg)

## Important Warning

The underlying v8 API used by this native addon is still experimental, and binary compatibility may not be guaranteed between data output by the same application running in two different node versions.

If you plan to use this over a wire, please bear that in mind.

## Compatibility

- node.js >= v20.19.3 (LTS/Jod)

## Installation

### Prerequisites

Please make sure you have the following installed:

- Python
- Make
- C++ compiler

Use one of the following commands depending on your platform.

#### Linux

You need `g++`, `make` and `python` installed.

##### Ubuntu/Debian

`sudo apt-get install -y build-essential python`

##### Fedora

`dnf install gcc-c++ make`

##### RHEL and CentOS

`yum install -y gcc-c++ make`

#### Windows

`npm install -g --production windows-build-tools`

## Usage

Use `npm install serialism` to install the npm package.

### Simple Example

```typescript
const buffer = new Serialism().serialize([1,2,3])
const deserialized = new Serialism().deserialize(buffer); 
console.log(deserialized) // [1, 2, 3]
```

#### So, why would I use this over `JSON.stringify`?

The biggest difference about serialism—aside from extended type support—is that it preserves an object's graph, i.e if you do this:
```js
const array = [1, 2, 3]
const buffer = serialism.serialize({arr1: array, arr2: array})
```
If you save the buffer and deserialize it later on, then—unlike `JSON`—the following holds true:
```js
const {arr1, arr2} = serialism.deserialize(buffer)
// Check for strict equality
console.log(arr1 === arr2); // true
```

Another tangential benefit is compactness. Serialism returns a node `Buffer` object, containing the object graph in a compact binary format.

Duplicate objects (i.e objects that exhibit strict equality) are stored using references, so no single value is ever stored twice.

The binary format is also useful if you wish to send it to a Worker or over an IPC mechanism while preserving its structure.

### More Complex Example
```typescript
import { assert } from 'chai';
import { Serialism } from 'serialism';
import { Class1 } from './Class1';

const serialism = new Serialism();

// Register a class.
serialism.register(Class1);

const sharedArray = ['value'];

const target = {
  undef: undefined,
  nullVal: null,
  negativeZero: -0,
  test: 'hello world',
  array: [1, 2, 3, 'test', Math.PI, new Date()],
  buffer: new Uint8Array([1, 2, 3]),
  arrayBuffer: new ArrayBuffer(8),
  arrayBufferView: new Uint8Array([1, 2, 3]),
  map: new Map<any, any>([['hi', 3], [Infinity, 'test'], [NaN, null]]),
  set: new Set<any>([1, 2, 3, +0, null, undefined]),
  regexp: /hello-world/g,
  instance: new Class1(),
  str: new String('string object instance'),
  num: new Number(100),
  object: {
    'test long key': 'value',
    emptyString: '',
    sharedArray,
  },
  sharedArray,
  myClass: new Class1()
};

// create some basic and deeply nested cyclic dependencies
target.cyclic = target;
target.map.set(target, target.set);
target.set.add(target);
target.map.set(target.array, target.instance);

const data = serialism.serialize(target);
const result = serialism.deserialize<typeof target>(result);

assert.deepEqual(target, result); // true
assert.strictEquals(result.cyclic, result.map.get(result.cyclic)) // true
assert.strictEquals(result.cyclic, result.map.get(result.cyclic)) // true
assert.strictEquals(result.cyclic, result.map.get(result.array)) // true
assert.strictEquals(result.sharedArray, result.object.sharedArray) // true
```

#### JavaScript

```js
const Serialism = require('serialism').Serialism;
const Class1 = require('./Class1');

const serialism = new Serialism().register(Class1);

// serialize an object, an array, a regex, a set, a map, a primitive value, anything works except local symbols
const data = serialism.serialize([
  1,
  2,
  NaN,
  new Map(),
  new Set(),
  /my-regex-here/g,
  new Class1()
]);

console.log(Buffer.isBuffer(data)); // true;

const deserialized = serialism.deserialize(data); // deserialize the data

assert.deepEqual(data, deserialized); // true
```

### Error Handling

- All classes must be registered to be proccessed. Serialism will throw if you attempt to serialize an unknown class.
```typescript
import {assert} from 'chai';
import {Serialism} from 'serialism';

const serializer = new Serialism();

assert.throws(() => serializer.serialize(
    new class MyUnregisteredClass {}
  ), 
  "No registered class found for MyUnregisteredClass"
);

```
- Serialism will throw if you attempt to deserialize an unknown class.
```typescript
import {assert} from 'chai';
import {Serialism} from 'serialism';

class RegisteredClass {}

serializer.register(RegisteredClass);

const result = serializer.serialize(new RegisteredClass());

assert.throws(
  // A new deserializer instance does not know the class.
  () => new Serialism().deserialize(result), 
  "No registered class found for: RegisteredClass"
);
```

- You can't register two different classes with the same name.
```typescript
import {assert} from 'chai';
import {Serialism} from 'serialism';

assert.throws(() => {
  const serializer2 = new Serialism();
  serializer2.register(class RegisteredClass {});
  serializer2.register(class RegisteredClass {});
}, "A different class with the name 'RegisteredClass' is already registered.");
```

### Contributions

All contributions and pull requests are welcome.

If you have something to suggest or an idea you'd like to discuss, then please submit an issue or a pull request.

Note: Please commit your changes using `npm run commit` to trigger `conventional-changelog`.

### License (MIT)

> Copyright (c) 2018 Abdullah A. Hassan
> 
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
> 
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.
