// Connect to the nodeJs Server
io = io.connect('/');

console.log("socket: browser says geteventlog")
io.emit('geteventlog', { some: 'data' });

//window.onunload() = closeConnect;
//
//function closeConnect() {
   // io.closeConnect();
//}

io.on('notifylogdata', function (data) {
    var usersList = "<table class=\"customers\"><tr class=\"header\"><th>Time</th><th>Unit</th><th>Port</th><th>Desciption</th>";

    for (var i = 0, ii = data.length; i < ii; i++) {
        usersList += "<tr>";
        var rowTag = "<td class=\"row\">";
        if (i & 1) {
            rowTag = "<td class=\"rowalt\">";
        }
        usersList += rowTag + data[i]["LogTime"] + "</td>";
        usersList += rowTag + data[i]["UnitId"] + "</td>";
        if (data[i]["PortId"] == "0")
            usersList += rowTag + "---" + "</td>";
        else
            usersList += rowTag + data[i]["PortId"] + "</td>";
        usersList += rowTag + data[i]["Description"] + "</td>";
        usersList += "</tr>";
    }
    usersList += "</table>";
    console.log("^^^^^^^^^^^^^^^^^^^^Data", usersList)
    document.getElementById("container").innerHTML = usersList;
});
