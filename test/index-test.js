var Set = require('../index');
var expect = require('chai').expect;

describe('UnorderedBufferSet', function() {
  it('should return true/false from tests appropriately', function() {
    var set = new Set(new Buffer('foo\nbar\nbaz\nthe foo', 'utf-8'));
    expect(set.contains(new Buffer('foo', 'utf-8'))).to.be.true;
    expect(set.contains(new Buffer('the foo', 'utf-8'))).to.be.true;
    expect(set.contains(new Buffer('moo', 'utf-8'))).to.be.false;
    expect(set.contains(new Buffer('fooX', 'utf-8'))).to.be.false;
  });

  it('should allow testing for Strings, which are encoded as utf-8', function() {
    var set = new Set(new Buffer('foo\nbar\nbaz\nthe foo', 'utf-8'));
    expect(set.contains('foo')).to.be.true;
    expect(set.contains('the foo')).to.be.true;
    expect(set.contains('moo')).to.be.false;
    expect(set.contains('fooX')).to.be.false;
  });

  it('should return a String Array of found tokens', function() {
    var set = new Set(new Buffer('foo\nbar\nbaz\nthe foo\nmoo', 'utf-8'));

    expect(set.findAllMatches('the foo went over the moo', 2))
      .to.deep.eq([ 'the foo', 'foo', 'moo' ]);

    expect(set.findAllMatches('the foo went over the moo', 1))
      .to.deep.eq([ 'foo', 'moo' ]);
  });
});
