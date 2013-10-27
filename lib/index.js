try{
    return module.exports = require("../build/Release/wgdb").wgdb;
}catch(e){
    return module.exports = require("../build/Debug/wgdb").wgdb;
}
