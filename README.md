# serialism

Serialize complex JavaScript objects or ES6 classes with circular dependencies natively.

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

#### Raw interface (no class deserialization)

This allows you to serialize and deserialize any JavaScript value, without the revival of classes and the processing of global symbols.

This interface is much faster because it will not traverse the object graph in JavaScript to entomb and revive values.

In this case, any class instances supplied will be serialized as plain objects.

```typescript
import { assert } from 'chai';
import { Serialism } from 'serialism';
import { Class1 } from '...';

const serialism = new Serialism();
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
  str: new String('string object instance'),
  num: new Number(100),
  object: {
    'test long key': 'value',
    emptyString: ''
  }
};

// create some basic and deeply nested cyclic dependencies
target.cyclic = target;
target.map.set(target, target.set);
target.set.add(target);
target.map.set(target.array, target.instance);

const data = serialism.serialize(target);

const result = serialism.deserialize(result);

assert.deepEqual(target, result); // true
```

#### TypeScript

```typescript
import { assert } from 'chai';
import { Serialism } from 'serialism';
import { Class1 } from '...';

const serialism = new Serialism();
// or if you plan on serializing a value containing ES6 classes:
// const serialism = new Serialism([Class1, Class2, ...]);

// serialize an object, an array, a regex, a primitive value, anything works except local symbols
const myArray = [1, 2, NaN, /my-regex-here/g, new Class1('ho there!')];

// cyclic and self-references will work
myArray.push(myArray);

const data = serialism.serialize(myArray);

console.log(Buffer.isBuffer(data)); // true;

const deserialized = serialism.deserialize<any>(data); // deserialize the data

assert.deepEqual(data, deserialized); // true, the clone is equal

assert.instanceOf(
  serialism.deserialize<Class1>(serialism.serialize(new Class1("I'm a class"))),
  Class1
); // true
```

#### JavaScript

```js
const Serialism = require('serialism').Serialism;
const Class1 = require('...');

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

assert.deepEqual(data, deserialized); // true, the clone is equal
```

### Contributions

All contributions and pull requests are welcome.

If you have something to suggest or an idea you'd like to discuss, then please submit an issue or a pull request.

Note: Please commit your changes using `npm run commit` to trigger `conventional-changelog`.

### License (MIT)

Copyright (c) 2018 Abdullah A. Hassan

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
