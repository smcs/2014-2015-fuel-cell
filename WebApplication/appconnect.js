var connectedToApp = false;
var connectedActive = false;
var net = require('net');
var client = new net.Socket();
client.setEncoding('utf8');

client.on("error", function (err) {
    console.log("Caught ocket error: ")
    console.log(err.stack);
    connectedActive = false;
});

client.on('connect', function () {
    connectedToApp = true;
    connectedActive = false;
    console.log('Connection opened');
});

client.on('close', function () {
    connectedToApp = false;
    connectedActive = false;
    console.log('Connection closed');
});

setInterval(function () {
    if (connectedToApp == false) {
        if (connectedActive == false) {
            try {
                connectedActive = true;
                console.log('Trying Connect');
                client.connect(3300, '127.0.0.1', function () {
                    // We have a connection - a socket object is assigned to the connection automatically
                    console.log('CONNECTED: ' + client.remoteAddress + ':' + client.remotePort);
                });
            }
            catch (e) {
                console.log('Catch Error in new socket: ' + e.message);
            }
        }
    }
}, 3000);

module.exports.appConnect = client;
