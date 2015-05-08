// Connect to the nodeJs Server
io = io.connect('/');
console.log("socket: browser connects")
io.emit('ping', { some: 'data' });

function processLog(msg)
{
    io.emit('log', { message: msg });
}

io.on("somedata", function (data) {
    var myTable = document.getElementById('statstable');
    var colIndex = 0;
    var splitCol = data.split(',');
    for (var rowIndex = 0; rowIndex < splitCol.length; rowIndex++) {
        myTable.rows[rowIndex + 1].cells[colIndex + 1].innerHTML = splitCol[rowIndex];
    }
});
