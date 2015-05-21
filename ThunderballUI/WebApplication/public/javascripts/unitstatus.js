// Connect to the nodeJs Server
var socket = io();

function processLog(msg)
{
    socket.emit('log', { message: msg });
}

socket.on("notifyunitdata", function (data) {
    var myTable = document.getElementById('statstable');
    var colIndex = 0;
    var splitCol = data.split(',');
    for (var rowIndex = 0; rowIndex < splitCol.length; rowIndex++) {
        myTable.rows[rowIndex + 1].cells[colIndex + 1].innerHTML = splitCol[rowIndex];
    }
});
