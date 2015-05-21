console.log('App.js starting');

var path = require('path');
var fs = require('fs');
var morgan = require('morgan');
var express = require('express');
var appcon = require('./libs/appconnect');

var app = express();
var http = require('http').Server(app);
var io = require('socket.io')(http);

app.set('views', path.join(__dirname, 'views'));
app.set('view engine', 'jade');
app.use(express.static(__dirname + '/public')); 	// set the static files location /public/img will be /img for users
app.use(morgan('dev')); 					// log every request to the console

//////////////////////////////////////////////////////////
// Main we server interface
// Note: port is 3000
http.listen(3000, function () {
    console.log('listening on *:3000');
});

/////////////////////////////////////////////////////
// data base connection stuff
var db_config = {
    host: '127.0.0.1',
    user: 'webuser',
    password: 'webuser',
    database: 'Thunderball'
};

////////////////////////////////////////////////////
// library of MySql access commands
var dbaccess = require('./libs/dbaccess.js');


var routes = require('./routes/routes.js');
////////////////////////////////////////////////////////////////
// routes to jade pages go here
// the details are in ./routes/routes.js
app.get('/jadeindex', routes.jadeindex);
app.get('/summary', routes.summary);
app.get('/log', routes.eventlog);
app.get('/stats', routes.statsview);
// new one here

// dummy file download (to client) example
app.get('/download', function (req, res) {
    var file = __dirname + '/public/images/Kitty.jpeg';
    res.download(file); // Set disposition and send it.
});

//////////////////////////////////////////////
// connections to fuel cell here
// it is accepting connections on port 3300

var client = new appcon(3300);
client.on('stats', function (data) {
    //console.log('FC Data::', data);

    // send on to all connected clients
    io.sockets.emit('notifynewfcdata', data);
});

//////////////////////////////////////////////////////////////////////////
// handle connections from browser here
var sockets = [];

io.on('connection', function (socket) {

    console.log('a user connected');
    sockets.push(socket);

    socket.on('disconnect', function () {
        console.log('user disconnected');
        sockets.splice(sockets.indexOf(socket), 1);
    });

    // Get log entries
    socket.on('getlogdata', function (data) {
        console.log('Sending log data to browser');

        // This call send a DB query and posts the data to the Web client using specified tags
        dbaccess.GetDataFromDatabase(db_config, 'SELECT * FROM TheLog', socket, 'new-data', 'some-data', 'data-done', 'db-err');
    });

    // get statictics
    socket.on('getstatsdata', function (data) {
        console.log('Sending stats data to browser');

        // This call send a DB query and posts the data to the Web client using specified tags
        dbaccess.GetDataFromDatabase(db_config, 'SELECT * FROM RealData', socket, 'new-data', 'some-data', 'data-done', 'db-err');
    });

    ///////////////////////////////////
    // button messages

    // Add log entry 
    socket.on('deletelog', function (data){
        dbaccess.UpdateDatabase(db_config, "delete from TheLog");
        //socket.emit('new-log');
    });
    socket.on('log', function (data) {
        console.log("Received log msg", data);

        // using javascript DB access
        //dbaccess.UpdateDatabase(db_config, "Insert INTO TheLog (MainId,SubId,Description) VALUES (1,1,'" + data["message"] + "')");

        // using fuel cell log services - these messages are processed in ThunderballDataInterface.cpp
        // The format is "command:arguments"
        client.write("log:" + data["message"] + "\n");
    });

    // Set fuel cell state
    socket.on('fuelcellstate', function (data) {
        console.log("Received  setfuelcellstate", data);

        // send command to fuel cell - these messages are processed in ThunderballDataInterface.cpp
        // The format is "command:arguments"
        //  These commands have no arguments
        if (data["state"] == '1')
            client.write("start:\n");
        else
            client.write("stop:\n");
    });

    ////////////////////////////////////////////////////////////////////////////////
    // File signal checking
    // The fuel cell will change a file in the system to tell this application
    //    that something has changed. The files are specifically named.
    //    In other words the fuel cell will updaate 'WebApplication/logchange.signal'

    fs.watch(__dirname + '/logchange.signal', function (curr, prev) {
        // force a 'pull' from the client
        socket.emit('new-log');
    });

    // other file signals maybe

});
