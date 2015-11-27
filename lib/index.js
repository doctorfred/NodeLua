var nodelua = null;

try{
    nodelua = require('../build/Release/nodelua');
}catch(e){
	console.dir(e);
    nodelua = require('../build/Debug/nodelua');
}

module.exports = nodelua;