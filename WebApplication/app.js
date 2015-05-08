console.log('App.js starting');

var express = require('express');
var routes = require('./routes');
var http = require('http');
var path = require('path');
var fs = require('fs');
var express = require('express');
var morgan = require('morgan');
var mysql = require('mysql');

var db_config = {
    host: '127.0.0.1',
    user: 'matt',
    password: 'matt',
    database: 'MattData'
};

var databaseConnection;

function handleDisconnect() {
    databaseConnection = mysql.createConnection(db_config); // Recreate the connection, since the old one cannot be reused.

    databaseConnection.connect(function (err) {
        if (err)                                // The server is either down or restarting (takes a while sometimes).
        {
            console.log('error when connecting to db:', err);
            setTimeout(handleDisconnect, 2000);     // We introduce a delay before attempting to reconnect,
        }                                           // to avoid a hot loop, and to allow our node script to
        else
            console.log('Database connection made');
    });                                             // process asynchronous requests in the meantime.

    // If you're also serving http, display a 503 error.
    databaseConnection.on('error', function (err) {
        console.log('db error: ', err);
        if (err.code === 'PROTOCOL_CONNECTION_LOST') {
            handleDisconnect();                         // Connection to the MySQL server is usually lost due to either server restart, or a
        }
        else {
            throw err;                                  // connnection idle timeout (the wait_timeout server variable configures this)
        }
    });
}

handleDisconnect();

var pollingLoop = function (socket) {

    try {
        // Make the database query
        var query = databaseConnection.query('SELECT * FROM TheLog ORDER BY Id DESC LIMIT 20'),
            users = []; // this array will contain the result of our db query

        // set up the query listeners
        query
        .on('error', function (err) {
            // Handle error, and 'end' event will be emitted after this as well
            console.log('error in .on error');
            console.log(err);
            updateSockets(err);

        })
        .on('result', function (user) {
            // it fills our array looping on each user row inside the db
            users.push(user);
        })
        .on('end', function () {
            console.log("Query Done");
            socket.volatile.emit('notifylogdata', users);
        });
    }
    catch (e) {
        console.log('Catch error in pollingLoop' + e.message);
    }

};

var app = express();

// all environments
app.set('port', process.env.PORT || 3000);
app.set('views', path.join(__dirname, 'views'));
app.set('view engine', 'jade');
app.use(express.static(__dirname + '/public')); 	// set the static files location /public/img will be /img for users
app.use(morgan('dev')); 					// log every request to the console

// development only
//var env = process.env.NODE_ENV || 'development';
//if ('development' == env) {
    // configure stuff here
//    app.use(errorhandler);
//}

var server = http.createServer(app).listen(app.get('port'), function(){
  console.log('Express server listening on port ' + app.get('port'));
});

var io = require('socket.io').listen(server);
var strPiece = "";

app.get('/', routes.index);
app.get('/eventlog', routes.eventlog());

app.get('/action', function (req, res) {
    res.render('eventlog', { title: 'Matts Log' });
});

// connection back to fuel cell app
var client = require('./appconnect.js');

// A user connects to the server (opens a socket)
io.sockets.on('connection', function (socket)
{
    // The server recieves a send power schema message
    // from the browser on this socket
    socket.on('ping', function (data)
    {
        console.log('socket: server recieves request from browser');
        socket.join('Us');
    });

    socket.on('disconnect', function (data) {
        console.log('socket: server recieves disconnect from browser');
        socket.leave('Us');
    });

    socket.on('log', function (data) {
        console.log('logging: ' + data.message);
        client.appConnect.write(data.message);
    });

    socket.on('AppMessage', function (data) {
        console.log('socket: server receives AppMessage', data.message);
        client.write(data.message);
    });

    socket.on('AppDebug', function (data) {
        console.log('AppDebug: ', data.message);
    });

    // call from browser to read database
    socket.on('geteventlog', function (data) {
        console.log('socket: server receives geteventlog');
        pollingLoop(socket);
    });

    // data from fuel cell app
    client.appConnect.on('data', function (data) {
        var i;
        for (i = 0; i < data.length; i++) 
        {
            var ch = data.charAt(i);
            if (ch == '[')
                strPiece = "";
            else if (ch == ']') {
                console.log('Server data:', strPiece);
                socket.volatile.emit('somedata', strPiece);
                strPiece = "";
            }
            else
                strPiece += ch;
        }
    });
});

