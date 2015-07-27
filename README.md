js-native-unordered-buffer-set
==============================

A native Node module implementing an unsorted set of C strings.

The goal is for super-duper-fast string matching. That means inputs and outputs
are always Buffers.

The implementation is a C++-standard unsorted_set. That's probably a hashtable.
But it's far, far faster than a Node hashtable.

Usage
-----

var NativeSet = require('js-native-unsorted-buffer-set');
// Input is a single Buffer: newline-separated Strings
var set = new NativeSet(new Buffer('foo\nbar\nbaz\nthe foo', 'utf-8'));

console.log(set.contains('foo')); // true
console.log(set.contains('moo')); // false

console.log(set.findAllMatches('the foo drove over the moo', 2)); // [ 'the foo', 'foo', 'moo' ]

findAllMatches
--------------

This method is interesting in that it can search for tokens that span multiple
words (the second argument specifies the number of words), in a memory-efficient
manner. The memory used is the size of the output Array. The time complexity is
on the order of the size of the input times the number of tokens.

Developing
----------

Download, `npm install`.

Run `mocha -w` in the background as you implement features. Write tests in
`test` and code in `src`.

LICENSE
-------

AGPL-3.0. This project is (c) Overview Services Inc. Please contact us should
you desire a more permissive license.
