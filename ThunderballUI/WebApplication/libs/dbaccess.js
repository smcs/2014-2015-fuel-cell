var mysql = require('mysql');

/////////////////////////////////////////////////////
// Data base interface

// data base connection stuff
var sample_db_config = {
    host: '127.0.0.1',
    user: 'webuser',
    password: 'webuser',
    database: 'Thunderball'
};

// General query function
exports.GetDataFromDatabase = function (db_config, query, socket, newtag, rowtag, donetag, errortag) {
    databaseConnection = mysql.createConnection(db_config); // Recreate the connection, since the old one cannot be reused.
    databaseConnection.connect(function (err) {
        if (err) {
            // The server is either down or restarting (takes a while sometimes).
            console.log('error when connecting to db:', err);
            socket.emit(errortag, 'Error when connecting to db: ' + err);
        }
        else {
            socket.emit(newtag, 'NEW');
            databaseConnection.query(query)
                .on('result', function (data) {
                    socket.emit(rowtag, [data]);
                })
                .on('end', function () {
                    console.log("Query Done");
                    socket.emit(donetag, 'DONE');
                    databaseConnection.end();
                })
        }
    });
}

exports.UpdateDatabase = function (db_config, query) {
    console.log('Updating DB with:', query);
    databaseConnection = mysql.createConnection(db_config); // Recreate the connection, since the old one cannot be reused.
    databaseConnection.connect(function (err) {
        if (err) {
            // The server is either down or restarting (takes a while sometimes).
            console.log('error when connecting to db:', err);
            socket.emit(errortag, 'Error when connecting to db: ' + err);
        }
        else {
            databaseConnection.query(query)
                .on('result', function (data) {
                    console.log("Update Results", data);
                })
                .on('end', function () {
                    console.log("Update Done");
                    databaseConnection.end();
                })
        }
    });
}
