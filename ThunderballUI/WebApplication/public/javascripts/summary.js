// Connect to the nodeJs Server
var socket = io();
<!--var data = 0;
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
} -->