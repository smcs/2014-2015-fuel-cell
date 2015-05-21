var util = require('util');
var EventEmitter = require('events').EventEmitter;

function AppConnection(port) {
    var connectedToApp = false;
    var connectedActive = false;
    var net = require('net');
    var client = new net.Socket();
    client.setEncoding('utf8');
    var okToSend = false;
    var chunk = "";

    this.port = port;
    this.client = client;

    // we need to store the reference of `this` to `self`, so that we can use the current context in the setTimeout (or any callback) functions
    // using `this` in the setTimeout functions will refer to those funtions, not the Radio class
    var self = this;

    client.on("error", function (err) {
        console.log("Caught ocket error: ")
        console.log(err.stack);
        connectedActive = false;
        okToSend = false;
    });

    client.on('connect', function () {
        connectedToApp = true;
        connectedActive = false;
        console.log('Connection opened');
    });

    client.on('close', function () {
        connectedToApp = false;
        connectedActive = false;
        okToSend = false;
        console.log('Connection closed');
        client.destroy();
    });

    client.on('data', function (data) {
        chunk += data.toString(); // Add string on the end of the variable 'chunk'
        d_index = chunk.indexOf(';'); // Find the delimiter

        // While loop to keep going until no delimiter can be found
        while (d_index > -1) {         
            {
                string = chunk.substring(0,d_index); // Create string up until the delimiter
                var splitData = string.split('|');
                self.emit(splitData[0], splitData[1]);

                //json = JSON.parse(string); // Parse the current string
                //process(json); // Function that does something with the current chunk of valid json.        
            }
            chunk = chunk.substring(d_index+1); // Cuts off the processed chunk
            d_index = chunk.indexOf(';'); // Find the new delimiter
        }      
    });

    setInterval(function () {
        if (connectedToApp == false) {
            if (connectedActive == false) {
                try {
                    connectedActive = true;
                    console.log('Trying Connect');
                    client.connect(port, '127.0.0.1', function () {
                        // We have a connection - a socket object is assigned to the connection automatically
                        console.log('CONNECTED: ' + client.remoteAddress + ':' + client.remotePort + ':' + client.localPort);
                        okToSend = true;
                    });
                    console.log('...Done Connect');
                }
                catch (e) {
                    console.log('Catch Error in new socket: ' + e.message);
                }
            }
        }
    }, 3000);

    this.write = function (msg) {
        if (okToSend == true) {
            console.log('Sending:', msg);
            this.client.write(msg);
        }
    };
}

// MUST BE BEFORE WRITE FUNCTION BELOW??
// extend the EventEmitter class using our Radio class
util.inherits(AppConnection, EventEmitter);

module.exports = AppConnection;
