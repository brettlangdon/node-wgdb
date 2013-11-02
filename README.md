node-wgdb
=========

Bindings for [WhiteDB](http://whitedb.org/) for [Node.JS](http://nodejs.org/).

This library is still a work in progress and might not work for everyone.
Please report any and all issues.

## Installing
You need to first install the [WhiteDB](http://whitedb.org/) library from their [download page](http://whitedb.org/download.html). The version developed against is `0.7.0`.

### Via Git
```bash
git clone git://github.com/brettlangdon/node-wgdb.git
cd ./node-wgdb
npm install
```

### Via NPM
```bash
npm install wgdb
```

## API

### wgdb
When requiring the module you will end up with the `wgdb` class which is how you
interact with WhiteDB

#### CONSTANTS
* `wgdb.EQUAL` - used to represent `=` condition for queries
* `wgdb.NOT_EQUAL` - used to represent `!=` condition for queries
* `wgdb.LESSTHAN` - used to represent `<` condition for queries
* `wgdb.GREATER` - used to represent `>` condition for queries
* `wgdb.LTEQUAL` - used to represent `<=` condition for queries
* `wgdb.GTEQUAL` - used to represent `>=` condition for queries

#### `new wgdb(db_name, [size])`
Creates a new instance of `wgdb`.
* `db_name` - integer name of shared memory space (required)
* `size` - the size in bytes for the databae if it does not already exist (default: 10000000 - 10MB)

```javascript
var wgdb = require("wgdb");

var db = new wgdb(1000, 2000000);
```

#### `wgdb.attach([callback])`
Either attaches to or creates the shared space for the `wgdb` instance.

Calling `attach` is very important, this makes it so you can use the other methods.

* `callback` - `function(error)` to call when done attaching, `error` will be `undefined` if no error occurred.

```javascript
var wgdb = require("wgdb");

var db = new wgdb(1000, 2000000);
db.attach(function(error){
    if(error){
       console.log(error);
    }

    // do some cool stuff
});
```

#### `wgdb.detach([callback])`
Detaches an attached `wgdb` instance.

* `callback` - `function(error)` to call when done detaching, `error` will be `undefined` if no error occurred.

```javascript
var wgdb = require("wgdb");

var db = new wgdb(1000, 2000000);
db.attach(function(error){
    if(!error){
        // do some cool stuff
        db.detach(console.log);
    }
});
```

#### `wgdb.delete([callback])`
Delete the shared memory space for the `wgdb` instance.

* `callback` - `function(error)` to call when done deleting, `error` will be `undefined` if no error occurred.

```javascript
var wgdb = require("wgdb");

var db = new wgdb(1000, 2000000);
db.attach(function(error){
    if(!error){
        db.delete(console.log);
    }
});
```

#### `wgdb.dump(filename, [callback])`
Dump the `wgdb` database to `filename`.

* `filename` - string filename of where to dump the database (required)
* `callback` - `function(error)` to call when done dumping, `error` will be `undefined` if no error occurred.

```javascript
var wgdb = require("wgdb");

var db = new wgdb(1000, 2000000);
db.attach(function(error){
    if(!error){
        db.dump("backup.db", console.log);
    }
});
```

#### `wgdb.import(filename, [callback])`
Import `filename` into the `wgdb` database.

* `filename` - string filename of where to load the database from (required)
* `callback` - `function(error)` to call when done importing, `error` will be `undefined` if no error occurred.

```javascript
var wgdb = require("wgdb");

var db = new wgdb(1000, 2000000);
db.attach(function(error){
    if(!error){
        db.import("backup.db", console.log);
    }
});
```

#### `wgdb.createRecord(num_fields, [callback])`
Add a new record to the `wgdb` instance.

* `num_fields` - int number of fields for the record to have (required)
* `callback` - `function(error, record)` to call when done creating, `error` will be `undefined` if no error occurred, `record` will be `undefined` if an error occurred or else will be a `record` instance.

```javascript
var wgdb = require("wgdb");

var db = new wgdb(1000, 2000000);
db.attach(function(error){
    if(!error){
        db.createRecord(2, function(error, record){
            if(error || !record){
                console.log(error);
                return;
            }

            // do some cool record stuff here
        });
    }
});
```

#### `wgdb.firstRecord([callback])`
Get the first record from the `wgdb` instance.

* `callback` - `function(error, record)` to call when done creating, `error` will be `undefined` if no error occurred, `record` will be `undefined` if an error occurred or else will be a `record` instance.

```javascript
var wgdb = require("wgdb");

var db = new wgdb(1000, 2000000);
db.attach(function(error){
    if(!error){
        db.firstRecord(function(error, record){
            if(error || !record){
                console.log(error);
                return;
            }

            // do some cool record stuff here
        });
    }
});
```

#### `wgdb.findRecord(field, condition, value [last_record], [callback])`
Find the first record in the `wgdb` database which meets the `condition` and `value` requirements.

If `last_record` is given, then find the first record after `last_record` which meets the conditions.

* `field` - int of the field to search by (required)
* `condition` - one of the `wgdb` condition constants,  e.g. `wgdb.EQUAL` (required)
* `value` - the value to match `condition` on (required)
* `last_record` - a `record` instance, find the next record after `last_record` which matches `condition` and `value` (optional)
* `callback` - `function(error, record)` to call when done searching, `error` will be `undefined` if no error occurred, `record` will be `undefined` if an error occurred or else will be a `record` instance.

```javascript
var wgdb = require("wgdb");

var db = new wgdb(1000, 2000000);
db.attach(function(error){
    if(!error){
        // find the first record where field 1 = 5
        db.findRecord(1, wgdb.EQUAL, 5, function(error, first){
            if(error || !first){
                console.log(error);
                return;
            }

            // find the next record after `first` where field 1 == 5
            db.findRecord(1, wgdb.EQUAL, 5, first, function(error, second){
                // do cool stuff with `second`
            });
        });
    }
});
```

#### `wgdb.query(query, [callback])`
Query the `wgdb` instance for all records which meet the provided `query`.

`query` is similar to a list of parameters to `wgdb.findRecord`.

```javascript
[
    [<field_num>, <condition>, <value>],
    ...
]
```

* `query` - array representing the query to make (required)
* `callback` - `function(error, cursor)` to call when done searching, `error` will be `undefined` if no error occurred, `cursor` will be `undefined` if an error occurred or else will be a `cursor` instance.

```javascript
var wgdb = require("wgdb");

var db = new wgdb(1000, 2000000);
db.attach(function(error){
    if(!error){
        db.query([
            [1, wgdb.GTEQUAL, 5],
        ], function(error, cursor){
            if(error || !cursor){
                console.log(error);
                return;
            }

            cursor.next(function(error, record){
                // do someting with record
                cursor.next(function(error, record){
                    // do something with record
                });
            });
        });
    }
});
```


## License
```
The MIT License (MIT)

Copyright (c) 2013 Brett Langdon <brett@blangdon.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
