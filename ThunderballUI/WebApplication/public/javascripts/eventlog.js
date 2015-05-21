// Connect to the nodeJs Server
var active = 1;
var socket = io();
socket.emit('getlogdata', { some: 'data' });

var usersList = "<table class=\"customers\"><tr class=\"header\"><th>Time</th><th>Desciption</th>";
var rowId = 0;

socket.on('new-data', function (data) {
    usersList = "<table class=\"customers\"><tr class=\"header\"><th>Time</th><th>Desciption</th>";
    var rowId = 0;
});

socket.on('new-log', function (data) {
    if (active != 1) {
        active = 1;
        socket.emit('getlogdata', { some: 'data' });
    }
});

socket.on('some-data', function (data) {

    for (var i = 0, ii = data.length; i < ii; i++) {
        usersList += "<tr>";
        var rowTag = "<td class=\"row\">";
        if (rowId & 1) {
            rowTag = "<td class=\"rowalt\">";
        }
        function parseDate(datestring) {
            var newdate = datestring.replace('T', '   at   ').replace(/-/g, '/').replace('Z', '').replace('.000', '');
            return newdate;
        }
        var datestring = data[i]["LogTime"];
        var date = parseDate(datestring);
        usersList += rowTag + date + "</td>";
        if (data[i]["PortId"] == "0")
            usersList += rowTag + "---" + "</td>";
        else
        usersList += rowTag + data[i]["Description"] + "</td>";
        usersList += "</tr>";
        rowId++;
    }

    // to display as you go
    //var finallist = usersList + "</table>";
    //document.getElementById("container").innerHTML = finallist;

});


socket.on('data-done', function (data) {
    var finallist = usersList + "</table>";
    document.getElementById("container").innerHTML = finallist;
    active = 0;
});
var data = 0;
function processLog(msg)
{
    socket.emit('log', { message: msg });
}
function deleteLog(){
		 socket.emit('deletelog');
}
socket.on("notifyunitdata", function (data) {
    var myTable = document.getElementById('statstable');
    var colIndex = 0;
    var splitCol = data.split(',');
    for (var rowIndex = 0; rowIndex < splitCol.length; rowIndex++) {
        myTable.rows[rowIndex + 1].cells[colIndex + 1].innerHTML = splitCol[rowIndex];
    }
});

function setFuelCellState(state) {
    if (state == 1) {
        socket.emit('fuelcellstate', { state: '1' });
    }
    else {
        socket.emit('fuelcellstate', { state: '0' });
    }
}
