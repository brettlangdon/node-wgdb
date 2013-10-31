var fs = require("fs");

var wgdb = require("./");
var db = new wgdb(1000, 2000000);

// yeah... this isn't a pretty example, but it'll make
// due until I write a better one
db.attach(function(error){
    console.dir(error);

    console.log("create record");
    console.log("=============");
    db.createRecord(2, function(error, record_one){
        console.dir(record_one.getLength());

        console.log("set field 0");
        console.log("=============");
        record_one.setField(0, "testing", function(error){
            console.dir(error);

            console.log("set field 1");
            console.log("=============");
            record_one.setField(1, 65.5, function(error){
                console.dir(error);

                console.log("get field 0");
                console.log("=============");
                record_one.getField(0, function(error, result){
                    console.dir(error);
                    console.dir(result);

                    console.log("dump db");
                    console.log("=============");
                    db.dump("test.db", function(error){
                        console.dir(error);

                        console.log("import db");
                        console.log("=============");
                        db.import("test.db", function(error){
                            console.dir(error);

                            console.log("get first record");
                            console.log("=============");
                            db.firstRecord(function(error, record){
                                console.dir(error);
                                console.dir(record);

                                console.log("get all fields");
                                console.log("=============");
                                record.getFields(function(error, fields){
                                    console.dir(error);
                                    console.dir(fields);

                                    console.log("detach");
                                    console.log("=============");
                                    db.detach(function(error){
                                        console.log(error);

                                        console.log("delete");
                                        console.log("=============");
                                        db.delete(function(error){
                                            console.log(error);

                                            console.log("unlink");
                                            console.log("=============");
                                            fs.unlinkSync("test.db");
                                        });
                                    });
                                });
                            });
                        });
                    });
                });
            });
        });
    });
});
